/* See drivers/radio.h. SX1262 over SPI; sequences ported from the badge's
 * MicroPython SX126x driver (net/sx126x.py, sx1262.py, lora.py). */
#include "drivers/radio.h"
#include "board_pins.h"

#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

static const char *TAG = "sx1262";

/* --- SX126x command opcodes --------------------------------------------- */
#define CMD_SET_SLEEP            0x84
#define CMD_SET_STANDBY          0x80
#define CMD_SET_FS               0xC1
#define CMD_SET_TX               0x83
#define CMD_SET_RX               0x82
#define CMD_SET_CAD              0xC5
#define CMD_SET_RF_FREQUENCY     0x86
#define CMD_SET_PACKET_TYPE      0x8A
#define CMD_SET_TX_PARAMS        0x8E
#define CMD_SET_PA_CONFIG        0x95
#define CMD_SET_BUFFER_BASE      0x8F
#define CMD_SET_MODULATION       0x8B
#define CMD_SET_PACKET_PARAMS    0x8C
#define CMD_SET_CAD_PARAMS       0x88
#define CMD_SET_DIO_IRQ_PARAMS   0x08
#define CMD_GET_IRQ_STATUS       0x12
#define CMD_CLEAR_IRQ_STATUS     0x02
#define CMD_SET_DIO2_RF_SWITCH   0x9D
#define CMD_SET_DIO3_TCXO        0x97
#define CMD_SET_REGULATOR_MODE   0x96
#define CMD_CALIBRATE            0x89
#define CMD_CALIBRATE_IMAGE      0x98
#define CMD_WRITE_REGISTER       0x0D
#define CMD_READ_REGISTER        0x1D
#define CMD_WRITE_BUFFER         0x0E
#define CMD_READ_BUFFER          0x1E
#define CMD_GET_RX_BUFFER_STATUS 0x13
#define CMD_GET_PACKET_STATUS    0x14

/* IRQ bits */
#define IRQ_TX_DONE      0x0001
#define IRQ_RX_DONE      0x0002
#define IRQ_HEADER_ERR   0x0020
#define IRQ_CRC_ERR      0x0040
#define IRQ_CAD_DONE     0x0080
#define IRQ_CAD_DETECTED 0x0100
#define IRQ_TIMEOUT      0x0200
#define IRQ_ALL          0x03FF

/* Registers */
#define REG_LORA_SYNC_WORD_MSB 0x0740
#define REG_OCP                0x08E7
#define REG_TX_CLAMP           0x08D8
#define REG_SENSITIVITY        0x0889

#define PACKET_TYPE_LORA  0x01
#define STANDBY_RC        0x00
#define HEADER_EXPLICIT   0x00
#define CRC_ON            0x01
#define IQ_STANDARD       0x00
#define LDRO_ON           0x01
#define LDRO_OFF          0x00
#define RX_CONTINUOUS     0xFFFFFF

#define XTAL_HZ           32000000.0
#define FREQ_DIV          33554432.0   /* 2^25 */
#define MAX_PKT           255

static spi_device_handle_t s_spi;
static SemaphoreHandle_t s_dio1;
static radio_params_t s_p;

/* --- low-level SPI ------------------------------------------------------- */
static void wait_busy(void)
{
    int guard = 0;
    while (gpio_get_level(PIN_RF_BUSY)) {
        esp_rom_delay_us(10);
        if (++guard > 10000) break; /* ~100ms safety */
    }
}

static void rf_xfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    wait_busy();
    spi_transaction_t t = { .length = len * 8, .tx_buffer = tx, .rx_buffer = rx };
    spi_device_polling_transmit(s_spi, &t);
}

static void cmd(uint8_t op, const uint8_t *params, size_t n)
{
    uint8_t tx[16];
    tx[0] = op;
    if (n) memcpy(tx + 1, params, n);
    rf_xfer(tx, NULL, n + 1);
}

static void write_reg(uint16_t addr, const uint8_t *data, size_t n)
{
    uint8_t tx[8];
    tx[0] = CMD_WRITE_REGISTER;
    tx[1] = addr >> 8;
    tx[2] = addr & 0xFF;
    memcpy(tx + 3, data, n);
    rf_xfer(tx, NULL, n + 3);
}

static void read_reg(uint16_t addr, uint8_t *out, size_t n)
{
    uint8_t tx[20] = {0}, rx[20] = {0};
    tx[0] = CMD_READ_REGISTER;
    tx[1] = addr >> 8;
    tx[2] = addr & 0xFF;
    rf_xfer(tx, rx, n + 4);     /* op,addrH,addrL,NOP, then data */
    memcpy(out, rx + 4, n);
}

static void write_buffer(const uint8_t *data, size_t n)
{
    uint8_t tx[2 + MAX_PKT];
    tx[0] = CMD_WRITE_BUFFER;
    tx[1] = 0x00; /* offset */
    memcpy(tx + 2, data, n);
    rf_xfer(tx, NULL, n + 2);
}

static void read_buffer(uint8_t offset, uint8_t *out, size_t n)
{
    uint8_t tx[3 + MAX_PKT] = {0}, rx[3 + MAX_PKT] = {0};
    tx[0] = CMD_READ_BUFFER;
    tx[1] = offset;
    tx[2] = 0x00; /* NOP */
    rf_xfer(tx, rx, n + 3);
    memcpy(out, rx + 3, n);
}

static uint16_t get_irq(void)
{
    uint8_t tx[3] = { CMD_GET_IRQ_STATUS, 0, 0 }, rx[3] = {0};
    rf_xfer(tx, rx, 3);
    return (uint16_t)((rx[1] << 8) | rx[2]);
}
static void clear_irq(uint16_t mask)
{
    uint8_t p[2] = { mask >> 8, mask & 0xFF };
    cmd(CMD_CLEAR_IRQ_STATUS, p, 2);
}
static void set_dio_irq(uint16_t irq, uint16_t dio1)
{
    uint8_t p[8] = { irq >> 8, irq & 0xFF, dio1 >> 8, dio1 & 0xFF, 0, 0, 0, 0 };
    cmd(CMD_SET_DIO_IRQ_PARAMS, p, 8);
}

void radio_chip_standby(void) { uint8_t m = STANDBY_RC; cmd(CMD_SET_STANDBY, &m, 1); }

/* --- modem config -------------------------------------------------------- */
static uint8_t bw_code(double bw_khz)
{
    int k = (int)(bw_khz / 2 + 0.01);
    switch (k) {
    case 3:   return 0x00; /* 7.8  */
    case 5:   return 0x08; /* 10.4 */
    case 7:   return 0x01; /* 15.6 */
    case 10:  return 0x09; /* 20.8 */
    case 15:  return 0x02; /* 31.25*/
    case 20:  return 0x0A; /* 41.7 */
    case 31:  return 0x03; /* 62.5 */
    case 62:  return 0x04; /* 125  */
    case 125: return 0x05; /* 250  */
    case 250: return 0x06; /* 500  */
    default:  return 0x05;
    }
}

static void set_modulation(void)
{
    double sym = (double)(1u << s_p.sf) / s_p.bw_khz;
    uint8_t ldro = (sym >= 16.0) ? LDRO_ON : LDRO_OFF;
    uint8_t p[4] = { (uint8_t)s_p.sf, bw_code(s_p.bw_khz), (uint8_t)(s_p.cr - 4), ldro };
    cmd(CMD_SET_MODULATION, p, 4);
}

static void set_packet_params(uint8_t payload_len)
{
    uint8_t p[6] = {
        (uint8_t)(s_p.preamble >> 8), (uint8_t)(s_p.preamble & 0xFF),
        HEADER_EXPLICIT, payload_len, CRC_ON, IQ_STANDARD
    };
    cmd(CMD_SET_PACKET_PARAMS, p, 6);
}

static void set_frequency(double mhz)
{
    uint32_t frf = (uint32_t)((mhz * FREQ_DIV) / (XTAL_HZ / 1000000.0));
    /* image calibration band select */
    uint8_t cal[2] = { 0xD7, 0xDB };       /* 902-928 default */
    if (mhz <= 900.0) { cal[0] = 0xD7; cal[1] = 0xD8; } /* 863-870 */
    cmd(CMD_CALIBRATE_IMAGE, cal, 2);
    uint8_t p[4] = { frf >> 24, frf >> 16, frf >> 8, frf };
    cmd(CMD_SET_RF_FREQUENCY, p, 4);
}

static void set_sync_word(int sync)
{
    uint8_t ctrl = 0x44;
    uint8_t d[2] = {
        (uint8_t)((sync & 0xF0) | ((ctrl & 0xF0) >> 4)),
        (uint8_t)(((sync & 0x0F) << 4) | (ctrl & 0x0F)),
    };
    write_reg(REG_LORA_SYNC_WORD_MSB, d, 2);
}

static void set_output_power(int dbm)
{
    if (dbm < -9) dbm = -9;
    if (dbm > 22) dbm = 22;
    uint8_t pa[4] = { 0x04, 0x07, 0x00, 0x01 };  /* SX1262 hi-power PA */
    cmd(CMD_SET_PA_CONFIG, pa, 4);
    uint8_t tp[2] = { (uint8_t)(dbm < 0 ? dbm + 256 : dbm), 0x04 /* ramp 200us */ };
    cmd(CMD_SET_TX_PARAMS, tp, 2);
    /* fixPaClamping: OR 0x1E into TX clamp config */
    uint8_t c; read_reg(REG_TX_CLAMP, &c, 1); c |= 0x1E; write_reg(REG_TX_CLAMP, &c, 1);
}

static void apply_params(void)
{
    set_modulation();
    set_packet_params(0xFF);
    set_sync_word(s_p.sync_word);
    set_frequency(s_p.freq_mhz);
    set_output_power(s_p.power_dbm);
}

/* --- DIO1 ISR ------------------------------------------------------------ */
static void IRAM_ATTR dio1_isr(void *arg)
{
    (void)arg;
    BaseType_t hp = pdFALSE;
    xSemaphoreGiveFromISR(s_dio1, &hp);
    if (hp) portYIELD_FROM_ISR();
}

esp_err_t radio_chip_init(const radio_params_t *p)
{
    s_p = *p;
    s_dio1 = xSemaphoreCreateBinary();

    /* GPIO: RST, BUSY, DIO1, RF_SW */
    gpio_config_t out = { .pin_bit_mask = (1ULL << PIN_RF_RST) | (1ULL << PIN_RF_SW),
                          .mode = GPIO_MODE_OUTPUT };
    gpio_config(&out);
    gpio_config_t in = { .pin_bit_mask = 1ULL << PIN_RF_BUSY, .mode = GPIO_MODE_INPUT };
    gpio_config(&in);
    gpio_set_level(PIN_RF_SW, 1); /* default RX */

    /* SPI bus + device (CS managed by peripheral). */
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_RF_MOSI, .sclk_io_num = PIN_RF_SCLK, .miso_io_num = PIN_RF_MISO,
        .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 4096,
    };
    if (spi_bus_initialize(RF_SPI_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
        ESP_LOGE(TAG, "radio spi bus init failed");
        return ESP_FAIL;
    }
    spi_device_interface_config_t dev = {
        .clock_speed_hz = RF_SPI_HZ, .mode = 0, .spics_io_num = PIN_RF_NSS, .queue_size = 4,
    };
    if (spi_bus_add_device(RF_SPI_HOST, &dev, &s_spi) != ESP_OK) return ESP_FAIL;

    /* Hardware reset (active LOW). */
    gpio_set_level(PIN_RF_RST, 0);
    esp_rom_delay_us(2000);
    gpio_set_level(PIN_RF_RST, 1);
    esp_rom_delay_us(5000);
    wait_busy();

    radio_chip_standby();

    /* TCXO on DIO3 @1.7V (code 0x01), ~5ms ready delay, then recalibrate. */
    uint8_t tcxo[4] = { 0x01, 0x00, 0x01, 0x40 }; /* voltage, delay(23:0)=0x000140=5ms */
    cmd(CMD_SET_DIO3_TCXO, tcxo, 4);
    uint8_t calall = 0x7F; cmd(CMD_CALIBRATE, &calall, 1);
    esp_rom_delay_us(5000);
    wait_busy();

    uint8_t lora = PACKET_TYPE_LORA; cmd(CMD_SET_PACKET_TYPE, &lora, 1);
    uint8_t dcdc = 0x01; cmd(CMD_SET_REGULATOR_MODE, &dcdc, 1);
    uint8_t rfsw = 0x01; cmd(CMD_SET_DIO2_RF_SWITCH, &rfsw, 1);
    uint8_t base[2] = { 0x00, 0x00 }; cmd(CMD_SET_BUFFER_BASE, base, 2);

    /* CAD params for listen-before-talk (2 symbols, SF-tuned peak). */
    uint8_t cadp[7] = { 0x02 /*2 sym*/, 22, 10, 0x00 /*CAD_ONLY*/, 0x00, 0x00, 0x00 };
    cmd(CMD_SET_CAD_PARAMS, cadp, 7);

    apply_params();

    /* DIO1 interrupt (rising). */
    gpio_config_t irq = { .pin_bit_mask = 1ULL << PIN_RF_DIO1, .mode = GPIO_MODE_INPUT,
                          .intr_type = GPIO_INTR_POSEDGE };
    gpio_config(&irq);
    gpio_isr_handler_add(PIN_RF_DIO1, dio1_isr, NULL); /* ISR service installed by keyboard */

    ESP_LOGI(TAG, "SX1262 up: %.3f MHz SF%d BW%.0f CR4/%d sync 0x%02X pwr %ddBm",
             s_p.freq_mhz, s_p.sf, s_p.bw_khz, s_p.cr, s_p.sync_word, s_p.power_dbm);
    return ESP_OK;
}

esp_err_t radio_chip_reconfigure(const radio_params_t *p)
{
    s_p = *p;
    radio_chip_standby();
    apply_params();
    radio_chip_start_rx();
    return ESP_OK;
}

static void rf_sw_tx(void) { gpio_set_level(PIN_RF_SW, 0); }
static void rf_sw_rx(void) { gpio_set_level(PIN_RF_SW, 1); }

int radio_chip_cad(void)
{
    radio_chip_standby();
    set_dio_irq(IRQ_CAD_DETECTED | IRQ_CAD_DONE, IRQ_CAD_DETECTED | IRQ_CAD_DONE);
    clear_irq(IRQ_ALL);
    xSemaphoreTake(s_dio1, 0);
    cmd(CMD_SET_CAD, NULL, 0);
    if (xSemaphoreTake(s_dio1, pdMS_TO_TICKS(60)) != pdTRUE) return -1;
    uint16_t r = get_irq();
    clear_irq(IRQ_ALL);
    if (r & IRQ_CAD_DETECTED) return 1;
    if (r & IRQ_CAD_DONE) return 0;
    return -1;
}

esp_err_t radio_chip_transmit(const uint8_t *data, int len)
{
    if (len > MAX_PKT) return ESP_ERR_INVALID_SIZE;
    radio_chip_standby();
    set_packet_params((uint8_t)len);
    set_dio_irq(IRQ_TX_DONE | IRQ_TIMEOUT, IRQ_TX_DONE);
    uint8_t base[2] = { 0, 0 }; cmd(CMD_SET_BUFFER_BASE, base, 2);
    write_buffer(data, (size_t)len);
    clear_irq(IRQ_ALL);
    xSemaphoreTake(s_dio1, 0);
    rf_sw_tx();
    uint8_t tx[3] = { 0, 0, 0 }; /* timeout 0 = single TX, no timeout */
    cmd(CMD_SET_TX, tx, 3);
    bool ok = xSemaphoreTake(s_dio1, pdMS_TO_TICKS(5000)) == pdTRUE;
    clear_irq(IRQ_ALL);
    rf_sw_rx();
    radio_chip_standby();
    return ok ? ESP_OK : ESP_ERR_TIMEOUT;
}

void radio_chip_start_rx(void)
{
    rf_sw_rx();
    set_packet_params(0xFF);
    set_dio_irq(IRQ_RX_DONE | IRQ_CRC_ERR | IRQ_HEADER_ERR | IRQ_TIMEOUT, IRQ_RX_DONE);
    clear_irq(IRQ_ALL);
    uint8_t to[3] = { (RX_CONTINUOUS >> 16) & 0xFF, (RX_CONTINUOUS >> 8) & 0xFF, RX_CONTINUOUS & 0xFF };
    cmd(CMD_SET_RX, to, 3);
}

int radio_chip_read_packet(uint8_t *buf, int cap, float *rssi, float *snr)
{
    uint16_t irq = get_irq();
    if (!(irq & IRQ_RX_DONE)) { clear_irq(IRQ_ALL); return -1; }
    if (irq & (IRQ_CRC_ERR | IRQ_HEADER_ERR)) { clear_irq(IRQ_ALL); return -1; }

    uint8_t st[2] = {0};
    uint8_t tx[3] = { CMD_GET_RX_BUFFER_STATUS, 0, 0 }, rx[3] = {0};
    rf_xfer(tx, rx, 3);
    int plen = rx[1];          /* payload length */
    uint8_t ptr = rx[2];       /* rx start ptr  */
    (void)st;
    if (plen > cap) plen = cap;
    read_buffer(ptr, buf, (size_t)plen);

    if (rssi || snr) {
        uint8_t ps_tx[4] = { CMD_GET_PACKET_STATUS, 0, 0, 0 }, ps_rx[4] = {0};
        rf_xfer(ps_tx, ps_rx, 4);
        if (rssi) *rssi = -((float)ps_rx[2]) / 2.0f;       /* RssiPkt */
        if (snr)  *snr  = ((int8_t)ps_rx[3]) / 4.0f;        /* SnrPkt  */
    }
    clear_irq(IRQ_ALL);
    return plen;
}

bool radio_chip_wait_event(int timeout_ms) { return xSemaphoreTake(s_dio1, pdMS_TO_TICKS(timeout_ms)) == pdTRUE; }
void radio_chip_wake(void) { xSemaphoreGive(s_dio1); }

void radio_chip_sleep(void)
{
    uint8_t m = 0x04; /* warm start, RTC off */
    cmd(CMD_SET_SLEEP, &m, 1);
    esp_rom_delay_us(500);
}
