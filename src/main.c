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
#define GREEN rgbToColour(0, 255, 0)

void print_instructions();
void print_score();
void print_final_score();
bool is_left_button_press();
bool is_right_button_press();
void init_snake();
void move_snake();
void draw_snake();
void free_snake();
void print_snake();
void init_fruit();
void draw_fruit();
void check_collisions();

struct SnakeCell
{
    int x;
    int y;
    struct SnakeCell *next;
};
typedef struct SnakeCell SnakeCell;

struct Fruit
{
    int x;
    int y;
};
typedef struct Fruit Fruit;

const int PIXELS_PER_SEGMENT = 15;
const int SCORE_PER_FRUIT = 1;
const int BOUNDING_BOX_WIDTH = 135;
const int BOUNDING_BOX_HEIGHT = 240 - 15;
const int SCORE_PIXELS = 15;
const int X_LIMIT = 9; // 135 / 15
const int Y_LIMIT = 15; // (240 - 15) / 15
const int SNAKE_SEGMENT_LIMIT = X_LIMIT * Y_LIMIT; // 135

// Snake is a linked-list of cells
// snake_tail is the head of the list
// snake_head is the tail of the list
// cell -> cell -> cell -> cell -> NULL
// ^                       ^
// |                       |
// snake_tail              snake_head
SnakeCell *snake_tail = NULL;
SnakeCell *snake_head = NULL;

Fruit fruit;
int direction = 1; // N: 0, E: 1, S: 2, W: 3
int headIndex = 0;
unsigned int score = 0;
bool game_over = false;
bool turn_clockwise_flag = false;
bool turn_counter_clockwise_flag = false;
int speeds[] = { 50, 30, 20, 15 };
char *difficulty_names[] = { "easy", "medium", "hard", "expert" };
bool difficulty = 0;

/**
 * Handles the left and right button presses prompt a turn of the snake.
 */
static void IRAM_ATTR gpio_isr_snake_controller_handler(void *arg) {
    uint32_t gpio_num = (uint32_t) arg;
    int val = gpio_get_level(gpio_num);
    static uint64_t last_key_time;
    uint64_t time = esp_timer_get_time();
    uint64_t time_since = time - last_key_time;
    ets_printf("gpio_isr_handler %d %d %lld\n", gpio_num, val, time_since);

    // Debounce inputs
    if (time_since > 2000 && val == 0) {
        if (gpio_num == GPIO_NUM_0) {
            turn_counter_clockwise_flag = true;
        } else if (gpio_num == GPIO_NUM_35) {
            turn_clockwise_flag = true;
        }
    }
    last_key_time = time;
}

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
        score = 0;
        game_over = false;
        turn_clockwise_flag = false;
        turn_counter_clockwise_flag = false;
        direction = 2;
 
        // Set initial screen UI
        cls(0);
        print_instructions();
        flip_frame();

        // Wait for left button input
        while(gpio_get_level(0));

        // Initialize snake and fruit
        init_snake();
        init_fruit();

        // Init control interrupt handlers
        gpio_set_intr_type(GPIO_NUM_0, GPIO_INTR_NEGEDGE);
        gpio_set_intr_type(GPIO_NUM_35, GPIO_INTR_NEGEDGE);
        gpio_isr_handler_add(GPIO_NUM_0, gpio_isr_snake_controller_handler, (void *) GPIO_NUM_0);
        gpio_isr_handler_add(GPIO_NUM_35, gpio_isr_snake_controller_handler, (void *) GPIO_NUM_35);

        // Game loop
        while (true) {
            // Clear screen
            cls(0);

            // Turn snake if button pressed
            if (turn_clockwise_flag) {
                direction = (direction + 1) % 4;
                turn_clockwise_flag = false;
            }
            if (turn_counter_clockwise_flag) {
                direction = (direction + 3) % 4;
                turn_counter_clockwise_flag = false;
            }

            // Move snake
            move_snake();

            // Check collisions
            check_collisions();

            // Check game over
            if (game_over) {
                // Remove control interrupt handlers
                gpio_isr_handler_remove(0);
                gpio_isr_handler_remove(35);
                free_snake();
                break;
            }

            // Draw snake and fruit
            draw_snake();
            draw_fruit();

            // Draw score
            print_score(score);

            // Draw and wait
            flip_frame();
            vTaskDelay(speeds[difficulty]);
        }

        // Game over
        cls(0);
        print_final_score(score);
        flip_frame();
        vTaskDelay(400);
    }
}

/**
 * Prints the initial instructions before the game starts.
 */
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

/**
 * Draws the top score bar.
 */
void print_score() {
    setFont(FONT_SMALL);
    char scoreStr[20];
    sprintf(scoreStr, "Score: %d", score);
    draw_rectangle(0, 0, display_height, SCORE_PIXELS, BLACK);
    draw_line(0, SCORE_PIXELS, display_width, SCORE_PIXELS, WHITE);
    print_xy(scoreStr, 2, 2);
}

/**
 * Draws a screen for the final score.
 */
void print_final_score() {
    setFont(FONT_DEJAVU24);
    char scoreStr[20];
    sprintf(scoreStr, "%d", score);
    print_xy("Score:", 0, 0);
    print_xy(scoreStr, 0, 24);
}

/**
 * Initializes a new snake of size 1 in the center of the screen.
 */
void init_snake() {
    if (snake_tail != NULL) {
        free_snake();
    }
    SnakeCell *new_cell = (SnakeCell *) malloc(sizeof(SnakeCell));
    new_cell->x = (X_LIMIT / 2) - 1;
    new_cell->y = Y_LIMIT / 2;
    new_cell->next = NULL;
    snake_tail = new_cell;
    snake_head = new_cell;
}

/**
 * Draws snake from the tail to the head
 */
void draw_snake() {
    SnakeCell *current = snake_tail;
    while (current != NULL) {
        draw_rectangle(
            (current->x * PIXELS_PER_SEGMENT) + 1,
            (current->y * PIXELS_PER_SEGMENT) + 1 + SCORE_PIXELS,
            PIXELS_PER_SEGMENT - 2,
            PIXELS_PER_SEGMENT - 2,
            current->next == NULL ? GREEN : WHITE
        );
        current = current->next;
    }
}

/**
 * Adds a fruit at a random location.
 * The fruit is allowed to collide with the snake's body cells.
 */
void init_fruit() {
    fruit.x = rand() % (X_LIMIT - 1);
    fruit.y = rand() % (Y_LIMIT - 1);
}

/**
 * Draws the fruit on the screen.
 */
void draw_fruit() {
    draw_rectangle(
        (fruit.x * PIXELS_PER_SEGMENT) + 1,
        (fruit.y * PIXELS_PER_SEGMENT) + 1 + SCORE_PIXELS,
        PIXELS_PER_SEGMENT - 2,
        PIXELS_PER_SEGMENT - 2,
        RED
    );
}

/**
 * Moves each snake segment, tail -> head.
 */
void move_snake() {
    if (snake_tail == NULL || snake_head == NULL) {
        return;
    }
    SnakeCell *old_tail = snake_tail;
    int new_x = snake_head->x;
    int new_y = snake_head->y;

    if (direction == 0) {
        new_y--;
    } else if (direction == 1) {
        new_x++;
    } else if (direction == 2) {
        new_y++;
    } else if (direction == 3) {
        new_x--;
    }

    // Snake head out of bounds
    if (new_x < 0 || new_x >= X_LIMIT || new_y < 0 || new_y >= Y_LIMIT) {
        game_over = true;
        return;
    }

    // Add new cell for head
    SnakeCell* new_cell = (SnakeCell *) malloc(sizeof(SnakeCell));
    new_cell->x = new_x;
    new_cell->y = new_y;
    new_cell->next = NULL;
    snake_head->next = new_cell;
    snake_head = new_cell;

    if (new_x == fruit.x && new_y == fruit.y) {
        // Fruit eaten, add new fruit and update score
        init_fruit();
        score += SCORE_PER_FRUIT;
    } else {
        // Fruit not eaten, pop old tail
        snake_tail = old_tail->next;
        free(old_tail);
    }
}

/**
 * Deallocates all heap memory for the snake.
 */
void free_snake() {
    while (snake_tail != NULL) {
        SnakeCell* temp = snake_tail;
        snake_tail = snake_tail->next;
        free(temp);
    }
    snake_tail = NULL;
    snake_head = NULL;
}

/**
 * Prints out each cell in the snake for debugging.
 */
void print_snake() {
    int i = 0;
    SnakeCell* current = snake_tail;
    while (current != NULL) {
        printf("cell %d, p: %p, x: %d, y: %d\n", i, current, current->x, current->y);
        current = current->next;
        i++;
    }
    printf("\n");
}

/**
 * Checks if the snake head collides with any part of the body, O(n) time.
 */
void check_collisions() {
    SnakeCell *current = snake_tail;
    while (current != snake_head) {
        if (current->x == snake_head->x && current->y == snake_head->y) {
            game_over = true;
            return;
        }
        current = current->next;
    }
}
