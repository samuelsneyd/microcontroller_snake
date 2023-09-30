#include <soc/uart_struct.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <stdio.h>
#include <fonts.h>
#include <graphics.h>
#include <string.h>
#include <stdbool.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define BLACK rgbToColour(0, 0, 0)
#define WHITE rgbToColour(255, 255, 255)

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

void app_main() {
    // Set buttons as inputs
    gpio_set_direction(0, GPIO_MODE_INPUT);
    gpio_set_direction(35, GPIO_MODE_INPUT);
    graphics_init();
    set_orientation(PORTRAIT);

    // Init consts
    const int PIXELS_PER_SQUARE = 5;
    const int SCORE_PER_FRUIT = 1;
    const int BOUNDING_BOX_WIDTH = 135;
    const int BOUNDING_BOX_HEIGHT = 240 - 20;
    const int X_LIMIT = 27; // 135 / 5
    const int Y_LIMIT = 44; // (240 - 20) / 5
    printf("display width: %i", display_width);
    printf("display height: %i", display_height);

    while (true) {
        float v = 100;
        unsigned int score = 0;
        bool score_updated = false;
        int n_blocks = 1;
        bool n_blocks_updated = false;

        // Set initial screen UI
        cls(0);
        print_instructions();
        flip_frame();

        printf("display width: %i", display_width);
        printf("display height: %i", display_height);


        // Wait for left button input
        while(gpio_get_level(0));
        uint64_t last_time = esp_timer_get_time();

        // Game loop
        while (true) {
            uint64_t time = esp_timer_get_time();
            float dt = (time - last_time) * 1e-6f;
            last_time = time;

            // Clear screen
            cls(0);

            // Draw snake

            // Check collisions

            // Check for button inputs            

            // Move snake

            print_score(score);
            flip_frame();
        }

        // Game over
        cls(0);
        print_final_score(score);
        flip_frame();
        vTaskDelay(400);
    }
}
