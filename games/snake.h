#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "driver/rtc_io.h"
#include "../main/globals.h"

#define MAP_WIDTH 20
#define MAP_HEIGHT 10

typedef struct snake_node
{
    struct snake_node* next;
    short int x;
    short int y;
    short int next_direction;
    bool eaten;
    
} snake_node;

typedef enum direction
{
    LEFT, DOWN, RIGHT, UP
} direction;

static bool snake_map[MAP_HEIGHT][MAP_WIDTH];

snake_node* snake_init()
{
    snake_node* snake_segment1 = (snake_node*)malloc(sizeof(snake_node));
    snake_node* snake_segment2 = (snake_node*)malloc(sizeof(snake_node));
    snake_node* snake_segment3 = (snake_node*)malloc(sizeof(snake_node));
    snake_node* snake_segment4 = (snake_node*)malloc(sizeof(snake_node));
    
    snake_segment1->x = 12; snake_segment1->y = 5; snake_segment1->eaten = false;
    snake_segment2->x = 11; snake_segment2->y = 5; snake_segment2->eaten = false;
    snake_segment3->x = 10; snake_segment3->y = 5; snake_segment3->eaten = false;
    snake_segment4->x = 9;  snake_segment4->y = 5; snake_segment4->eaten = false;

    snake_segment1->next = snake_segment2; snake_segment1->next_direction = LEFT; 
    snake_segment2->next = snake_segment3; snake_segment2->next_direction = LEFT;
    snake_segment3->next = snake_segment4; snake_segment3->next_direction = LEFT;
    snake_segment4->next = NULL;           snake_segment4->next_direction = LEFT;

    snake_map[5][9] = true;  snake_map[5][10] = true;
    snake_map[5][11] = true; snake_map[5][12] = true;

    return snake_segment1;
}

void snake_free_memory(snake_node* snake_head)
{
    snake_node* prev = snake_head;
    while(snake_head)
    {
        snake_head = snake_head->next;
        free(prev);
        prev = snake_head;
    }
}

snake_node* snake_add_segment(snake_node* snake_head, direction snake_direction)
{
    snake_node* new_head = (snake_node*)malloc(sizeof(snake_node));
    new_head->next = snake_head;
    new_head->x = snake_head->x;
    new_head->y = snake_head->y;
    new_head->eaten = false;
    snake_head = new_head;
            
    switch (snake_direction)
    {
    case LEFT:
        snake_head->x--;
        if(snake_head->x < 0)
            snake_head->x = MAP_WIDTH - 1;
        snake_head->next_direction = RIGHT;
        break;
    case DOWN:
        snake_head->y--;
        if(snake_head->y < 0)
            snake_head->y = MAP_HEIGHT - 1;
        snake_head->next_direction = UP;
        break;
    case UP:
        snake_head->y++;
        if(snake_head->y >= MAP_HEIGHT)
            snake_head->y = 0;
        snake_head->next_direction = DOWN;
        break;
    case RIGHT:
        snake_head->x++;
        if(snake_head->x >= MAP_WIDTH)
            snake_head->x = 0;
        snake_head->next_direction = LEFT;
        break;
    }
    snake_map[snake_head->y][snake_head->x] = true;
    
    return snake_head;
}

void snake_pop_last_segment(snake_node* snake_head)
{
    snake_node* curr = snake_head;
    snake_node* prev = snake_head;
    while(curr->next)
    {
        prev = curr;
        curr = curr->next;
    }
    snake_map[curr->y][curr->x] = false;
    free(curr);
    prev->next = NULL;
}

bool snake_apple_in_front(snake_node* snake_head, direction snake_direction, short int apple_x, short int apple_y)
{
    switch(snake_direction)
    {
        case LEFT:
            if(snake_head->y == apple_y &&
                (((snake_head->x - 1 + MAP_WIDTH) % MAP_WIDTH) == apple_x ||
                ((snake_head->x - 2 + MAP_WIDTH) % MAP_WIDTH) == apple_x))
                return true;
            else
                return false;
        case RIGHT:
            if(snake_head->y == apple_y &&
                (((snake_head->x+1) % MAP_WIDTH) == apple_x ||
                ((snake_head->x+2) % MAP_WIDTH) == apple_x))
                return true;
            else
                return false;
        case DOWN:
            if(snake_head->x == apple_x &&
                (((snake_head->y - 1 + MAP_HEIGHT) % MAP_HEIGHT) == apple_y ||
                ((snake_head->y - 2 + MAP_HEIGHT) % MAP_HEIGHT) == apple_y))
                return true;
            else
                return false;
        case UP:
            if(snake_head->x == apple_x &&
                (((snake_head->y+1) % MAP_HEIGHT) == apple_y ||
                ((snake_head->y+2) % MAP_HEIGHT) == apple_y))
                return true;
            else
                return false;
        default:
            return false;
    }
}

void snake_draw_snake(snake_node* snake_head, direction snake_direction)
{
    short int x_offset = (DISPLAY_WIDTH - 4*MAP_WIDTH) / 2 - 1;
    short int y_offset = 4;
    short int x_pos, y_pos; 

    //draw the middle part
    snake_node* curr = snake_head->next;
    direction prev_direction = snake_head->next_direction;
    while(curr->next)
    {
        x_pos = curr->x * 4;
        y_pos = curr->y * 4;

        bool orientation = true;
        if(prev_direction == DOWN || prev_direction == RIGHT)
            orientation = false;
        if(curr->next_direction != prev_direction && 
            (curr->next_direction == DOWN || curr->next_direction == RIGHT))
            orientation = !orientation;
        if(orientation)
        {
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 1, DISPLAY_HEIGHT - (y_offset + y_pos + 2));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 2, DISPLAY_HEIGHT - (y_offset + y_pos + 1));
        }
        else
        {
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 1, DISPLAY_HEIGHT - (y_offset + y_pos + 1));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 2, DISPLAY_HEIGHT - (y_offset + y_pos + 2));
        }

        if(curr->eaten)
        {
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 0, DISPLAY_HEIGHT - (y_offset + y_pos + 1));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 0, DISPLAY_HEIGHT - (y_offset + y_pos + 2));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 3, DISPLAY_HEIGHT - (y_offset + y_pos + 1));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 3, DISPLAY_HEIGHT - (y_offset + y_pos + 2));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 1, DISPLAY_HEIGHT - (y_offset + y_pos + 0));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 2, DISPLAY_HEIGHT - (y_offset + y_pos + 0));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 1, DISPLAY_HEIGHT - (y_offset + y_pos + 3));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 2, DISPLAY_HEIGHT - (y_offset + y_pos + 3));
        }

        switch(curr->next_direction)
        {
            case LEFT:
                x_pos -= 2; break;
            case RIGHT:
                x_pos += 2; break;
            case DOWN:
                y_pos -= 2; break;
            case UP:
                y_pos += 2; break;
        }
        u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 1 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
            DISPLAY_HEIGHT - (y_offset + (y_pos + 1 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
        u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 2 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
            DISPLAY_HEIGHT - (y_offset + (y_pos + 1 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
        u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 1 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
            DISPLAY_HEIGHT - (y_offset + (y_pos + 2 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
        u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 2 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
            DISPLAY_HEIGHT - (y_offset + (y_pos + 2 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));

        prev_direction = curr->next_direction;
        curr = curr->next;
    }

    //draw the tail
    x_pos = 4 * curr->x;
    y_pos = 4 * curr->y;
    switch(prev_direction)
    {
        case RIGHT:
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 1, DISPLAY_HEIGHT - (y_offset + y_pos + 1));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 2, DISPLAY_HEIGHT - (y_offset + y_pos + 1));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 1, DISPLAY_HEIGHT - (y_offset + y_pos + 2));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 3, DISPLAY_HEIGHT - (y_offset + y_pos + 1));
            break;
        case LEFT:
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 1, DISPLAY_HEIGHT - (y_offset + y_pos + 1));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 2, DISPLAY_HEIGHT - (y_offset + y_pos + 1));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 2, DISPLAY_HEIGHT - (y_offset + y_pos + 2));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 0, DISPLAY_HEIGHT - (y_offset + y_pos + 1));
            break;
        case UP:
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 1, DISPLAY_HEIGHT - (y_offset + y_pos + 1));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 2, DISPLAY_HEIGHT - (y_offset + y_pos + 1));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 2, DISPLAY_HEIGHT - (y_offset + y_pos + 2));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 2, DISPLAY_HEIGHT - (y_offset + y_pos + 3));
            break;
        case DOWN:
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 1, DISPLAY_HEIGHT - (y_offset + y_pos + 2));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 2, DISPLAY_HEIGHT - (y_offset + y_pos + 1));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 2, DISPLAY_HEIGHT - (y_offset + y_pos + 2));
            u8g2_DrawPixel(&u8g2, x_offset + x_pos + 2, DISPLAY_HEIGHT - (y_offset + y_pos + 0));
            break;
    }

    //draw head
    x_pos = snake_head->x * 4;
    y_pos = snake_head->y * 4;
    u8g2_DrawPixel(&u8g2, x_offset + x_pos + 1, DISPLAY_HEIGHT - (y_offset + y_pos + 1));
    u8g2_DrawPixel(&u8g2, x_offset + x_pos + 2, DISPLAY_HEIGHT - (y_offset + y_pos + 1));
    u8g2_DrawPixel(&u8g2, x_offset + x_pos + 1, DISPLAY_HEIGHT - (y_offset + y_pos + 2));
    u8g2_DrawPixel(&u8g2, x_offset + x_pos + 2, DISPLAY_HEIGHT - (y_offset + y_pos + 2));

    //draw neck and eye
    switch(snake_head->next_direction)
    {
        case RIGHT:
            x_pos += 2;
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 1 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 1 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 2 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 1 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 1 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 3 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 2 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 2 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_SetDrawColor(&u8g2, 0);
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 1 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 2 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_SetDrawColor(&u8g2, 1);
            break;
        case LEFT:
            x_pos -= 2;
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 1 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 1 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 2 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 1 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 1 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 2 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 2 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 3 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_SetDrawColor(&u8g2, 0);
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 2 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 2 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_SetDrawColor(&u8g2, 1);
            break;
        case DOWN:
            y_pos -= 2;
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 1 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 1 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 2 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 1 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 0 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 2 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 2 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 2 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_SetDrawColor(&u8g2, 0);
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 1 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 2 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_SetDrawColor(&u8g2, 1);
            break;
        case UP:
            y_pos += 2;
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 0 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 1 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 2 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 1 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 1 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 2 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 2 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 2 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_SetDrawColor(&u8g2, 0);
            u8g2_DrawPixel(&u8g2, x_offset + (x_pos + 1 + 4 * MAP_WIDTH)  % (4 * MAP_WIDTH),
                DISPLAY_HEIGHT - (y_offset + (y_pos + 1 + 4 * MAP_HEIGHT) % (4 * MAP_HEIGHT)));
            u8g2_SetDrawColor(&u8g2, 1);
            break;
    }
}

void snake_start_screen()
{
    u8g2_ClearBuffer(&u8g2);

    u8g2_SetFont(&u8g2, u8g2_font_logisoso32_tr);
    const char *title = "Snake";
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

void snake_end_screen(int score)
{
    u8g2_ClearBuffer(&u8g2);

    u8g2_SetFont(&u8g2, u8g2_font_helvB10_tr);
    const char *msg = (score > snake_highscore) ? "New High Score!" : "Game Over";
    int msg_x = (DISPLAY_WIDTH - u8g2_GetStrWidth(&u8g2, msg)) / 2 - 2;
    u8g2_DrawStr(&u8g2, msg_x, 16, msg);

    char buf[32];
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    snprintf(buf, sizeof(buf), "Score: %d", score);
    int score_x = (DISPLAY_WIDTH - u8g2_GetStrWidth(&u8g2, buf)) / 2;
    u8g2_DrawStr(&u8g2, score_x, 32, buf);

    if (score <= snake_highscore) {
        snprintf(buf, sizeof(buf), "Best: %d", snake_highscore);
        int best_x = (DISPLAY_WIDTH - u8g2_GetStrWidth(&u8g2, buf)) / 2;
        u8g2_DrawStr(&u8g2, best_x, 44, buf);
    }
    
    u8g2_SetFont(&u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(&u8g2, 5, 60, "Play Again");
    u8g2_DrawStr(&u8g2, 95, 60, "Exit");

    u8g2_SendBuffer(&u8g2);

    if (score > snake_highscore)
        snake_highscore = score;
}

bool snake_collision_check(snake_node* snake_head, direction snake_direction)
{
    int head_x = snake_head->x;
    int head_y = snake_head->y;
    switch(snake_direction)
    {
        case LEFT:
            head_x--; break;
        case RIGHT:
            head_x++; break;
        case UP:
            head_y++; break;
        case DOWN:
            head_y--; break;
    }

    return snake_map[head_y % MAP_HEIGHT][head_x % MAP_WIDTH];
}

void snake_draw_frame()
{
    short int x1 = (DISPLAY_WIDTH - 4*MAP_WIDTH - 4) / 2 - 1;
    short int x2 = x1 + 3 + 4 * MAP_WIDTH;
    short int y1 = 2;
    short int y2 = y1 + 3 + 4 * MAP_HEIGHT;
    u8g2_DrawLine(&u8g2, x1, DISPLAY_HEIGHT - y1 ,x1, DISPLAY_HEIGHT - y2);
    u8g2_DrawLine(&u8g2, x2, DISPLAY_HEIGHT - y1 ,x2, DISPLAY_HEIGHT - y2);
    u8g2_DrawLine(&u8g2, x1, DISPLAY_HEIGHT - y1 ,x2, DISPLAY_HEIGHT - y1);
    u8g2_DrawLine(&u8g2, x1, DISPLAY_HEIGHT - y2 ,x2, DISPLAY_HEIGHT - y2);
    u8g2_DrawLine(&u8g2, x1, DISPLAY_HEIGHT - (y2 + 2) ,x2, DISPLAY_HEIGHT - (y2 + 2));
}

void snake_draw_score(int score)
{
    char score_str[12] = "Score:0000";
    score_str[9]  = '0' + (score % 10);
    score_str[8]  = '0' + (score / 10) % 10;
    score_str[7]  = '0' + (score / 100) % 10;
    score_str[6]  = '0' + (score / 1000) % 10;
    u8g2_SetFont(&u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(&u8g2, 21, DISPLAY_HEIGHT - 48, score_str);
}

void snake_draw_animal(int x_map, int y_map, int animal_id)
{
    int x = (DISPLAY_WIDTH - 4*MAP_WIDTH) / 2 + x_map * 4;
    int y = 6 + y_map * 4; 
    switch(animal_id)
    {
        case 0: //lizard
            u8g2_DrawBox(&u8g2, x + 1, DISPLAY_HEIGHT - (y + 1), 5, 2);
            u8g2_DrawPixel(&u8g2, x - 1, DISPLAY_HEIGHT - y);
            u8g2_DrawPixel(&u8g2, x - 1, DISPLAY_HEIGHT - (y + 1));
            u8g2_DrawPixel(&u8g2, x, DISPLAY_HEIGHT - y);
            u8g2_DrawPixel(&u8g2, x, DISPLAY_HEIGHT - (y + 2));
            u8g2_DrawPixel(&u8g2, x + 1, DISPLAY_HEIGHT - (y - 1));
            u8g2_DrawPixel(&u8g2, x + 2, DISPLAY_HEIGHT - (y + 2));
            u8g2_DrawPixel(&u8g2, x + 4, DISPLAY_HEIGHT - (y - 1));
            u8g2_DrawPixel(&u8g2, x + 4, DISPLAY_HEIGHT - (y + 2));
            u8g2_DrawPixel(&u8g2, x + 6, DISPLAY_HEIGHT - y);
            break;
        case 1: //crab
            u8g2_DrawBox(&u8g2, x + 1, DISPLAY_HEIGHT - (y + 2), 4, 3);
            u8g2_DrawLine(&u8g2, x-1, DISPLAY_HEIGHT - (y-1), x-1, DISPLAY_HEIGHT - (y+1));
            u8g2_DrawLine(&u8g2, x+6, DISPLAY_HEIGHT - (y-1), x+6, DISPLAY_HEIGHT - (y+1));
            u8g2_DrawPixel(&u8g2, x, DISPLAY_HEIGHT - (y + 1));
            u8g2_DrawPixel(&u8g2, x + 1, DISPLAY_HEIGHT - (y - 1));
            u8g2_DrawPixel(&u8g2, x + 4, DISPLAY_HEIGHT - (y - 1));
            u8g2_DrawPixel(&u8g2, x + 5, DISPLAY_HEIGHT - (y + 1));
            break;
        case 2: //fish
            u8g2_DrawBox(&u8g2, x + 3, DISPLAY_HEIGHT - (y + 1), 3, 2);
            u8g2_DrawBox(&u8g2, x - 1, DISPLAY_HEIGHT - (y + 2), 2, 2);
            u8g2_DrawPixel(&u8g2, x + 1, DISPLAY_HEIGHT - y);
            u8g2_DrawPixel(&u8g2, x + 2, DISPLAY_HEIGHT - y);
            u8g2_DrawPixel(&u8g2, x + 3, DISPLAY_HEIGHT - (y - 1));
            u8g2_DrawPixel(&u8g2, x + 4, DISPLAY_HEIGHT - (y + 2));
            u8g2_DrawPixel(&u8g2, x + 5, DISPLAY_HEIGHT - (y - 1));
            u8g2_DrawPixel(&u8g2, x + 6, DISPLAY_HEIGHT - y);
            break;
    }
}

void snake_draw_animal_timer(int animal_timer)
{
    if(animal_timer <= 0) return;
    char animal_time_str[3] = "00";
    animal_time_str[0] += animal_timer / 10;
    animal_time_str[1] += animal_timer % 10;
    u8g2_SetFont(&u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(&u8g2, 96, DISPLAY_HEIGHT - 48, animal_time_str);
}

void snake_generate_apple(short int *apple_x, short int *apple_y)
{
    short int apple_pos = rand() % (MAP_HEIGHT * MAP_WIDTH);
    for(short int i = 0; i < MAP_HEIGHT * MAP_WIDTH; i++)
    {
        short int x = (apple_pos + i) % MAP_WIDTH;
        short int y = ((apple_pos + i)  / MAP_WIDTH) % MAP_HEIGHT;
        if(!snake_map[y][x])
        {
            *apple_x = x;
            *apple_y = y;
            return;
        }
    }
    *apple_x = -1;
    *apple_y = -1;
}

void snake_generate_animal(short int *animal_x, short int *animal_y)
{
    short int animal_pos = rand() % (MAP_HEIGHT * MAP_WIDTH);
    for(short int i = 0; i < MAP_HEIGHT * MAP_WIDTH; i++)
    {
        short int x = (animal_pos + i) % MAP_WIDTH;
        short int y = ((animal_pos + i)  / MAP_WIDTH) % MAP_HEIGHT;
        if(x != (MAP_WIDTH - 1) && !snake_map[y][x] && !snake_map[y][x+1])
        {
            *animal_x = x;
            *animal_y = y;
            return;
        }
    }
    *animal_x = -1;
    *animal_y = -1;
}

void snake_draw_apple(short int x_map, short int y_map)
{
    if(x_map == -1 || y_map == -1)
        return;

    short int x = (DISPLAY_WIDTH - 4*MAP_WIDTH) / 2 + x_map * 4;
    short int y =  6 + y_map * 4;
    u8g2_DrawPixel(&u8g2, x - 1, DISPLAY_HEIGHT - y);
    u8g2_DrawPixel(&u8g2, x + 1, DISPLAY_HEIGHT - y);
    u8g2_DrawPixel(&u8g2, x, DISPLAY_HEIGHT - (y - 1));
    u8g2_DrawPixel(&u8g2, x, DISPLAY_HEIGHT - (y + 1));
}

void snake_open_mouth(snake_node* snake_head, direction snake_direction)
{
    short int x = (DISPLAY_WIDTH - 4*MAP_WIDTH) / 2 + snake_head->x * 4;
    short int y = 5 + snake_head->y * 4;
    switch(snake_direction)
    {
        case LEFT:
            u8g2_DrawPixel(&u8g2, x, DISPLAY_HEIGHT - (y - 1));
            u8g2_DrawPixel(&u8g2, x, DISPLAY_HEIGHT - (y + 2));
            u8g2_SetDrawColor(&u8g2, 0);
            u8g2_DrawPixel(&u8g2, x, DISPLAY_HEIGHT - y);
            u8g2_DrawPixel(&u8g2, x, DISPLAY_HEIGHT - (y + 1));
            u8g2_SetDrawColor(&u8g2, 1);
            break;
        case RIGHT:
            u8g2_DrawPixel(&u8g2, x + 1, DISPLAY_HEIGHT - (y - 1));
            u8g2_DrawPixel(&u8g2, x + 1, DISPLAY_HEIGHT - (y + 2));
            u8g2_SetDrawColor(&u8g2, 0);
            u8g2_DrawPixel(&u8g2, x + 1, DISPLAY_HEIGHT - y);
            u8g2_DrawPixel(&u8g2, x + 1, DISPLAY_HEIGHT - (y + 1));
            u8g2_SetDrawColor(&u8g2, 1);
            break;
        case DOWN:
            u8g2_DrawPixel(&u8g2, x - 1, DISPLAY_HEIGHT - y);
            u8g2_DrawPixel(&u8g2, x + 2, DISPLAY_HEIGHT - y);
            u8g2_SetDrawColor(&u8g2, 0);
            u8g2_DrawPixel(&u8g2, x, DISPLAY_HEIGHT - y);
            u8g2_DrawPixel(&u8g2, x + 1, DISPLAY_HEIGHT - y);
            u8g2_SetDrawColor(&u8g2, 1);
            break;
        case UP:
            u8g2_DrawPixel(&u8g2, x - 1, DISPLAY_HEIGHT - (y + 1));
            u8g2_DrawPixel(&u8g2, x + 2, DISPLAY_HEIGHT - (y + 1));
            u8g2_SetDrawColor(&u8g2, 0);
            u8g2_DrawPixel(&u8g2, x, DISPLAY_HEIGHT - (y + 1));
            u8g2_DrawPixel(&u8g2, x + 1, DISPLAY_HEIGHT - (y + 1));
            u8g2_SetDrawColor(&u8g2, 1);
            break;
    }
}

void snake_death_scene(snake_node* snake_head, direction snake_direction, int score)
{
    for(int i = 0; i < 9; i++)
    {
        u8g2_ClearBuffer(&u8g2);
        snake_draw_frame();
        snake_draw_score(score);
        if(i % 2)
            snake_draw_snake(snake_head, snake_direction);
        u8g2_SendBuffer(&u8g2);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void snake_run()
{
    direction snake_direction;
    snake_node* snake_head = NULL;
    int score;
    short int apple_x, apple_y, apples_till_animal,
        animal_timer, animal_id, animal_x, animal_y;
        
    while(true)
    {
        //initialize variables
        snake_direction = RIGHT;
        snake_head = snake_init();
        memset(snake_map, 0, sizeof(snake_map));
        apple_x = -1; apple_y = -1, animal_x = -1, animal_y = -1;
        apples_till_animal = 4, animal_timer = 0, score = 0;
        animal_id = rand() % 3;
        snake_start_screen();

        //check for any button press to start
        esp_light_sleep_start();
    
        //play loop
        while(true)
        {
            u8g2_ClearBuffer(&u8g2);

            if(gpio_get_level(LEFT_BUTTON) && snake_direction != RIGHT)
                snake_direction = LEFT;
            if(gpio_get_level(DOWN_BUTTON) && snake_direction != UP)
                snake_direction = DOWN;
            if(gpio_get_level(RIGHT_BUTTON) && snake_direction != LEFT)
                snake_direction = RIGHT;
            if(gpio_get_level(UP_BUTTON) && snake_direction != DOWN)
                snake_direction = UP;

            if(snake_collision_check(snake_head, snake_direction))
            {
                snake_death_scene(snake_head, snake_direction, score);
                break;
            }

            snake_head = snake_add_segment(snake_head, snake_direction);

            //check if apple is eaten
            if(snake_head->x == apple_x && snake_head->y == apple_y)
            {
                score += 7;
                apple_x = -1;
                apple_y = -1;
                snake_head->eaten = true;
                apples_till_animal--;
            }
            else
                snake_pop_last_segment(snake_head);

            //generate new apple if previous one got eaten
            if(apple_x == -1 || apple_y == -1)
                snake_generate_apple(&apple_x, &apple_y);

            //check if animal is eaten
            if(animal_timer > 0 && animal_y == snake_head->y &&
                (animal_x == snake_head->x || (animal_x + 1) == snake_head->x))
            {
                score += animal_timer;
                animal_timer = 0;
                animal_x = -1; animal_y = -1;
                snake_head->eaten = true;
            }
            if(animal_timer > 0)
                animal_timer--;

            //generate animal on every 5th apple
            if(apples_till_animal == 0)
            {
                apples_till_animal = 5;
                animal_timer = 20;
                animal_id = rand() % 3;
                if(apple_x != -1 && apple_y != -1)
                {
                    snake_map[apple_y][apple_x] = true;
                    snake_generate_animal(&animal_x, &animal_y);
                    snake_map[apple_y][apple_x] = false;
                }
                else
                    snake_generate_animal(&animal_x, &animal_y);
            }

            //render everything
            snake_draw_snake(snake_head, snake_direction);
            if(snake_apple_in_front(snake_head, snake_direction, apple_x, apple_y))
                snake_open_mouth(snake_head, snake_direction);
            snake_draw_frame();
            snake_draw_score(score);
            snake_draw_apple(apple_x, apple_y);
            if(animal_x != -1 && animal_y != -1 && animal_timer > 0)
            {
                snake_draw_animal_timer(animal_timer);
                snake_draw_animal(animal_x, animal_y, animal_id);
            }

            u8g2_SendBuffer(&u8g2);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        snake_end_screen(score);
        snake_free_memory(snake_head);

        //wait for play again or exit button press
        esp_light_sleep_start();
        if(!gpio_get_level(LEFT_BUTTON))
            break; //exit game
    }
}

void snake_draw_left_frame()
{
    snake_node segment_1, segment_2, segment_3, segment_4, segment_5, segment_6, 
    segment_7, segment_8, segment_9, segment_10, segment_11, segment_12, segment_13, 
    segment_14, segment_15, segment_16;

    short int head_x = 3, head_y = 9;
    segment_1.x = head_x;      segment_1.y = head_y;
    segment_2.x = head_x - 1;  segment_2.y = head_y;
    segment_3.x = head_x - 2;  segment_3.y = head_y;
    segment_4.x = head_x - 3;  segment_4.y = head_y;
    segment_5.x = head_x - 3;  segment_5.y = head_y - 1;
    segment_6.x = head_x - 3;  segment_6.y = head_y - 2;
    segment_7.x = head_x - 2;  segment_7.y = head_y - 2;
    segment_8.x = head_x - 1;  segment_8.y = head_y - 2;
    segment_9.x = head_x;      segment_9.y = head_y - 2;
    segment_10.x = head_x + 1; segment_10.y = head_y - 2;
    segment_11.x = head_x + 1; segment_11.y = head_y - 3;
    segment_12.x = head_x + 1; segment_12.y = head_y - 4;
    segment_13.x = head_x;     segment_13.y = head_y - 4;
    segment_14.x = head_x - 1; segment_14.y = head_y - 4;
    segment_15.x = head_x - 2; segment_15.y = head_y - 4;
    segment_16.x = head_x - 3; segment_16.y = head_y - 4;

    segment_2.next_direction = LEFT;  segment_3.next_direction = LEFT;
    segment_4.next_direction = DOWN;  segment_5.next_direction = DOWN;
    segment_6.next_direction = RIGHT; segment_7.next_direction = RIGHT;
    segment_8.next_direction = RIGHT; segment_9.next_direction = RIGHT;
    segment_10.next_direction = DOWN; segment_11.next_direction = DOWN;
    segment_12.next_direction = LEFT; segment_13.next_direction = LEFT;
    segment_14.next_direction = LEFT; segment_15.next_direction = LEFT;
    segment_16.next_direction = LEFT; segment_1.next_direction = LEFT;

    segment_1.eaten = false; segment_2.eaten = false; segment_3.eaten = false;
    segment_4.eaten = false; segment_5.eaten = false; segment_6.eaten = false;
    segment_7.eaten = true;  segment_8.eaten = false; segment_9.eaten = false;
    segment_10.eaten = false;segment_11.eaten = false;segment_12.eaten = false;
    segment_13.eaten = false;segment_14.eaten = false;segment_15.eaten = false;
    segment_16.eaten = false;

    segment_1.next = &segment_2; segment_2.next = &segment_3; segment_3.next = &segment_4;
    segment_4.next = &segment_5; segment_5.next = &segment_6; segment_6.next = &segment_7;
    segment_7.next = &segment_8; segment_8.next = &segment_9; segment_9.next = &segment_10;
    segment_10.next = &segment_11; segment_11.next = &segment_12; segment_12.next = &segment_13;
    segment_13.next = &segment_14; segment_14.next = &segment_15; segment_15.next = &segment_16;
    segment_16.next = NULL;
    snake_draw_snake(&segment_1, RIGHT);
    snake_open_mouth(&segment_1, RIGHT);
    snake_draw_apple(head_x + 1, head_y);
}

void snake_draw_middle_frame()
{

}

void snake_draw_right_frame()
{
    snake_node segment_1, segment_2, segment_3, segment_4, segment_5, segment_6, 
    segment_7, segment_8, segment_9, segment_10, segment_11, segment_12, segment_13, 
    segment_14, segment_15, segment_16;

    short int head_x = 18, head_y = 8;
    segment_1.x = head_x;      segment_1.y = head_y;
    segment_2.x = head_x - 1;  segment_2.y = head_y;
    segment_3.x = head_x - 2;  segment_3.y = head_y;
    segment_4.x = head_x - 3;  segment_4.y = head_y;
    segment_5.x = head_x - 3;  segment_5.y = head_y - 1;
    segment_6.x = head_x - 3;  segment_6.y = head_y - 2;
    segment_7.x = head_x - 2;  segment_7.y = head_y - 2;
    segment_8.x = head_x - 1;  segment_8.y = head_y - 2;
    segment_9.x = head_x;      segment_9.y = head_y - 2;
    segment_10.x = head_x + 1; segment_10.y = head_y - 2;
    segment_11.x = head_x + 1; segment_11.y = head_y - 3;
    segment_12.x = head_x + 1; segment_12.y = head_y - 4;
    segment_13.x = head_x;     segment_13.y = head_y - 4;
    segment_14.x = head_x - 1; segment_14.y = head_y - 4;
    segment_15.x = head_x - 2; segment_15.y = head_y - 4;
    segment_16.x = head_x - 3; segment_16.y = head_y - 4;

    segment_2.next_direction = LEFT;  segment_3.next_direction = LEFT;
    segment_4.next_direction = DOWN;  segment_5.next_direction = DOWN;
    segment_6.next_direction = RIGHT; segment_7.next_direction = RIGHT;
    segment_8.next_direction = RIGHT; segment_9.next_direction = RIGHT;
    segment_10.next_direction = DOWN; segment_11.next_direction = DOWN;
    segment_12.next_direction = LEFT; segment_13.next_direction = LEFT;
    segment_14.next_direction = LEFT; segment_15.next_direction = LEFT;
    segment_16.next_direction = LEFT; segment_1.next_direction = LEFT;

    segment_1.eaten = false; segment_2.eaten = false; segment_3.eaten = false;
    segment_4.eaten = false; segment_5.eaten = false; segment_6.eaten = false;
    segment_7.eaten = true;  segment_8.eaten = false; segment_9.eaten = false;
    segment_10.eaten = false;segment_11.eaten = false;segment_12.eaten = false;
    segment_13.eaten = false;segment_14.eaten = false;segment_15.eaten = false;
    segment_16.eaten = false;

    segment_1.next = &segment_2; segment_2.next = &segment_3; segment_3.next = &segment_4;
    segment_4.next = &segment_5; segment_5.next = &segment_6; segment_6.next = &segment_7;
    segment_7.next = &segment_8; segment_8.next = &segment_9; segment_9.next = &segment_10;
    segment_10.next = &segment_11; segment_11.next = &segment_12; segment_12.next = &segment_13;
    segment_13.next = &segment_14; segment_14.next = &segment_15; segment_15.next = &segment_16;
    segment_16.next = NULL;
    snake_draw_snake(&segment_1, RIGHT);
    snake_open_mouth(&segment_1, RIGHT);
    snake_draw_apple(head_x + 1, head_y);
    snake_draw_animal(head_x - 2, head_y - 3, 2);
}