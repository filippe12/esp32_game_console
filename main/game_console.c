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

void console_draw_screen(game_state game)
{
    switch (game)
    {
        case SNAKE:
            snake_start_screen();
            break;
        case TETRIS:
            tetris_start_screen();
            break;
        case FLAPPY_BIRD:
            Start_Screen();
            break;
    }
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
