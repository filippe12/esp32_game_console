#pragma once
#include <u8g2.h>
#include "u8g2_esp32_hal.h"
#include "esp_sleep.h"

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

#define LEFT_BUTTON  15
#define DOWN_BUTTON  2
#define UP_BUTTON    27
#define RIGHT_BUTTON 26

#define PIN_SDA 21
#define PIN_SCL 22

extern u8g2_t u8g2;
extern u8g2_esp32_hal_t u8g2_esp32_hal;
extern int snake_highscore;
extern int tetris_highscore;
extern int flappy_bird_highscore;

void init_low_power_mode()
{
    uint64_t buttonPinMask = (1ULL << LEFT_BUTTON) | (1ULL << DOWN_BUTTON) |
                             (1ULL << RIGHT_BUTTON) | (1ULL << UP_BUTTON);
    esp_sleep_enable_ext1_wakeup(buttonPinMask, ESP_EXT1_WAKEUP_ANY_HIGH);
}

void init_buttons()
{
    int buttons[] = {LEFT_BUTTON, DOWN_BUTTON, UP_BUTTON, RIGHT_BUTTON};
    for(int i = 0; i < sizeof(buttons)/sizeof(buttons[0]); i++)
    {
        gpio_reset_pin(buttons[i]);
        gpio_set_direction(buttons[i], GPIO_MODE_INPUT);
        gpio_pullup_dis(buttons[i]);
        gpio_pulldown_en(buttons[i]);
    }
}

void init_display()
{
    u8g2_esp32_hal.bus.i2c.sda = PIN_SDA;
    u8g2_esp32_hal.bus.i2c.scl = PIN_SCL;
    u8g2_esp32_hal_init(u8g2_esp32_hal);


    u8g2_Setup_sh1106_i2c_128x64_noname_f(&u8g2, U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb); 
    
    u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);
    u8g2_InitDisplay(&u8g2);  // initialize display, display is in sleep mode after this
    u8g2_SetPowerSave(&u8g2, 0);  // wake up display
    u8g2_ClearBuffer(&u8g2);
    u8g2_SendBuffer(&u8g2);
}
