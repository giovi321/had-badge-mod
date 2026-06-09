/* See drivers/keyboard.h. TCA8418 over the new I2C master driver + INT ISR. */
#include "drivers/keyboard.h"
#include "board_pins.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "rom/ets_sys.h"

static const char *TAG = "kbd";

/* TCA8418 registers */
#define REG_CFG        0x01
#define REG_INT_STAT   0x02
#define REG_KEY_LCK_EC 0x03
#define REG_KEY_EVENT_A 0x04
#define REG_KP_GPIO1   0x1D
#define REG_KP_GPIO2   0x1E
#define REG_KP_GPIO3   0x1F

/* Key matrix: index = TCA8418 key id (1..80). Values per keyboard.py. */
static const int KEY_MATRIX[81] = {
    K_NONE,
    K_NONE, K_F1, '+', '9', '8', '7', K_F2, K_F3, K_F4, K_F5,
    K_ESC,  'q',  'w', 'e', 'r', 't', 'y',  'u',  'i',  'o',
    K_TAB,  'a',  's', 'd', 'f', 'g', 'h',  'j',  'k',  'l',
    K_SFT,  'z',  'x', 'c', 'v', 'b', 'n',  'm',  ',',  '.',
    K_CTL,  K_JW, K_ALT, '\\', ' ', K_NONE, K_RIGHT, K_DOWN, K_LEFT, K_ALT,
    K_NONE, K_NONE, '-', '6', '5', '4', ']', '[', 'p', K_NONE,
    K_NONE, K_NONE, '*', '3', '2', '1', K_ENTER, '\'', ';', K_NONE,
    K_NONE, K_NONE, '/', '=', '.', '0', K_SFT, K_UP, K_BS, K_NONE,
};
static const int SHIFT_MATRIX[81] = {
    K_NONE,
    K_NONE, K_F1, '+', '(', '*', '&', K_F2, K_F3, K_F4, K_F5,
    '`',    'Q',  'W', 'E', 'R', 'T', 'Y',  'U',  'I',  'O',
    K_TAB,  'A',  'S', 'D', 'F', 'G', 'H',  'J',  'K',  'L',
    K_SFT,  'Z',  'X', 'C', 'V', 'B', 'N',  'M',  '<',  '>',
    K_CTL,  K_JW, K_ALT, '|', ' ', K_NONE, K_RIGHT, K_DOWN, K_LEFT, K_ALT,
    K_NONE, K_NONE, '_', '^', '%', '$', '}', '{', 'P', K_NONE,
    K_NONE, K_NONE, '*', '#', '@', '!', K_ENTER, '"', ':', K_NONE,
    K_NONE, K_NONE, '?', '+', ',', ')', K_SFT, K_UP, K_DEL, K_NONE,
};

enum { FN_UP = 0, FN_UNREAD = 1, FN_READ = 2 };

static i2c_master_dev_handle_t s_dev;
static SemaphoreHandle_t s_int_sem;
static QueueHandle_t s_keyq;            /* uint32_t key codes -> LVGL indev */

static volatile bool s_shift, s_ctrl, s_alt, s_meta;
static volatile uint8_t s_fn[5], s_esc_fn;
static volatile uint32_t s_activity;

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t b[2] = { reg, val };
    return i2c_master_transmit(s_dev, b, 2, 50);
}
static esp_err_t reg_read(uint8_t reg, uint8_t *buf, size_t n)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, n, 50);
}

static void IRAM_ATTR int_isr(void *arg)
{
    (void)arg;
    BaseType_t hp = pdFALSE;
    xSemaphoreGiveFromISR(s_int_sem, &hp);
    if (hp) portYIELD_FROM_ISR();
}

static void set_fn(volatile uint8_t *st, bool pressed)
{
    if (pressed) { if (*st != FN_READ) *st = FN_UNREAD; }
    else *st = FN_UP;
}

static void handle_event(uint8_t ev)
{
    bool pressed = (ev & 0x80) != 0;
    int id = ev & 0x7F;
    if (id < 1 || id > 80) return;

    int cat = KEY_MATRIX[id];
    switch (cat) {
    case K_SFT: s_shift = pressed; return;
    case K_CTL: s_ctrl = pressed; return;
    case K_ALT: s_alt = pressed; return;
    case K_JW:  s_meta = pressed; return;
    case K_ESC:
        /* Esc is an app-level Back/Home (polled via edge state), not fed to
         * LVGL — so it never interferes with text fields or focus. */
        set_fn(&s_esc_fn, pressed);
        return;
    case K_F1: set_fn(&s_fn[0], pressed); return;
    case K_F2: set_fn(&s_fn[1], pressed); return;
    case K_F3: set_fn(&s_fn[2], pressed); return;
    case K_F4: set_fn(&s_fn[3], pressed); return;
    case K_F5: set_fn(&s_fn[4], pressed); return;
    default: break;
    }
    if (!pressed) return;                 /* text only on press */
    int code = s_shift ? SHIFT_MATRIX[id] : KEY_MATRIX[id];
    if (code == K_NONE) return;
    if (s_ctrl || s_alt) return;          /* swallowed (app shortcuts) */
    uint32_t k = (uint32_t)code;
    xQueueSend(s_keyq, &k, 0);
}

static void input_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (xSemaphoreTake(s_int_sem, portMAX_DELAY) != pdTRUE) continue;
        uint8_t cnt = 0;
        if (reg_read(REG_KEY_LCK_EC, &cnt, 1) != ESP_OK) continue;
        cnt &= 0x0F;
        if (cnt) s_activity++;
        for (int i = 0; i < cnt; i++) {
            uint8_t ev = 0;
            if (reg_read(REG_KEY_EVENT_A, &ev, 1) == ESP_OK) handle_event(ev);
        }
        reg_write(REG_INT_STAT, 0x01);    /* clear K_INT */
    }
}

esp_err_t keyboard_init(void)
{
    /* Hardware reset: >120us low pulse, >120us recovery. */
    gpio_config_t rst = { .pin_bit_mask = 1ULL << PIN_KBD_RST, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&rst);
    gpio_set_level(PIN_KBD_RST, 0);
    ets_delay_us(150);
    gpio_set_level(PIN_KBD_RST, 1);
    ets_delay_us(150);

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = KBD_I2C_PORT,
        .sda_io_num = PIN_KBD_SDA,
        .scl_io_num = PIN_KBD_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    esp_err_t e = i2c_new_master_bus(&bus_cfg, &bus);
    if (e != ESP_OK) { ESP_LOGE(TAG, "i2c bus: %s", esp_err_to_name(e)); return e; }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = KBD_I2C_ADDR,
        .scl_speed_hz = KBD_I2C_HZ,
    };
    e = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (e != ESP_OK) { ESP_LOGE(TAG, "i2c dev: %s", esp_err_to_name(e)); return e; }

    /* Configure the key array (TCA8418 datasheet p.41). */
    reg_write(REG_KP_GPIO1, 0xFF);  /* ROW7:0 -> matrix */
    reg_write(REG_KP_GPIO2, 0xFF);  /* COL7:0 -> matrix */
    reg_write(REG_KP_GPIO3, 0x03);  /* COL9:8 -> matrix */
    reg_write(REG_CFG, 0x91);       /* KE_IEN | INT_CFG | AI */
    reg_write(REG_INT_STAT, 0x01);  /* clear K_INT */

    s_int_sem = xSemaphoreCreateBinary();
    s_keyq = xQueueCreate(32, sizeof(uint32_t));

    gpio_config_t intp = {
        .pin_bit_mask = 1ULL << PIN_KBD_INT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&intp);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_KBD_INT, int_isr, NULL);

    xTaskCreatePinnedToCore(input_task, "kbd", 3072, NULL, 5, NULL, 0);
    ESP_LOGI(TAG, "TCA8418 keyboard up");
    return ESP_OK;
}

bool keyboard_pop(uint32_t *out) { return xQueueReceive(s_keyq, out, 0) == pdTRUE; }

bool keyboard_f_pressed(int n)
{
    if (n < 1 || n > 5) return false;
    if (s_fn[n - 1] == FN_UNREAD) { s_fn[n - 1] = FN_READ; return true; }
    return false;
}
bool keyboard_esc_pressed(void)
{
    if (s_esc_fn == FN_UNREAD) { s_esc_fn = FN_READ; return true; }
    return false;
}
bool keyboard_shift(void) { return s_shift; }
bool keyboard_ctrl(void)  { return s_ctrl; }
bool keyboard_alt(void)   { return s_alt; }
bool keyboard_meta(void)  { return s_meta; }
uint32_t keyboard_activity_count(void) { return s_activity; }
