#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "sdkconfig.h"
#include "driver/rtc_io.h"
#include "../main/globals.h"

#define TETRIS_BLOCK_SIZE 3
#define TETRIS_MAP_WIDTH  10
#define TETRIS_MAP_HEIGHT 20
#define TETRIS_MAX_SPEED  5
#define TETRIS_NUMBER_OF_BLOCKS 9

static bool tetris_map[20][10];

typedef enum block_rotation
{
    NO_ROTATION, LEFT_90, RIGHT_90, UPSIDE_DOWN
} block_rotation;

void tetris_shift_rows_down(short int starting_row, short int amount)
{
    for(int row = starting_row; row < TETRIS_MAP_HEIGHT - amount; row++)
    {
        memcpy(tetris_map[row], tetris_map[row + amount], sizeof(tetris_map[row]));
    }
    for(int row = TETRIS_MAP_HEIGHT - amount; row < TETRIS_MAP_HEIGHT; row++)
    {
        memset(tetris_map[row], 0, sizeof(tetris_map[row]));
    }
}

void tetris_start_screen()
{
    u8g2_ClearBuffer(&u8g2);

    u8g2_SetFont(&u8g2, u8g2_font_logisoso32_tr);
    const char *title = "Tetris";
    short int title_width = u8g2_GetStrWidth(&u8g2, title);
    short int title_x = (DISPLAY_WIDTH - title_width) / 2;
    u8g2_DrawStr(&u8g2, title_x, 42, title);

    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    const char *prompt = "Press any button to play";
    short int prompt_width = u8g2_GetStrWidth(&u8g2, prompt);
    short int prompt_x = (DISPLAY_WIDTH - prompt_width) / 2;
    u8g2_DrawStr(&u8g2, prompt_x, 60, prompt);

    u8g2_SendBuffer(&u8g2);
}

void tetris_end_screen(int score)
{
    u8g2_ClearBuffer(&u8g2);

    u8g2_SetFont(&u8g2, u8g2_font_helvB10_tr);
    const char *msg = (score > tetris_highscore) ? "New High Score!" : "Game Over";
    int msg_x = (DISPLAY_WIDTH - u8g2_GetStrWidth(&u8g2, msg)) / 2 - 2;
    u8g2_DrawStr(&u8g2, msg_x, 16, msg);

    char buf[32];
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    snprintf(buf, sizeof(buf), "Score: %d", score);
    int score_x = (DISPLAY_WIDTH - u8g2_GetStrWidth(&u8g2, buf)) / 2;
    u8g2_DrawStr(&u8g2, score_x, 32, buf);

    if (score <= tetris_highscore) {
        snprintf(buf, sizeof(buf), "Best: %d", tetris_highscore);
        int best_x = (DISPLAY_WIDTH - u8g2_GetStrWidth(&u8g2, buf)) / 2;
        u8g2_DrawStr(&u8g2, best_x, 44, buf);
    }
    
    u8g2_SetFont(&u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(&u8g2, 5, 60, "Play Again");
    u8g2_DrawStr(&u8g2, 95, 60, "Exit");

    u8g2_SendBuffer(&u8g2);

    if (score > tetris_highscore)
        tetris_highscore = score;
}

void tetris_draw_frame()
{
    short int x1 = DISPLAY_WIDTH/2;
    short int x2 = x1 + TETRIS_MAP_WIDTH*TETRIS_BLOCK_SIZE + 1;
    short int y1 = (DISPLAY_HEIGHT - TETRIS_BLOCK_SIZE*TETRIS_MAP_HEIGHT - 2)/2;
    short int y2 = y1 + TETRIS_MAP_HEIGHT*TETRIS_BLOCK_SIZE + 2; 
    u8g2_DrawLine(&u8g2, x1, DISPLAY_HEIGHT - y1, x2, DISPLAY_HEIGHT - y1);
    u8g2_DrawLine(&u8g2, x1, DISPLAY_HEIGHT - y2, x2, DISPLAY_HEIGHT - y2);
    u8g2_DrawLine(&u8g2, x1, DISPLAY_HEIGHT - y2, x1, DISPLAY_HEIGHT - y1);
    u8g2_DrawLine(&u8g2, x2, DISPLAY_HEIGHT - y2, x2, DISPLAY_HEIGHT - y1);
}

void tetris_draw_blocks()
{
    short int x_offset = DISPLAY_WIDTH/2 + 1;
    short int y_offset = (DISPLAY_HEIGHT - TETRIS_BLOCK_SIZE*TETRIS_MAP_HEIGHT - 2)/2 + 1;
    for(int row = 0; row < TETRIS_MAP_HEIGHT; row++)
    {
        for(int col = 0; col < TETRIS_MAP_WIDTH; col++)
        {
            if(tetris_map[row][col])
                u8g2_DrawBox(&u8g2, x_offset + col*TETRIS_BLOCK_SIZE,
                    DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + row*TETRIS_BLOCK_SIZE),
                    TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
        }
    }
}

void tetris_draw_active_block(short int map_x, short int map_y, short int id, block_rotation rotation)
{
    short int x_offset = DISPLAY_WIDTH/2 + 1;
    short int y_offset = (DISPLAY_HEIGHT - TETRIS_BLOCK_SIZE*TETRIS_MAP_HEIGHT - 2)/2 + 1;
    switch(id)
    {
        case 0: //single block
            u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                    DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                    TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);       
            break;

        case 1: //2x2 block
            u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                    DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                    2*TETRIS_BLOCK_SIZE, 2*TETRIS_BLOCK_SIZE);       
            break;

        case 2: //small L block
            u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                    DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                    2*TETRIS_BLOCK_SIZE, 2*TETRIS_BLOCK_SIZE);       
            u8g2_SetDrawColor(&u8g2, 0);
            switch (rotation)
            {
                case NO_ROTATION:
                    u8g2_DrawBox(&u8g2, x_offset + (map_x + 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);       
                    break;
                case RIGHT_90:
                    u8g2_DrawBox(&u8g2, x_offset + (map_x + 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 1)*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);       
                    break;
                case UPSIDE_DOWN:
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 1)*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);       
                    break;
                case LEFT_90:
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);       
                    break;
            }
            u8g2_SetDrawColor(&u8g2, 1);
            break;

        case 3: //t block
            u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                    DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                    TETRIS_BLOCK_SIZE, 2*TETRIS_BLOCK_SIZE);
            switch(rotation)
            {
                case NO_ROTATION:
                    u8g2_DrawBox(&u8g2, x_offset + (map_x - 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        3*TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    break;
                case RIGHT_90:
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 2)*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    u8g2_DrawBox(&u8g2, x_offset + (map_x - 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 1)*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    break;
                case UPSIDE_DOWN:
                    u8g2_DrawBox(&u8g2, x_offset + (map_x - 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 1)*TETRIS_BLOCK_SIZE),
                        3*TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    break;
                case LEFT_90:
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 2)*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    u8g2_DrawBox(&u8g2, x_offset + (map_x + 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 1)*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    break;
            } break;

        case 4: //z block
            switch(rotation)
            {
                case NO_ROTATION:
                case UPSIDE_DOWN:
                    u8g2_DrawBox(&u8g2, x_offset + (map_x - 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        2*TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 1)*TETRIS_BLOCK_SIZE),
                        2*TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    break;
                case RIGHT_90:
                case LEFT_90:
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, 2*TETRIS_BLOCK_SIZE);
                    u8g2_DrawBox(&u8g2, x_offset + (map_x - 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 1)*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, 2*TETRIS_BLOCK_SIZE);
                    break;
            } break;

        case 5: //reverse z block
            switch(rotation)
            {
                case NO_ROTATION:
                case UPSIDE_DOWN:
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        2*TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    u8g2_DrawBox(&u8g2, x_offset + (map_x - 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 1)*TETRIS_BLOCK_SIZE),
                        2*TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    break;
                case RIGHT_90:
                case LEFT_90:
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, 2*TETRIS_BLOCK_SIZE);
                    u8g2_DrawBox(&u8g2, x_offset + (map_x + 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 1)*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, 2*TETRIS_BLOCK_SIZE);
                    break;
            } break;

        case 6: //L block
            switch(rotation)
            {
                case NO_ROTATION:
                    u8g2_DrawBox(&u8g2, x_offset + (map_x + 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, 2*TETRIS_BLOCK_SIZE);
                    u8g2_DrawBox(&u8g2, x_offset + (map_x - 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 1)*TETRIS_BLOCK_SIZE),
                        2*TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    break;
                case RIGHT_90:
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 2)*TETRIS_BLOCK_SIZE),
                        2*TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, 2*TETRIS_BLOCK_SIZE);
                    break;
                case UPSIDE_DOWN:
                    u8g2_DrawBox(&u8g2, x_offset + (map_x - 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, 2*TETRIS_BLOCK_SIZE);
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        2*TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    break;
                case LEFT_90:
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        2*TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    u8g2_DrawBox(&u8g2, x_offset + (map_x + 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 1)*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, 2*TETRIS_BLOCK_SIZE);
                    break;
            } break;

        case 7: //reverse L block
            switch(rotation)
            {
                case NO_ROTATION:
                    u8g2_DrawBox(&u8g2, x_offset + (map_x - 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, 2*TETRIS_BLOCK_SIZE);
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 1)*TETRIS_BLOCK_SIZE),
                        2*TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    break;
                case RIGHT_90:
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        2*TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 1)*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, 2*TETRIS_BLOCK_SIZE);
                    break;
                case UPSIDE_DOWN:
                    u8g2_DrawBox(&u8g2, x_offset + (map_x + 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, 2*TETRIS_BLOCK_SIZE);
                    u8g2_DrawBox(&u8g2, x_offset + (map_x - 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        2*TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    break;
                case LEFT_90:
                    u8g2_DrawBox(&u8g2, x_offset + (map_x + 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, 2*TETRIS_BLOCK_SIZE);
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + (map_y - 2)*TETRIS_BLOCK_SIZE),
                        2*TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    break;
            } break;

        case 8: //4x1 long block
            switch(rotation)
            {
                case NO_ROTATION:
                case UPSIDE_DOWN:
                    u8g2_DrawBox(&u8g2, x_offset + (map_x - 1)*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        4*TETRIS_BLOCK_SIZE, TETRIS_BLOCK_SIZE);
                    break;
                case RIGHT_90:
                case LEFT_90:
                    u8g2_DrawBox(&u8g2, x_offset + map_x*TETRIS_BLOCK_SIZE,
                        DISPLAY_HEIGHT - (TETRIS_BLOCK_SIZE - 1) - (y_offset + map_y*TETRIS_BLOCK_SIZE),
                        TETRIS_BLOCK_SIZE, 4*TETRIS_BLOCK_SIZE);
                    break;
            } break;
    }
}

void tetris_draw_background(int score, short int speed, short int next_id)
{
    u8g2_SetFont(&u8g2, u8g2_font_4x6_tf);

    char buf[16];
    const int ui_x = 40;
    int y = 6;

    // --- SCORE ---
    u8g2_DrawStr(&u8g2, ui_x, y, "SCORE");
    snprintf(buf, sizeof(buf), "%d", score);
    y += 7;
    u8g2_DrawFrame(&u8g2, ui_x, y - 6, 19, 9);
    int score_width = u8g2_GetStrWidth(&u8g2, buf);
    u8g2_DrawStr(&u8g2, ui_x + 19 - score_width - 2, y + 1, buf);
    y += 11;

    // --- SPEED ---
    u8g2_DrawStr(&u8g2, ui_x, y, "SPEED");
    snprintf(buf, sizeof(buf), "%d", speed);
    y += 7;
    u8g2_DrawFrame(&u8g2, ui_x, y - 6, 19, 9);
    int speed_width = u8g2_GetStrWidth(&u8g2, buf);
    u8g2_DrawStr(&u8g2, ui_x + 19 - speed_width - 2, y + 1, buf);
    y += 17;

    // --- NEXT Block ---
    u8g2_DrawStr(&u8g2, ui_x + 3, y, "NEXT");
    y += 2;
    int preview_x = ui_x + 2;
    int preview_y = y;
    u8g2_DrawFrame(&u8g2, preview_x - 1, preview_y - 1, 18, 12);
    switch(next_id)
    {
        case 0: //single block
            u8g2_DrawBox(&u8g2, preview_x + 7, preview_y + 4, 2, 2);
            break;

        case 1: //2x2 block
            u8g2_DrawBox(&u8g2, preview_x + 6, preview_y + 3, 4, 4);
            break;

        case 2: //small L block
            u8g2_DrawBox(&u8g2, preview_x + 6, preview_y + 3, 2, 4);
            u8g2_DrawBox(&u8g2, preview_x + 8, preview_y + 5, 2, 2);
            break;

        case 3: //t block
            u8g2_DrawBox(&u8g2, preview_x + 5, preview_y + 3, 6, 2);
            u8g2_DrawBox(&u8g2, preview_x + 7, preview_y + 5, 2, 2);
            break;

        case 4: //z block
            u8g2_DrawBox(&u8g2, preview_x + 5, preview_y + 3, 4, 2);
            u8g2_DrawBox(&u8g2, preview_x + 7, preview_y + 5, 4, 2);
            break;

        case 5: //reverse z block
            u8g2_DrawBox(&u8g2, preview_x + 7, preview_y + 3, 4, 2);
            u8g2_DrawBox(&u8g2, preview_x + 5, preview_y + 5, 4, 2);
            break;

        case 6: //L block
            u8g2_DrawBox(&u8g2, preview_x + 9, preview_y + 3, 2, 2);
            u8g2_DrawBox(&u8g2, preview_x + 5, preview_y + 5, 6, 2);
            break;

        case 7: //reverse L block
            u8g2_DrawBox(&u8g2, preview_x + 5, preview_y + 3, 2, 2);
            u8g2_DrawBox(&u8g2, preview_x + 5, preview_y + 5, 6, 2);
            break;

        case 8: //4x1 long block
            u8g2_DrawBox(&u8g2, preview_x + 4, preview_y + 4, 8, 2);
            break;
    }
}

void tetris_draw_row_deletion(short int row, short int count, int score, short int speed, short int next_id)
{
    if(row == -1)
        return;

    for(int i = 0; i < TETRIS_MAP_WIDTH/2; i++)
    {
        for(int j = 0; j < count; j++)
        {
            tetris_map[row + j][TETRIS_MAP_WIDTH/2 + i] = false;
            tetris_map[row + j][TETRIS_MAP_WIDTH/2 - 1 - i] = false;
        }
        u8g2_ClearBuffer(&u8g2);
        tetris_draw_background(score, speed, next_id);
        tetris_draw_frame();
        tetris_draw_blocks();
        u8g2_SendBuffer(&u8g2);
    }

    tetris_shift_rows_down(row, count);
    u8g2_ClearBuffer(&u8g2);
    tetris_draw_background(score, speed, next_id);
    tetris_draw_frame();
    tetris_draw_blocks();
    u8g2_SendBuffer(&u8g2);
}

bool tetris_block_fits(short int map_x, short int map_y, short int id, block_rotation rotation)
{
    switch(id)
    {
        case 0: //signle block
            if(map_x >= TETRIS_MAP_WIDTH || map_x < 0 || map_y < 0)
                return false;
            if(tetris_map[map_y][map_x])
                return false;
            break;

        case 1: //2x2 block
            if((map_x + 1) >= TETRIS_MAP_WIDTH || map_x < 0 || (map_y - 1) < 0)
                return false;
            if(tetris_map[map_y][map_x] || tetris_map[map_y - 1][map_x + 1] ||
                tetris_map[map_y - 1][map_x] || tetris_map[map_y][map_x + 1])
                return false;
            break;

        case 2: //small L block
            if((map_x + 1) >= TETRIS_MAP_WIDTH || map_x < 0 || (map_y - 1) < 0)
                return false;
            switch(rotation)
            {
                case NO_ROTATION:
                    if(tetris_map[map_y][map_x] || tetris_map[map_y - 1][map_x + 1] || tetris_map[map_y - 1][map_x])
                        return false;
                    break;
                case RIGHT_90:
                    if(tetris_map[map_y][map_x] || tetris_map[map_y - 1][map_x] || tetris_map[map_y][map_x + 1])
                        return false;
                    break;
                case UPSIDE_DOWN:
                    if(tetris_map[map_y][map_x] || tetris_map[map_y - 1][map_x + 1] || tetris_map[map_y][map_x + 1])
                        return false;
                    break;
                case LEFT_90:
                    if(tetris_map[map_y - 1][map_x + 1] || tetris_map[map_y - 1][map_x] || tetris_map[map_y][map_x + 1])
                        return false;
                    break;
            }
            break;

        case 3: //t block
            switch(rotation)
            {
                case NO_ROTATION:
                    if((map_x + 1) >= TETRIS_MAP_WIDTH || (map_x - 1) < 0 || (map_y - 1) < 0)
                        return false;
                    if(tetris_map[map_y][map_x] || tetris_map[map_y - 1][map_x] ||
                        tetris_map[map_y][map_x + 1] || tetris_map[map_y][map_x - 1])
                        return false;
                    break;
                case RIGHT_90:
                    if(map_x >= TETRIS_MAP_WIDTH || (map_x - 1) < 0 || (map_y - 2) < 0)
                        return false;
                    if(tetris_map[map_y][map_x] || tetris_map[map_y - 1][map_x] ||
                        tetris_map[map_y - 2][map_x] || tetris_map[map_y - 1][map_x - 1])
                        return false;
                    break;
                case UPSIDE_DOWN:
                    if((map_x + 1) >= TETRIS_MAP_WIDTH || (map_x - 1) < 0 || (map_y - 1) < 0)
                        return false;
                    if(tetris_map[map_y][map_x] || tetris_map[map_y - 1][map_x] ||
                        tetris_map[map_y - 1][map_x + 1] || tetris_map[map_y - 1][map_x - 1])
                        return false;
                    break;
                case LEFT_90:
                    if((map_x + 1) >= TETRIS_MAP_WIDTH || map_x < 0 || (map_y - 2) < 0)
                        return false;
                    if(tetris_map[map_y][map_x] || tetris_map[map_y - 1][map_x] ||
                        tetris_map[map_y - 2][map_x] || tetris_map[map_y - 1][map_x + 1])
                        return false;
                    break;
            }
            break;

        case 4: //z block
            switch (rotation)
            {
                case NO_ROTATION:
                case UPSIDE_DOWN:
                    if((map_x + 1) >= TETRIS_MAP_WIDTH || (map_x - 1) < 0 || (map_y - 1) < 0)
                        return false;
                    if(tetris_map[map_y][map_x - 1] || tetris_map[map_y][map_x] ||
                        tetris_map[map_y - 1][map_x] || tetris_map[map_y - 1][map_x + 1])
                        return false;
                    break;
                case RIGHT_90:
                case LEFT_90:
                    if(map_x >= TETRIS_MAP_WIDTH || (map_x - 1) < 0 || (map_y - 2) < 0)
                        return false;
                    if(tetris_map[map_y][map_x] || tetris_map[map_y - 1][map_x] ||
                        tetris_map[map_y - 1][map_x - 1] || tetris_map[map_y - 2][map_x - 1])
                        return false;
                    break;
            } break;

        case 5: //reverse z block
            switch (rotation)
            {
                case NO_ROTATION:
                case UPSIDE_DOWN:
                    if((map_x + 1) >= TETRIS_MAP_WIDTH || (map_x - 1) < 0 || (map_y - 1) < 0)
                        return false;
                    if(tetris_map[map_y][map_x + 1] || tetris_map[map_y][map_x] ||
                        tetris_map[map_y - 1][map_x] || tetris_map[map_y - 1][map_x - 1])
                        return false;
                    break;
                case RIGHT_90:
                case LEFT_90:
                    if((map_x + 1) >= TETRIS_MAP_WIDTH || map_x < 0 || (map_y - 2) < 0)
                        return false;
                    if(tetris_map[map_y][map_x] || tetris_map[map_y - 1][map_x] ||
                        tetris_map[map_y - 1][map_x + 1] || tetris_map[map_y - 2][map_x + 1])
                        return false;
                    break;
            } break;

        case 6: //L block
            switch (rotation)
            {
                case NO_ROTATION:
                    if((map_x + 1) >= TETRIS_MAP_WIDTH || (map_x - 1) < 0 || (map_y - 1) < 0)
                        return false;
                    if(tetris_map[map_y - 1][map_x - 1] || tetris_map[map_y - 1][map_x + 1] ||
                        tetris_map[map_y - 1][map_x] || tetris_map[map_y][map_x + 1])
                        return false;
                    break;
                case RIGHT_90:
                    if((map_x + 1) >= TETRIS_MAP_WIDTH || map_x < 0 || (map_y - 2) < 0)
                        return false;
                    if(tetris_map[map_y][map_x] || tetris_map[map_y - 2][map_x + 1] ||
                        tetris_map[map_y - 1][map_x] || tetris_map[map_y - 2][map_x])
                        return false;
                    break;
                case UPSIDE_DOWN:
                    if((map_x + 1) >= TETRIS_MAP_WIDTH || (map_x - 1) < 0 || (map_y - 1) < 0)
                        return false;
                    if(tetris_map[map_y][map_x - 1] || tetris_map[map_y][map_x] ||
                        tetris_map[map_y][map_x + 1] || tetris_map[map_y - 1][map_x - 1])
                        return false;
                    break;
                case LEFT_90:
                    if((map_x + 1) >= TETRIS_MAP_WIDTH || map_x < 0 || (map_y - 2) < 0)
                        return false;
                    if(tetris_map[map_y][map_x + 1] || tetris_map[map_y - 1][map_x + 1] ||
                        tetris_map[map_y - 2][map_x + 1] || tetris_map[map_y][map_x])
                        return false;
                    break;
            } break;

        case 7: //reverse L block
            switch (rotation)
            {
                case NO_ROTATION:
                    if((map_x + 1) >= TETRIS_MAP_WIDTH || (map_x - 1) < 0 || (map_y - 1) < 0)
                        return false;
                    if(tetris_map[map_y][map_x - 1] || tetris_map[map_y - 1][map_x - 1] ||
                        tetris_map[map_y - 1][map_x] || tetris_map[map_y - 1][map_x + 1])
                        return false;
                    break;
                case RIGHT_90:
                    if((map_x + 1) >= TETRIS_MAP_WIDTH || map_x < 0 || (map_y - 2) < 0)
                        return false;
                    if(tetris_map[map_y][map_x] || tetris_map[map_y][map_x + 1] ||
                        tetris_map[map_y - 1][map_x] || tetris_map[map_y - 2][map_x])
                        return false;
                    break;
                case UPSIDE_DOWN:
                    if((map_x + 1) >= TETRIS_MAP_WIDTH || (map_x - 1) < 0 || (map_y - 1) < 0)
                        return false;
                    if(tetris_map[map_y][map_x - 1] || tetris_map[map_y][map_x] ||
                        tetris_map[map_y][map_x + 1] || tetris_map[map_y - 1][map_x + 1])
                        return false;
                    break;
                case LEFT_90:
                    if((map_x + 1) >= TETRIS_MAP_WIDTH || map_x < 0 || (map_y - 2) < 0)
                        return false;
                    if(tetris_map[map_y][map_x + 1] || tetris_map[map_y - 1][map_x + 1] ||
                        tetris_map[map_y - 2][map_x] || tetris_map[map_y - 2][map_x + 1])
                        return false;
                    break;
            } break;

        case 8: //4x1 long block
            switch (rotation)
            {
                case NO_ROTATION:
                case UPSIDE_DOWN:
                    if((map_x + 2) >= TETRIS_MAP_WIDTH || (map_x - 1) < 0 || map_y < 0)
                        return false;
                    if(tetris_map[map_y][map_x - 1] || tetris_map[map_y][map_x] ||
                        tetris_map[map_y][map_x + 1] || tetris_map[map_y][map_x + 2])
                        return false;
                    break;
                case RIGHT_90:
                case LEFT_90:
                    if(map_x >= TETRIS_MAP_WIDTH || map_x < 0 || (map_y - 3) < 0)
                        return false;
                    if(tetris_map[map_y][map_x] || tetris_map[map_y - 1][map_x] ||
                        tetris_map[map_y - 2][map_x] || tetris_map[map_y - 3][map_x])
                        return false;
                    break;
            } break;
    }
    return true;
}

void tetris_deactivate_block(short int map_x, short int map_y, short int id, block_rotation rotation)
{
    switch(id)
    {
        case 0: //single block
            tetris_map[map_y][map_x] = true;
            break;

        case 1: //2x2 block
            tetris_map[map_y][map_x] = true;
            tetris_map[map_y][map_x + 1] = true;
            tetris_map[map_y - 1][map_x] = true;
            tetris_map[map_y - 1][map_x + 1] = true;
            break;

        case 2: //small L block
            switch(rotation)
            {
                case NO_ROTATION:
                    tetris_map[map_y][map_x] = true;
                    tetris_map[map_y - 1][map_x] = true;
                    tetris_map[map_y - 1][map_x + 1] = true;
                    break;
                case RIGHT_90:
                    tetris_map[map_y][map_x] = true;
                    tetris_map[map_y][map_x + 1] = true;
                    tetris_map[map_y - 1][map_x] = true;
                    break;
                case UPSIDE_DOWN:
                    tetris_map[map_y][map_x] = true;
                    tetris_map[map_y][map_x + 1] = true;
                    tetris_map[map_y - 1][map_x + 1] = true;
                    break;
                case LEFT_90:
                    tetris_map[map_y][map_x + 1] = true;
                    tetris_map[map_y - 1][map_x] = true;
                    tetris_map[map_y - 1][map_x + 1] = true;
                    break;
            } break;

        case 3: //t block
            switch(rotation)
            {
                case NO_ROTATION:
                    tetris_map[map_y][map_x] = true;
                    tetris_map[map_y - 1][map_x] = true;
                    tetris_map[map_y][map_x + 1] = true;
                    tetris_map[map_y][map_x - 1] = true;
                    break;
                case RIGHT_90:
                    tetris_map[map_y][map_x] = true;
                    tetris_map[map_y - 1][map_x] = true;
                    tetris_map[map_y - 2][map_x] = true;
                    tetris_map[map_y - 1][map_x - 1] = true;
                    break;
                case UPSIDE_DOWN:
                    tetris_map[map_y][map_x] = true;
                    tetris_map[map_y - 1][map_x] = true;
                    tetris_map[map_y - 1][map_x + 1] = true;
                    tetris_map[map_y - 1][map_x - 1] = true;
                    break;
                case LEFT_90:
                    tetris_map[map_y][map_x] = true;
                    tetris_map[map_y - 1][map_x] = true;
                    tetris_map[map_y - 2][map_x] = true;
                    tetris_map[map_y - 1][map_x + 1] = true;
                    break;
            } break;
        
        case 4: //z block
            tetris_map[map_y][map_x] = true;
            tetris_map[map_y - 1][map_x] = true;
            switch(rotation)
            {
                case NO_ROTATION:
                case UPSIDE_DOWN:
                    tetris_map[map_y][map_x - 1] = true;
                    tetris_map[map_y - 1][map_x + 1] = true;
                    break;
                case RIGHT_90:
                case LEFT_90:
                    tetris_map[map_y - 1][map_x - 1] = true;
                    tetris_map[map_y - 2][map_x - 1] = true;
                    break;
            } break;

        case 5: //reverse z block
            tetris_map[map_y][map_x] = true;
            tetris_map[map_y - 1][map_x] = true;
            switch(rotation)
            {
                case NO_ROTATION:
                case UPSIDE_DOWN:
                    tetris_map[map_y][map_x + 1] = true;
                    tetris_map[map_y - 1][map_x - 1] = true;
                    break;
                case RIGHT_90:
                case LEFT_90:
                    tetris_map[map_y - 1][map_x + 1] = true;
                    tetris_map[map_y - 2][map_x + 1] = true;
                    break;
            } break;
        
        case 6: //L block
            switch(rotation)
            {
                case NO_ROTATION:
                    tetris_map[map_y][map_x + 1] = true;
                    tetris_map[map_y - 1][map_x - 1] = true;
                    tetris_map[map_y - 1][map_x] = true;
                    tetris_map[map_y - 1][map_x + 1] = true;
                    break;
                case RIGHT_90:
                    tetris_map[map_y][map_x] = true;
                    tetris_map[map_y - 2][map_x + 1] = true;
                    tetris_map[map_y - 1][map_x] = true;
                    tetris_map[map_y - 2][map_x] = true;
                    break;
                case UPSIDE_DOWN:
                    tetris_map[map_y][map_x - 1] = true;
                    tetris_map[map_y][map_x] = true;
                    tetris_map[map_y][map_x + 1] = true;
                    tetris_map[map_y - 1][map_x - 1] = true;
                    break;
                case LEFT_90:
                    tetris_map[map_y][map_x] = true;
                    tetris_map[map_y][map_x + 1] = true;
                    tetris_map[map_y - 1][map_x + 1] = true;
                    tetris_map[map_y - 2][map_x + 1] = true;
                    break;
            } break;

        case 7: //reverse L block
            switch(rotation)
            {
                case NO_ROTATION:
                    tetris_map[map_y][map_x - 1] = true;
                    tetris_map[map_y - 1][map_x - 1] = true;
                    tetris_map[map_y - 1][map_x] = true;
                    tetris_map[map_y - 1][map_x + 1] = true;
                    break;
                case RIGHT_90:
                    tetris_map[map_y][map_x] = true;
                    tetris_map[map_y][map_x + 1] = true;
                    tetris_map[map_y - 1][map_x] = true;
                    tetris_map[map_y - 2][map_x] = true;
                    break;
                case UPSIDE_DOWN:
                    tetris_map[map_y][map_x - 1] = true;
                    tetris_map[map_y][map_x] = true;
                    tetris_map[map_y][map_x + 1] = true;
                    tetris_map[map_y - 1][map_x + 1] = true;
                    break;
                case LEFT_90:
                    tetris_map[map_y][map_x + 1] = true;
                    tetris_map[map_y - 1][map_x + 1] = true;
                    tetris_map[map_y - 2][map_x] = true;
                    tetris_map[map_y - 2][map_x + 1] = true;
                    break;
            } break;

        case 8: //4x1 long block
            switch(rotation)
            {
                case NO_ROTATION:
                case UPSIDE_DOWN:
                    tetris_map[map_y][map_x - 1] = true;
                    tetris_map[map_y][map_x] = true;
                    tetris_map[map_y][map_x + 1] = true;
                    tetris_map[map_y][map_x + 2] = true;
                    break;
                case RIGHT_90:
                case LEFT_90:
                    tetris_map[map_y][map_x] = true;
                    tetris_map[map_y - 1][map_x] = true;
                    tetris_map[map_y - 2][map_x] = true;
                    tetris_map[map_y - 3][map_x] = true;
                    break;
            } break;
    }
}

int tetris_check_row_completion(short int* score_multiplier, int score, short int speed, short int next_id)
{
    short int consecutive_rows = 1;
    short int starting_row = -1;
    bool completed_row;
    for(int row = 0; row < TETRIS_MAP_HEIGHT; row++)
    {
        completed_row = 1;
        for(int col = 0; col < TETRIS_MAP_WIDTH; col++)
        {
            if(!tetris_map[row][col])
            {
                completed_row = 0;
                break;
            }
        }

        if(starting_row != -1)
        {
            if(completed_row)
                consecutive_rows += 1;
            else
                break;
        }
        else if(completed_row)
            starting_row = row;
    }

    tetris_draw_row_deletion(starting_row, consecutive_rows, score, speed, next_id);
    
    if(starting_row == -1)
    {
        *score_multiplier = 0;
        return 0;
    }

    (*score_multiplier)++;
    switch(consecutive_rows)
    {
        case 1:
            return (*score_multiplier) * 100;
        case 2:
            return (*score_multiplier) * 300;
        case 3:
            return (*score_multiplier) * 600;
        case 4:
            return (*score_multiplier) * 1000;
    }
    return 0;
}

void tetris_run()
{
    int score, speed_limit;
    short int block_id, block_x, block_y;
    short int next_id, next_x, next_y;
    short int speed, ticks_till_fall, score_multiplier;
    block_rotation rotation, next_rotation;

    while(true)
    {
        //initialize variables
        score = 0, speed = 1, speed_limit = 2000, score_multiplier = 0;
        ticks_till_fall = TETRIS_MAX_SPEED + 1 - speed;
        block_id = rand() % TETRIS_NUMBER_OF_BLOCKS;
        next_id = rand() % TETRIS_NUMBER_OF_BLOCKS;
        block_x = TETRIS_MAP_WIDTH / 2 - 1;
        block_y = TETRIS_MAP_HEIGHT - 1;
        rotation = NO_ROTATION;
        next_x = block_x, next_y = block_y, next_rotation = rotation;
        memset(tetris_map, 0, sizeof(tetris_map));
        
        tetris_start_screen();

        //wait for button press to start the game
        esp_light_sleep_start();

        //main game loop
        while(true)
        {
            u8g2_ClearBuffer(&u8g2);

            //process user inupt
            if(gpio_get_level(DOWN_BUTTON))
                next_y = block_y - 1;
            if(gpio_get_level(LEFT_BUTTON))
                next_x = block_x - 1;
            if(gpio_get_level(RIGHT_BUTTON))
                next_x = block_x + 1;
            if(gpio_get_level(UP_BUTTON))
                switch(rotation)
                {
                    case NO_ROTATION:
                        next_rotation = RIGHT_90; break;
                    case RIGHT_90:
                        next_rotation = UPSIDE_DOWN; break;
                    case UPSIDE_DOWN:
                        next_rotation = LEFT_90; break;
                    case LEFT_90:
                        next_rotation = NO_ROTATION; break;
                }

            if(block_id == -1)
            {
                block_id = next_id;
                next_id = rand() % TETRIS_NUMBER_OF_BLOCKS;
                block_x = TETRIS_MAP_WIDTH / 2 - 1;
                block_y = TETRIS_MAP_HEIGHT - 1;
                rotation = NO_ROTATION;
                next_x = block_x, next_y = block_y, next_rotation = rotation;
                if(!tetris_block_fits(block_x, block_y, block_id, rotation))
                {
                    break;
                }
            }

            ticks_till_fall--;
            if(ticks_till_fall == 0)
            {
                if(score >= speed_limit && speed_limit != -1)
                {
                    speed++;
                    switch(speed)
                    {
                        case 2:
                            speed_limit = 4000; break;
                        case 3:
                            speed_limit = 10000; break;
                        case 4:
                            speed_limit = 20000; break;
                        case 5:
                            speed_limit = -1; break;
                    }
                }
                ticks_till_fall = TETRIS_MAX_SPEED + 1 - speed;
                if(next_y == block_y)
                    next_y--;
            }

            if(next_x != block_x)
                if(tetris_block_fits(next_x, block_y, block_id, rotation))
                    block_x = next_x;
            if(next_rotation != rotation)
                if(tetris_block_fits(block_x, block_y, block_id, next_rotation))
                    rotation = next_rotation;
            if(next_y < block_y)
            {
                if(tetris_block_fits(block_x, next_y, block_id, rotation))
                    block_y = next_y;
                else
                {
                    tetris_deactivate_block(block_x, block_y, block_id, rotation);
                    block_y = -1, block_x = -1, block_id = -1;
                    next_x = -1, next_y = -1;
                }
            }
            //rollback all unsuccessful states
            next_x = block_x, next_y = block_y, next_rotation = rotation;

            //render eveything
            tetris_draw_active_block(block_x, block_y, block_id, rotation);
            tetris_draw_background(score, speed, next_id);
            tetris_draw_frame();
            tetris_draw_blocks();
            u8g2_SendBuffer(&u8g2);

            //check for completed rows
            if(block_id == -1)
                score += tetris_check_row_completion(&score_multiplier, score, speed, next_id);
            
        }

        tetris_end_screen(score);

        //wait for exit the game or play again button press
        esp_light_sleep_start();
        if(!gpio_get_level(LEFT_BUTTON))
            break;
    }
}

void tetris_draw_minimap(short int x_offset, short int y_offset, short int block_size)
{
    bool minimap[10][10];
    memset(minimap, 0, sizeof(minimap));
    memset(minimap[0], 1, sizeof(minimap[0]));
    memset(minimap[1], 1, sizeof(minimap[1]));
    memset(minimap[2], 1, sizeof(minimap[2]));
    minimap[0][3] = false; minimap[0][4] = false;
    minimap[1][8] = false; minimap[2][1] = false;
    minimap[3][3] = true; minimap[3][4] = true; minimap[3][5] = true;
    minimap[3][6] = true; minimap[3][7] = true; minimap[3][8] = true;
    minimap[3][9] = true; minimap[4][6] = true; minimap[4][7] = true;
    minimap[4][8] = true; minimap[4][9] = true; minimap[5][7] = true;
    minimap[5][8] = true; minimap[5][9] = true; minimap[6][9] = true;
    minimap[7][9] = true; minimap[7][3] = true; minimap[8][2] = true;
    minimap[8][3] = true; minimap[8][4] = true;

    for(int row = 0; row < 10; row++)
    {
        for(int col = 0; col < 10; col++)
        {
            if(!minimap[row][col])
                continue;
            u8g2_DrawBox(&u8g2, x_offset + col*block_size,
                y_offset + 9*block_size - row * block_size,
            block_size, block_size);
        }
    }
}

void tetris_draw_left_frame()
{
    tetris_draw_minimap(23, 21, 2);
}

void tetris_draw_middle_frame()
{
    tetris_draw_minimap(51, 17, 3);
}

void tetris_draw_right_frame()
{
    tetris_draw_minimap(89, 21, 2);
}