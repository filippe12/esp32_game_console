#include "driver/rtc_io.h"
#include "globals.h"
#include "../games/snake.h"
#include "../games/tetris.h"
#include "../games/flappy_bird.h"

typedef enum game_state{
    SNAKE, TETRIS, FLAPPY_BIRD
} game_state;

u8g2_t u8g2;
u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
int snake_highscore = 0;
int tetris_highscore = 0;
int flappy_bird_highscore = 0;

void console_draw_frame()
{
    //draw left and right arrows
    for(int i = 0; i < 10; i++)
    {
        u8g2_DrawLine(&u8g2, 6 + i,   DISPLAY_HEIGHT/2 - 2 - i, 6 + i,   DISPLAY_HEIGHT/2 - 1 + i);
        u8g2_DrawLine(&u8g2, 125 - i, DISPLAY_HEIGHT/2 - 2 - i, 125 - i, DISPLAY_HEIGHT/2 - 1 + i);
    }

    //draw left frame
    u8g2_DrawFrame(&u8g2, 22, DISPLAY_HEIGHT/2 - 12, 22, 22);

    //draw middle frame
    u8g2_DrawFrame(&u8g2, 48, DISPLAY_HEIGHT/2 - 18, 36, 36);
    u8g2_DrawFrame(&u8g2, 50, DISPLAY_HEIGHT/2 - 16, 32, 32);

    //draw right frame
    u8g2_DrawFrame(&u8g2, 88, DISPLAY_HEIGHT/2 - 12, 22, 22);

    u8g2_SetFont(&u8g2, u8g2_font_6x12_tr);

    // Draw "PLAY" below middle frame
    const char *bottom_text = "PLAY";
    int bottom_text_width = u8g2_GetStrWidth(&u8g2, bottom_text);
    u8g2_DrawStr(&u8g2, (128 - bottom_text_width) / 2 + 2, DISPLAY_HEIGHT/2 + 28, bottom_text);

    // Draw "PLAY LAST GAME" above middle frame
    const char *top_text = "PLAY LAST GAME";
    int top_text_width = u8g2_GetStrWidth(&u8g2, top_text);
    u8g2_DrawStr(&u8g2, (128 - top_text_width) / 2 + 3, DISPLAY_HEIGHT/2 - 20, top_text);
}

void console_draw_screen(game_state game)
{
    u8g2_ClearBuffer(&u8g2);

    console_draw_frame();

    switch (game)
    {
        case SNAKE:
            flappy_bird_draw_left_frame();
            snake_draw_middle_frame();
            tetris_draw_right_frame();
            break;
        case TETRIS:
            snake_draw_left_frame();
            tetris_draw_middle_frame();
            flappy_bird_draw_right_frame();
            break;
        case FLAPPY_BIRD:
            tetris_draw_left_frame();
            flappy_bird_draw_middle_frame();
            snake_draw_right_frame();
            break;
    }

    u8g2_SendBuffer(&u8g2);
}

void app_main()
{
    init_buttons();
    init_display();
    init_low_power_mode();
    srand(time(0));

    game_state selected_game = SNAKE;
    game_state previous_game = SNAKE;

    while(true)
    {
        console_draw_screen(selected_game);

        esp_light_sleep_start();

        if(gpio_get_level(LEFT_BUTTON))
            switch(selected_game)
            {
                case SNAKE:
                    selected_game = FLAPPY_BIRD; break;
                case TETRIS:
                    selected_game = SNAKE; break;
                case FLAPPY_BIRD:
                    selected_game = TETRIS; break;
            }

        if(gpio_get_level(RIGHT_BUTTON))
            switch(selected_game)
            {
                case SNAKE:
                    selected_game = TETRIS; break;
                case TETRIS:
                    selected_game = FLAPPY_BIRD; break;
                case FLAPPY_BIRD:
                    selected_game = SNAKE; break;
            }

        if(gpio_get_level(UP_BUTTON))
            switch(previous_game)
            {
                case SNAKE:
                    snake_run(); break;
                case TETRIS:
                    tetris_run(); break;
                case FLAPPY_BIRD:
                    flappy_bird_run(); break;
            }

        if(gpio_get_level(DOWN_BUTTON))
        {
            switch(selected_game)
            {
                case SNAKE:
                    snake_run(); break;
                case TETRIS:
                    tetris_run(); break;
                case FLAPPY_BIRD:
                    flappy_bird_run(); break;
            }
            previous_game = selected_game;
        }
    }
}
