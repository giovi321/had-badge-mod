/* NV3007 TFT bring-up for the Communicator badge.
 *
 * Strategy: use LVGL's built-in NV3007 driver (lv_nv3007_create), which encodes
 * the correct init sequence (generic MIPI DCS). ESP-IDF's esp_lcd panel-IO (SPI)
 * is used only as a byte pipe for the driver's send_cmd / send_color callbacks;
 * DMA-complete signals lv_display_flush_ready. Rotation (270), the y=12 column
 * gap, and RGB565 byte-swap match the working MicroPython config.
 */
#include "drivers/display.h"
#include "board_pins.h"

#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"

static const char *TAG = "display";

static lv_display_t *s_disp;
static esp_lcd_panel_io_handle_t s_io;
static esp_timer_handle_t s_tick_timer;

/* LVGL renders the logical 428x142 frame; partial buffers ~1/8 screen each. */
#define DRAW_LINES   40
#define BUF_PX       (SCREEN_LOGICAL_W * DRAW_LINES)
#define SCREEN_LOGICAL_W 428

static bool on_color_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *e, void *ctx)
{
    (void)io; (void)e; (void)ctx;
    if (s_disp) lv_display_flush_ready(s_disp);
    return false;
}

/* LVGL NV3007 driver callbacks -> esp_lcd panel IO. cmd is a 1-byte DCS command.
 * Use the global s_io (not user_data): lv_nv3007_create() runs the init sequence
 * *during* the create call, before user_data could be set. */
static void lcd_send_cmd(lv_display_t *disp, const uint8_t *cmd, size_t cmd_size,
                         const uint8_t *param, size_t param_size)
{
    (void)disp; (void)cmd_size;
    esp_lcd_panel_io_tx_param(s_io, *cmd, param, param_size);
}

static void lcd_send_color(lv_display_t *disp, const uint8_t *cmd, size_t cmd_size,
                           uint8_t *param, size_t param_size)
{
    (void)disp; (void)cmd_size;
    esp_lcd_panel_io_tx_color(s_io, *cmd, param, param_size);
}

static void tick_cb(void *arg) { (void)arg; lv_tick_inc(2); }

static void reset_panel(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_LCD_RST,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(PIN_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_LCD_RST, 0);   /* reset is active LOW per board */
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
}

esp_err_t display_init(void)
{
    /* 1. SPI bus (MOSI + SCLK only; the NV3007 is write-only here). */
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_LCD_MOSI,
        .sclk_io_num = PIN_LCD_SCLK,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BUF_PX * 2 + 16,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "spi bus");

    /* 2. Panel IO over SPI (esp_lcd drives DC/CS and DMA). */
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = PIN_LCD_DC,
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = LCD_SPI_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = on_color_done,
        .user_ctx = NULL,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, &s_io),
                        TAG, "panel io");

    /* 3. Hardware reset, then let the LVGL driver run the init sequence. */
    reset_panel();

    if (!lv_is_initialized()) lv_init();

    /* 4. Create the LVGL NV3007 display. Native 142x428; rotate to 428x142. */
    s_disp = lv_nv3007_create(LCD_NATIVE_W, LCD_NATIVE_H, LV_LCD_FLAG_NONE,
                              lcd_send_cmd, lcd_send_color);
    if (!s_disp) { ESP_LOGE(TAG, "lv_nv3007_create failed"); return ESP_FAIL; }
    lv_display_set_user_data(s_disp, s_io);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_nv3007_set_gap(s_disp, LCD_OFFSET_X, LCD_OFFSET_Y);
    lv_display_set_rotation(s_disp, LV_DISPLAY_ROTATION_270);

    /* 5. Two partial draw buffers in internal DMA-capable RAM. */
    size_t buf_bytes = BUF_PX * 2;
    void *b1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    void *b2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!b1 || !b2) { ESP_LOGE(TAG, "draw buffer alloc"); return ESP_ERR_NO_MEM; }
    lv_display_set_buffers(s_disp, b1, b2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* 6. LVGL tick from a 2 ms esp_timer. */
    esp_timer_create_args_t targs = { .callback = tick_cb, .name = "lv_tick" };
    ESP_RETURN_ON_ERROR(esp_timer_create(&targs, &s_tick_timer), TAG, "tick timer");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(s_tick_timer, 2000), TAG, "tick start");

    ESP_LOGI(TAG, "NV3007 display up (428x142, rot270, gap 0/%d)", LCD_OFFSET_Y);
    return ESP_OK;
}

lv_display_t *display_handle(void) { return s_disp; }
