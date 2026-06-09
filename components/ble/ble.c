/* See ble/ble.h. Meshtastic BLE companion service. */
#include "ble/ble.h"
#include "net/backend.h"
#include "net/message.h"
#include "core/nodedb.h"
#include "mesh/meshtastic_pb.h"
#include "pb_encode.h"
#include "pb_decode.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "host/ble_att.h"

static const char *TAG = "ble";

/* Meshtastic service + characteristic UUIDs (bytes are LSB-first for NimBLE). */
static const ble_uuid128_t SVC_UUID =
    BLE_UUID128_INIT(0xfd, 0xea, 0x73, 0xe2, 0xca, 0x5d, 0xa8, 0x9f, 0x1f, 0x46, 0xa8, 0x15, 0x18, 0xb2, 0xa1, 0x6b);
static const ble_uuid128_t TORADIO_UUID =
    BLE_UUID128_INIT(0xe7, 0x01, 0x44, 0x12, 0x66, 0x78, 0xdd, 0xa1, 0xad, 0x4d, 0x9e, 0x12, 0xd2, 0x76, 0x5c, 0xf7);
static const ble_uuid128_t FROMRADIO_UUID =
    BLE_UUID128_INIT(0x02, 0x00, 0x12, 0xac, 0x42, 0x02, 0x78, 0xb8, 0xed, 0x11, 0x93, 0x49, 0x9e, 0xe6, 0x55, 0x2c);
static const ble_uuid128_t FROMNUM_UUID =
    BLE_UUID128_INIT(0x53, 0x44, 0xe3, 0x47, 0x75, 0xaa, 0x70, 0xa6, 0x66, 0x4f, 0x00, 0xa8, 0x8c, 0xa1, 0x9d, 0xed);

#define FIFO_CAP 16
#define FRAME_MAX 400

static struct { uint8_t buf[FRAME_MAX]; uint16_t len; } s_fifo[FIFO_CAP];
static int s_head, s_tail, s_n;
static SemaphoreHandle_t s_lock;
static uint32_t s_fromnum;
static uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_fromnum_handle;
static settings_t *s_reg;
static char s_name[24];
static uint8_t s_own_addr_type;
static struct ble_npl_event s_notify_ev;

static void start_advertising(void);

/* Send the FromNum notify. Runs on the NimBLE host task (posted from fifo_push)
 * so NimBLE host APIs are never touched from the radio task. */
static void notify_cb(struct ble_npl_event *ev)
{
    (void)ev;
    if (s_conn == BLE_HS_CONN_HANDLE_NONE || !s_fromnum_handle) return;
    struct os_mbuf *om = ble_hs_mbuf_from_flat(&s_fromnum, sizeof s_fromnum);
    if (om) ble_gatts_notify_custom(s_conn, s_fromnum_handle, om);
}

/* --- FromRadio queue ---------------------------------------------------- */
static void fifo_push(const uint8_t *data, int len)
{
    if (len <= 0 || len > FRAME_MAX) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_n < FIFO_CAP) {
        memcpy(s_fifo[s_tail].buf, data, len);
        s_fifo[s_tail].len = (uint16_t)len;
        s_tail = (s_tail + 1) % FIFO_CAP;
        s_n++;
    }
    s_fromnum++;
    xSemaphoreGive(s_lock);

    /* fifo_push runs on the radio task. Hand the GATT notify to the NimBLE host
     * task via its event queue, which is safe to post to from any task. */
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &s_notify_ev);
}

static int fifo_pop(uint8_t *out)
{
    int len = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_n > 0) {
        len = s_fifo[s_head].len;
        memcpy(out, s_fifo[s_head].buf, len);
        s_head = (s_head + 1) % FIFO_CAP;
        s_n--;
    }
    xSemaphoreGive(s_lock);
    return len;
}

static void enqueue_fromradio(const meshtastic_FromRadio *fr)
{
    uint8_t buf[FRAME_MAX];
    pb_ostream_t s = pb_ostream_from_buffer(buf, sizeof buf);
    if (pb_encode(&s, meshtastic_FromRadio_fields, fr)) fifo_push(buf, (int)s.bytes_written);
}

/* --- config handshake --------------------------------------------------- */
static void fill_self_user(meshtastic_NodeInfo *ni)
{
    ni->num = net_my_node();
    ni->has_user = true;
    net_node_id_str(net_my_node(), ni->user.id);
    char nm[48] = "", sn[16] = "";
    if (s_reg) {
        settings_get_str(s_reg, "device_name", nm, sizeof nm);
        settings_get_str(s_reg, "short_name", sn, sizeof sn);
    }
    if (!nm[0]) net_node_id_str(net_my_node(), nm);
    snprintf(ni->user.long_name, sizeof ni->user.long_name, "%s", nm);
    snprintf(ni->user.short_name, sizeof ni->user.short_name, "%s", sn[0] ? sn : nm);
}

static void do_handshake(uint32_t config_id)
{
    meshtastic_FromRadio fr;

    fr = (meshtastic_FromRadio)meshtastic_FromRadio_init_zero;
    fr.which_payload_variant = meshtastic_FromRadio_my_info_tag;
    fr.payload_variant.my_info.my_node_num = net_my_node();
    enqueue_fromradio(&fr);

    /* self */
    fr = (meshtastic_FromRadio)meshtastic_FromRadio_init_zero;
    fr.which_payload_variant = meshtastic_FromRadio_node_info_tag;
    fill_self_user(&fr.payload_variant.node_info);
    enqueue_fromradio(&fr);

    /* known nodes */
    nodedb_t *db = net_nodedb();
    for (int i = 0; i < db->count; i++) {
        node_record_t *r = &db->nodes[i];
        fr = (meshtastic_FromRadio)meshtastic_FromRadio_init_zero;
        fr.which_payload_variant = meshtastic_FromRadio_node_info_tag;
        meshtastic_NodeInfo *ni = &fr.payload_variant.node_info;
        ni->num = r->num;
        ni->snr = r->snr;
        ni->last_heard = r->last_heard;
        ni->has_user = true;
        net_node_id_str(r->num, ni->user.id);
        snprintf(ni->user.long_name, sizeof ni->user.long_name, "%s",
                 r->long_name[0] ? r->long_name : ni->user.id);
        snprintf(ni->user.short_name, sizeof ni->user.short_name, "%s", r->short_name);
        if (r->has_position) {
            ni->has_position = true;
            ni->position.latitude_i = r->lat_i;
            ni->position.longitude_i = r->lon_i;
            ni->position.altitude = r->alt;
        }
        if (r->has_telemetry) {
            ni->has_device_metrics = true;
            ni->device_metrics.battery_level = r->battery;
            ni->device_metrics.voltage = r->voltage;
        }
        enqueue_fromradio(&fr);
    }

    /* primary channel */
    fr = (meshtastic_FromRadio)meshtastic_FromRadio_init_zero;
    fr.which_payload_variant = meshtastic_FromRadio_channel_tag;
    fr.payload_variant.channel.index = 0;
    fr.payload_variant.channel.role = 1; /* PRIMARY */
    fr.payload_variant.channel.has_settings = true;
    if (s_reg) settings_get_str(s_reg, "mesh_chan", fr.payload_variant.channel.settings.name,
                                sizeof fr.payload_variant.channel.settings.name);
    enqueue_fromradio(&fr);

    /* done */
    fr = (meshtastic_FromRadio)meshtastic_FromRadio_init_zero;
    fr.which_payload_variant = meshtastic_FromRadio_config_complete_id_tag;
    fr.payload_variant.config_complete_id = config_id;
    enqueue_fromradio(&fr);
    ESP_LOGI(TAG, "config handshake queued (id %lu, %d nodes)", (unsigned long)config_id, db->count);
}

static void handle_toradio(const uint8_t *data, int len)
{
    meshtastic_ToRadio tr = meshtastic_ToRadio_init_zero;
    pb_istream_t s = pb_istream_from_buffer(data, len);
    if (!pb_decode(&s, meshtastic_ToRadio_fields, &tr)) return;

    if (tr.which_payload_variant == meshtastic_ToRadio_want_config_id_tag) {
        do_handshake(tr.payload_variant.want_config_id);
    } else if (tr.which_payload_variant == meshtastic_ToRadio_packet_tag) {
        meshtastic_MeshPacket *p = &tr.payload_variant.packet;
        if (p->which_payload_variant == meshtastic_MeshPacket_decoded_tag) {
            uint8_t dbuf[256];
            int dl = mt_data_encode(dbuf, sizeof dbuf, &p->payload_variant.decoded);
            if (dl > 0) net_send_meshpacket(p->to ? p->to : 0xFFFFFFFFu, p->want_ack, dbuf, dl);
        }
    }
}

/* --- LoRa -> phone ------------------------------------------------------ */
static void on_rx(uint32_t from, uint32_t to, uint32_t id, const uint8_t *data, int data_len,
                  float rssi, float snr)
{
    meshtastic_Data d;
    if (!mt_data_decode(data, data_len, &d)) return;
    meshtastic_FromRadio fr = meshtastic_FromRadio_init_zero;
    fr.which_payload_variant = meshtastic_FromRadio_packet_tag;
    meshtastic_MeshPacket *p = &fr.payload_variant.packet;
    p->from = from;
    p->to = to;
    p->id = id;
    p->rx_rssi = (int32_t)rssi;
    p->rx_snr = snr;
    p->which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    p->payload_variant.decoded = d;
    enqueue_fromradio(&fr);
}

/* --- GATT access -------------------------------------------------------- */
static int toradio_access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn; (void)attr; (void)arg;
    uint8_t buf[512];
    uint16_t len = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof buf, &len) == 0) handle_toradio(buf, len);
    return 0;
}

static int fromradio_access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn; (void)attr; (void)arg;
    uint8_t buf[FRAME_MAX];
    int len = fifo_pop(buf);
    if (len > 0) os_mbuf_append(ctxt->om, buf, len);
    return 0;
}

static int fromnum_access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn; (void)attr; (void)arg;
    os_mbuf_append(ctxt->om, &s_fromnum, sizeof s_fromnum);
    return 0;
}

static const struct ble_gatt_svc_def GATT_SVCS[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SVC_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            { .uuid = &TORADIO_UUID.u, .access_cb = toradio_access, .flags = BLE_GATT_CHR_F_WRITE },
            { .uuid = &FROMRADIO_UUID.u, .access_cb = fromradio_access, .flags = BLE_GATT_CHR_F_READ },
            { .uuid = &FROMNUM_UUID.u, .access_cb = fromnum_access,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY, .val_handle = &s_fromnum_handle },
            { 0 },
        },
    },
    { 0 },
};

/* --- GAP ---------------------------------------------------------------- */
static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) s_conn = event->connect.conn_handle;
        else start_advertising();
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        s_conn = BLE_HS_CONN_HANDLE_NONE;
        start_advertising();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_advertising();
        break;
    default:
        break;
    }
    return 0;
}

static void start_advertising(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t *)&SVC_UUID;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    struct ble_hs_adv_fields rsp = {0};
    rsp.name = (uint8_t *)s_name;
    rsp.name_len = strlen(s_name);
    rsp.name_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&rsp);

    struct ble_gap_adv_params adv = {0};
    adv.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv.disc_mode = BLE_GAP_DISC_MODE_GEN;
    int rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv, gap_event, NULL);
    if (rc && rc != BLE_HS_EALREADY && rc != BLE_HS_EBUSY)
        ESP_LOGW(TAG, "adv start failed: %d", rc);
}

static void on_sync(void)
{
    ble_hs_id_infer_auto(0, &s_own_addr_type);
    start_advertising();
    ESP_LOGI(TAG, "advertising as '%s'", s_name);
}

static void host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static const setting_t BLE_SCHEMA[] = {
    {.key = "ble_enabled", .type = SET_BOOL, .def = "false", .label = "Bluetooth (Meshtastic app)", .group = "Bluetooth"},
};

void ble_init(settings_t *reg, const char *device_short_name)
{
    s_reg = reg;
    settings_register_many(reg, BLE_SCHEMA, 1);
    if (!settings_get_bool(reg, "ble_enabled")) { ESP_LOGI(TAG, "BLE off"); return; }

    snprintf(s_name, sizeof s_name, "%s", device_short_name && device_short_name[0] ? device_short_name : "Communicator");
    s_lock = xSemaphoreCreateMutex();

    if (nimble_port_init() != ESP_OK) { ESP_LOGE(TAG, "nimble init failed"); return; }
    ble_svc_gap_init();
    ble_svc_gatt_init();
    int rc;
    if ((rc = ble_gatts_count_cfg(GATT_SVCS))) { ESP_LOGE(TAG, "gatts count_cfg %d", rc); return; }
    if ((rc = ble_gatts_add_svcs(GATT_SVCS))) { ESP_LOGE(TAG, "gatts add_svcs %d", rc); return; }
    ble_svc_gap_device_name_set(s_name);
    ble_att_set_preferred_mtu(247);            /* let the phone send full frames */
    ble_hs_cfg.sync_cb = on_sync;

    /* The default event queue exists now; arm the host-task notify and only then
     * let the radio task start pushing received frames at us. */
    ble_npl_event_init(&s_notify_ev, notify_cb, NULL);
    net_set_rx_observer(on_rx);

    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "BLE Meshtastic companion starting");
}
