/* See drivers/backlight.h. LEDC 10-bit PWM on PIN_LCD_BACKLIGHT. */
#include "drivers/backlight.h"
#include "board_pins.h"
#include "driver/ledc.h"

#define BL_TIMER   LEDC_TIMER_0
#define BL_CHANNEL LEDC_CHANNEL_0
#define BL_MODE    LEDC_LOW_SPEED_MODE

static int s_duty;

void backlight_init(void)
{
    ledc_timer_config_t t = {
        .speed_mode = BL_MODE,
        .timer_num = BL_TIMER,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&t);

    ledc_channel_config_t c = {
        .gpio_num = PIN_LCD_BACKLIGHT,
        .speed_mode = BL_MODE,
        .channel = BL_CHANNEL,
        .timer_sel = BL_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&c);
    backlight_set(LCD_BL_DUTY_MAX / 2);
}

void backlight_set(int duty)
{
    if (duty < 0) duty = 0;
    if (duty > LCD_BL_DUTY_MAX) duty = LCD_BL_DUTY_MAX;
    s_duty = duty;
    ledc_set_duty(BL_MODE, BL_CHANNEL, (uint32_t)duty);
    ledc_update_duty(BL_MODE, BL_CHANNEL);
}

int backlight_get(void) { return s_duty; }
