#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "zarqaa.h"

#define WIDTH 28
#define HEIGHT 12
#define MAX_SNAKE 100

struct Point {
    int x;
    int y;
};

void _start() {
    printf("\n====================================\n");
    printf("     ZarqaaOS Retro Snake Game!     \n");
    printf("====================================\n");
    printf("Controls: W (Up), S (Down), A (Left), D (Right), Q (Quit)\n");
    printf("Starting game in 2 seconds...\n\n");
    sleep_ms(2000);

    struct Point snake[MAX_SNAKE];
    int snake_len = 3;
    snake[0].x = 10; snake[0].y = 5;
    snake[1].x = 9;  snake[1].y = 5;
    snake[2].x = 8;  snake[2].y = 5;

    int dir_x = 1, dir_y = 0;
    int food_x = 18, food_y = 6;
    int score = 0;
    int game_over = 0;

    unsigned int rand_seed = 12345;

    while (!game_over) {
        // Handle input non-blockingly
        int key = pollkey();
        if (key > 0) {
            if (key == 'w' || key == 'W') {
                if (dir_y == 0) { dir_x = 0; dir_y = -1; }
            } else if (key == 's' || key == 'S') {
                if (dir_y == 0) { dir_x = 0; dir_y = 1; }
            } else if (key == 'a' || key == 'A') {
                if (dir_x == 0) { dir_x = -1; dir_y = 0; }
            } else if (key == 'd' || key == 'D') {
                if (dir_x == 0) { dir_x = 1; dir_y = 0; }
            } else if (key == 'q' || key == 'Q') {
                break;
            }
        }

        // Move body
        for (int i = snake_len - 1; i > 0; i--) {
            snake[i] = snake[i - 1];
        }
        snake[0].x += dir_x;
        snake[0].y += dir_y;

        // Check wall collisions
        if (snake[0].x <= 0 || snake[0].x >= WIDTH - 1 ||
            snake[0].y <= 0 || snake[0].y >= HEIGHT - 1) {
            game_over = 1;
            break;
        }

        // Check self collisions
        for (int i = 1; i < snake_len; i++) {
            if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
                game_over = 1;
                break;
            }
        }
        if (game_over) break;

        // Check food collision
        if (snake[0].x == food_x && snake[0].y == food_y) {
            score += 10;
            if (snake_len < MAX_SNAKE - 1) {
                snake[snake_len] = snake[snake_len - 1];
                snake_len++;
            }
            rand_seed = (rand_seed * 1103515245 + 12345) & 0x7fffffff;
            food_x = 1 + (rand_seed % (WIDTH - 2));
            rand_seed = (rand_seed * 1103515245 + 12345) & 0x7fffffff;
            food_y = 1 + (rand_seed % (HEIGHT - 2));
        }

        // Render board
        printf("\nScore: %d | Snake Length: %d\n", score, snake_len);
        for (int y = 0; y < HEIGHT; y++) {
            char row[WIDTH + 1];
            for (int x = 0; x < WIDTH; x++) {
                if (y == 0 || y == HEIGHT - 1 || x == 0 || x == WIDTH - 1) {
                    row[x] = '#';
                } else if (x == food_x && y == food_y) {
                    row[x] = '*';
                } else {
                    int is_snake = 0;
                    for (int i = 0; i < snake_len; i++) {
                        if (snake[i].x == x && snake[i].y == y) {
                            row[x] = (i == 0) ? 'O' : 'o';
                            is_snake = 1;
                            break;
                        }
                    }
                    if (!is_snake) row[x] = ' ';
                }
            }
            row[WIDTH] = '\0';
            printf("%s\n", row);
        }

        sleep_ms(250);
    }

    printf("\n************************************\n");
    printf("   GAME OVER! Final Score: %d\n", score);
    printf("************************************\n\n");

    exit();
}
