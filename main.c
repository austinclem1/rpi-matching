#include <linux/gpio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define GPIO_CHARDEV_PATH "/dev/gpiochip0"

#define LED_PIN_LEFT 2
#define LED_PIN_MID 3
#define LED_PIN_RIGHT 4
#define BUTTON_PIN_LEFT 17
#define BUTTON_PIN_MID 27
#define BUTTON_PIN_RIGHT 22
#define BUZZER_PIN 12

#define DEBOUNCE_PERIOD_US 10000

#define DEFAULT_PLAYBACK_DELAY 1000

#define MAX_SEQUENCE_LEN 2000

#define NS_PER_SEC 1000000000

typedef enum {
    choice_left,
    choice_mid,
    choice_right,
    NUM_CHOICES,
} Choice;

typedef enum {
    event_button_down,
    event_button_up,
} EventType;

typedef struct {
    EventType type;
    Choice choice;
} InputEvent;

typedef enum {
    game_state_playback,
    game_state_input,
    game_state_game_over,
} GameState;

static const int ledPins[NUM_CHOICES] = { LED_PIN_LEFT, LED_PIN_MID, LED_PIN_RIGHT };
static const int buttonPins[NUM_CHOICES] = { BUTTON_PIN_LEFT, BUTTON_PIN_MID, BUTTON_PIN_RIGHT };
static const int freqs[NUM_CHOICES] = { 440, 550, 660 };

void startTone(Choice choice) {
    const int freq = freqs[choice];
    const int period_ns = NS_PER_SEC / freq;
    const int duty_cycle_ns = period_ns / 2;
}

void stopTone() {}

void turnOnLed(int fd, Choice choice) {
    struct gpio_v2_line_values values = { 0 };
    values.bits = -1;
    values.mask = 1 << choice;
    
    if (ioctl(fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
        fprintf(stderr, "Failed to set led line value to 1: %s\n", strerror(errno));
    }
}

void turnOffAllLeds(int fd) {
    struct gpio_v2_line_values values = { 0 };
    values.bits = 0;
    values.mask = 0b111;
    
    if (ioctl(fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
        fprintf(stderr, "Failed to set led line values to 0: %s\n", strerror(errno));
    }
}

bool pollInput(InputEvent *ev_out) {
    ev_out->type = event_button_down;
    ev_out->choice = choice_left;
    return true;
}

void clearInputEvents() {}

int initLeds() {
    int gpio_chardev_fd, lines_fd;
    struct gpio_v2_line_request request = { 0 };

    lines_fd = -1;
    
    gpio_chardev_fd = open(GPIO_CHARDEV_PATH, O_RDONLY);
    if (gpio_chardev_fd < 0) {
        fprintf(stderr, "Failed to open gpio device \"%s\": %s\n", GPIO_CHARDEV_PATH, strerror(errno));
        goto exit;
    }
    
    strncpy(request.consumer, "leds", GPIO_MAX_NAME_SIZE);
    request.offsets[0] = LED_PIN_LEFT;
    request.offsets[1] = LED_PIN_MID;
    request.offsets[2] = LED_PIN_RIGHT;
    request.num_lines = 3;
    request.config.flags = GPIO_V2_LINE_FLAG_ACTIVE_LOW | GPIO_V2_LINE_FLAG_OUTPUT;

    if (ioctl(gpio_chardev_fd, GPIO_V2_GET_LINE_IOCTL, &request) < 0) {
        fprintf(stderr, "Failed to get gpio line handle with GPIO_V2_GET_LINE_IOCTL: %s", strerror(errno));
        goto exit_close_gpio_chardev;
    }
    
    lines_fd = request.fd;

exit_close_gpio_chardev:
    if (close(gpio_chardev_fd) < 0) {
        fprintf(stderr, "Failed to close gpio device \"%s\": %s\n", GPIO_CHARDEV_PATH, strerror(errno));
    }

exit:
    return lines_fd;
}

int initButtons() {
    int gpio_chardev_fd, lines_fd;
    struct gpio_v2_line_request request = { 0 };

    lines_fd = -1;
    
    gpio_chardev_fd = open(GPIO_CHARDEV_PATH, O_RDONLY);
    if (gpio_chardev_fd < 0) {
        fprintf(stderr, "Failed to open gpio device \"%s\": %s\n", GPIO_CHARDEV_PATH, strerror(errno));
        goto exit;
    }
    
    strncpy(request.consumer, "buttons", GPIO_MAX_NAME_SIZE);
    request.offsets[0] = BUTTON_PIN_LEFT;
    request.offsets[1] = BUTTON_PIN_MID;
    request.offsets[2] = BUTTON_PIN_RIGHT;
    request.num_lines = 3;
    request.config.flags =
        GPIO_V2_LINE_FLAG_INPUT |
        GPIO_V2_LINE_FLAG_EDGE_FALLING |
        GPIO_V2_LINE_FLAG_EDGE_RISING;
    request.config.attrs[0].mask = 0b111;
    request.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_DEBOUNCE;
    request.config.attrs[0].attr.debounce_period_us = DEBOUNCE_PERIOD_US;
    request.config.num_attrs = 1;

    if (ioctl(gpio_chardev_fd, GPIO_V2_GET_LINE_IOCTL, &request) < 0) {
        fprintf(stderr, "Failed to get gpio line handle with GPIO_V2_GET_LINE_IOCTL: %s", strerror(errno));
        goto exit_close_gpio_chardev;
    }
    
    lines_fd = request.fd;

exit_close_gpio_chardev:
    if (close(gpio_chardev_fd) < 0) {
        fprintf(stderr, "Failed to close gpio device \"%s\": %s\n", GPIO_CHARDEV_PATH, strerror(errno));
    }

exit:
    return lines_fd;
}

time_t nanoTimestamp() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    return (ts.tv_sec * NS_PER_SEC) + ts.tv_nsec;
}

int main(int argc, char **argv) {
    int leds_fd, buttons_fd, buzzer_fd;
    time_t cur_time, last_time, elapsed_time;
    Choice sequence[MAX_SEQUENCE_LEN];
    int sequence_len;
    int cur_sequence_index;
    int playback_time_remaining;
    InputEvent input_event;
    bool waiting_for_button_release;
    GameState game_state;
    bool running;

    srand(time(NULL));

    leds_fd = initLeds();
    if (leds_fd < 0) goto exit;
    
    buttons_fd = initButtons();
    if (buttons_fd < 0) goto exit_close_leds_fd;
    
    buzzer_fd = initBuzzer();
    if (buzzer_fd < 0) goto exit_close_buttons_leds_fd;
    
    // initialize first element in random sequence, set mode to playback
    sequence_len = 0;
    sequence[sequence_len] = rand() % NUM_CHOICES;
    sequence_len++;

    // this will advance the sequence index to 0 after the first frame (because time
    // remaining is 0), then the first element tone + LED will turn on
    cur_sequence_index = -1;
    playback_time_remaining = 0;
    game_state = game_state_playback;
    
    running = true;
    last_time = nanoTimestamp();
    while (running) {
        cur_time = nanoTimestamp();
        elapsed_time = cur_time - last_time;
        last_time = cur_time;
        
        switch (game_state) {
        case game_state_playback:
            playback_time_remaining -= elapsed_time;
            if (playback_time_remaining <= 0) {
                cur_sequence_index++;
                if (cur_sequence_index >= sequence_len) {
                    stopTone();
                    turnOffAllLeds(leds_fd);
                    clearInputEvents();
                    waiting_for_button_release = false;
                    game_state = game_state_input;
                } else {
                    startTone(sequence[cur_sequence_index]);
                    turnOffAllLeds(leds_fd);
                    turnOnLed(leds_fd, sequence[cur_sequence_index]);
                    playback_time_remaining = DEFAULT_PLAYBACK_DELAY;
                }
            }
            break;
        case game_state_input:
            if (pollInput(&input_event)) {
                switch (input_event.type) {
                case event_button_down:
                    if (waiting_for_button_release) break;
                    if (input_event.choice == sequence[cur_sequence_index]) {
                        startTone(sequence[cur_sequence_index]);
                        turnOffAllLeds(leds_fd);
                        turnOnLed(leds_fd, sequence[cur_sequence_index]);
                        waiting_for_button_release = true;
                    } else {
                        game_state = game_state_game_over;
                    }
                    break;
                case event_button_up:
                    if (waiting_for_button_release && input_event.choice == sequence[cur_sequence_index]) {
                        stopTone();
                        turnOffAllLeds(leds_fd);
                        waiting_for_button_release = false;
                        cur_sequence_index++;
                        if (cur_sequence_index >= sequence_len) {
                            sequence[sequence_len] = rand() % NUM_CHOICES;
                            sequence_len++;
                            game_state = game_state_playback;
                        }
                    }
                    break;
                } 
            }
            break;
        case game_state_game_over:
            break;
        }
    }

exit_close_buzzer_buttons_leds_fd:
    if (close(buzzer_fd) < 0) {
        fprintf(stderr, "Failed to close gpio buzzer line file: %s\n", strerror(errno));
    }

exit_close_buttons_leds_fd:
    if (close(buttons_fd) < 0) {
        fprintf(stderr, "Failed to close gpio buttons line file: %s\n", strerror(errno));
    }

exit_close_leds_fd:
    if (close(leds_fd) < 0) {
        fprintf(stderr, "Failed to close gpio LEDs line file: %s\n", strerror(errno));
    }

exit:
    return 0;
}
