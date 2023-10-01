#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <stdio.h>
#include <fonts.h>
#include <graphics.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <rom/ets_sys.h>

#define BLACK rgbToColour(0, 0, 0)
#define WHITE rgbToColour(255, 255, 255)
#define RED rgbToColour(255, 0, 0)

struct SnakeSegment
{
    int x;
    int y;
    int direction;
};
typedef struct SnakeSegment SnakeSegment;

struct Fruit
{
    int x;
    int y;
};
typedef struct Fruit Fruit;


void print_instructions();
void print_score(unsigned int score);
void print_final_score(unsigned int score);
bool is_left_button_press();
bool is_right_button_press();
void turn_clockwise();
void turn_counter_clockwise();
void init_snake();
void draw_snake();
void init_fruit();
void draw_fruit();
void move_snake();

/**
 * Handles the left and right button presses to turn the snake.
 */
static void IRAM_ATTR gpio_isr_snake_controller_handler(void *arg) {
    uint32_t gpio_num = (uint32_t) arg;
    int val = gpio_get_level(gpio_num);
    static uint64_t last_key_time;
    uint64_t time = esp_timer_get_time();
    uint64_t time_since = time - last_key_time;
    ets_printf("gpio_isr_handler %d %d %lld\n", gpio_num, val, time_since);

    // Debounce inputs (15 ms for particularly sticky keys)
    if (time_since > 150000 && val == 0) {
        if (gpio_num == GPIO_NUM_0) {
            turn_counter_clockwise();
        } else if (gpio_num == GPIO_NUM_35) {
            turn_clockwise();
        }
    }
    last_key_time = time;
}

// Init consts
const int PIXELS_PER_SEGMENT = 15;
const int SCORE_PER_FRUIT = 1;
const int BOUNDING_BOX_WIDTH = 135;
const int BOUNDING_BOX_HEIGHT = 240 - 15;
const int X_LIMIT = 9; // 135 / 15
const int Y_LIMIT = 15; // (240 - 15) / 15
const int SNAKE_SEGMENT_LIMIT = X_LIMIT * Y_LIMIT; // 135

SnakeSegment snake[135];
Fruit fruit;

// Init globals

// N: 0, E: 1, S: 2, W: 3
int direction = 1;
int x = 0, y = 0;
int headIndex = 0;

void app_main() {
    // Set buttons as inputs
    gpio_set_direction(0, GPIO_MODE_INPUT);
    gpio_set_direction(35, GPIO_MODE_INPUT);

    // Install interrupt handler service
    gpio_install_isr_service(0);

    // Init graphics
    graphics_init();
    set_orientation(PORTRAIT);

    while (true) {
        unsigned int score = 0;
        bool game_over = false;
 
        // Set initial screen UI
        cls(0);
        print_instructions();
        flip_frame();

        // Wait for left button input
        while(gpio_get_level(0));
        uint64_t last_time = esp_timer_get_time();

        // Initialize snake
        init_snake();
        init_fruit();

        // Init control interrupt handlers
        gpio_set_intr_type(GPIO_NUM_0, GPIO_INTR_NEGEDGE);
        gpio_set_intr_type(GPIO_NUM_35, GPIO_INTR_NEGEDGE);
        gpio_isr_handler_add(GPIO_NUM_0, gpio_isr_snake_controller_handler, (void *) GPIO_NUM_0);
        gpio_isr_handler_add(GPIO_NUM_35, gpio_isr_snake_controller_handler, (void *) GPIO_NUM_35);

        // Game loop
        while (true) {
            printf("direction %d\n", snake[headIndex].direction);

            // Clear screen
            cls(0);

            // Move snake
            move_snake();
            SnakeSegment head = snake[headIndex];

            // Check collisions
            if (head.x < 0 || head.x > X_LIMIT || head.y < 0 || head.y > Y_LIMIT) {
                game_over = true;
            }
            if (head.x == fruit.x && head.y == fruit.y) {
                score++;
                init_fruit();
            }
            if (game_over) {
                // Remove control interrupt handlers
                gpio_isr_handler_remove(0);
                gpio_isr_handler_remove(35);                
                break;
            }

            // Draw snake
            draw_snake();
            draw_fruit();

            print_score(score);
            flip_frame();
            vTaskDelay(50);
        }

        // Game over
        cls(0);
        print_final_score(score);
        flip_frame();
        vTaskDelay(400);
    }
}


void print_instructions() {
    setFont(FONT_DEJAVU18);
    draw_rectangle(0, 0, display_width, display_height, BLACK);
    print_xy("Instructions", 0, 0);
    setFont(FONT_UBUNTU16);
    const int N_INSTRUCTIONS = 6;
    char *instructions[] = {
        "Press the left and",
        "right buttons to",
        "move the snake",
        "\n",
        "Press the left",
        "button to begin!"
    };
    for (int i = 0; i < N_INSTRUCTIONS; i++) {
        print_xy(instructions[i], 0, 34 + (i * 16));
    }
}

void print_score(unsigned int score) {
    setFont(FONT_SMALL);
    char scoreStr[20];
    sprintf(scoreStr, "Score: %d", score);
    draw_rectangle(0, 0, display_height, 10, BLACK);
    print_xy(scoreStr, 2, 2);
}

void print_final_score(unsigned int score) {
    setFont(FONT_DEJAVU24);
    char scoreStr[20];
    sprintf(scoreStr, "%d", score);
    print_xy("Score:", 0, 0);
    print_xy(scoreStr, 0, 24);
}

bool is_left_button_press() {
    return gpio_get_level(0) == 0;
}

bool is_right_button_press() {
    return gpio_get_level(35) == 0;
}

void turn_clockwise() {
    snake[headIndex].direction = (snake[headIndex].direction + 1) % 4;
}

void turn_counter_clockwise() {
    snake[headIndex].direction = (snake[headIndex].direction + 3) % 4;
}

void init_snake() {
    // Init segments as empty
    for (int i = 0; i < SNAKE_SEGMENT_LIMIT; i++) {
        snake[i].x = -1;
        snake[i].y = -1;
        snake[1].direction = -1;
    }
    // Init starting segment in center
    headIndex = 0;
    snake[0].x = (X_LIMIT / 2) - 1;
    snake[0].y = Y_LIMIT / 2;
    snake[0].direction = 1;
}

void draw_snake() {
    for (int i = 0; i < SNAKE_SEGMENT_LIMIT; i++) {
        if (snake[i].direction == -1) {
            // End of snake reached
            break;
        }
        // Draw segment with 1px border
        draw_rectangle(
            (snake[i].x * PIXELS_PER_SEGMENT) + 1,
            (snake[i].y * PIXELS_PER_SEGMENT) + 1,
            PIXELS_PER_SEGMENT - 2,
            PIXELS_PER_SEGMENT - 2,
            WHITE
        );
    }
}

void init_fruit() {
    fruit.x = rand() % (X_LIMIT - 1);
    fruit.y = rand() % (Y_LIMIT - 1);
}

void draw_fruit() {
    draw_rectangle(
        (fruit.x * PIXELS_PER_SEGMENT) + 1,
        (fruit.y * PIXELS_PER_SEGMENT) + 1,
        PIXELS_PER_SEGMENT - 2,
        PIXELS_PER_SEGMENT - 2,
        RED
    );
}

/**
 * Moves each snake segment.
 * Returns the index of the snake's head.
 */
void move_snake() {
    int i;
    for (i = 0; i < SNAKE_SEGMENT_LIMIT; i++) {
        switch (snake[i].direction) {
            // Not a part of snake
            case -1:
                headIndex = i - 1;
                return;
            // North
            case 0:
                snake[i].y--;
                break;
            // East
            case 1:
                snake[i].x++;
                break;
            // South
            case 2:
                snake[i].y++;
                break;
            // West
            case 3:
                snake[i].x--;
                break;
            default:
                headIndex = i - 1;
                return;
        }
    }
    headIndex = i;
}
