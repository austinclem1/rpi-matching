#include <linux/gpio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define GPIO_CHIP_PATH "/dev/gpiochip0"

#define LED_PIN_LEFT 2
#define LED_PIN_MID 3
#define LED_PIN_RIGHT 4
#define BUTTON_PIN_LEFT 17
#define BUTTON_PIN_MID 27
#define BUTTON_PIN_RIGHT 22
#define BUZZER_PIN 12

#define DEFAULT_PLAYBACK_DELAY 1000

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

void turnOnLed(Choice choice) {}

void turnOffAllLeds() {}

bool pollInput() {}

int main(int argc, char **argv) {
    int cur_time, last_time, elapsed_time;
    Choice sequence[MAX_SEQUENCE_LEN];
    int sequence_len;
    int cur_sequence_index;
    int playback_time_remaining;
    bool waiting_for_button_release;
    GameState game_state;
    bool running;
    
    sequence_len = 0;
    sequence[sequence_len] = getRandom() % NUM_CHOICES;
    sequence_len++;

    cur_sequence_index = -1;
    playback_time_remaining = 0;
    game_state = game_state_playback;
    
    running = true;
    last_time = timeNow();
    while (running) {
        cur_time = timeNow();
        elapsed_time = cur_time - last_time;
        last_time = cur_time;
        
        switch (game_state) {
        case game_state_playback:
            playback_time_remaining -= elapsed;
            if (playback_time_remaining <= 0) {
                cur_sequence_index++;
                if (cur_sequence_index >= sequence_len) {
                    stopTone();
                    turnOffLeds();
                    clearInputEvents();
                    waiting_for_button_release = false;
                    game_state = game_state_input;
                } else {
                    startTone(sequence[cur_sequence_index]);
                    turnOffLeds();
                    turnOnLed(sequence[cur_sequence_index]);
                    playback_time_remaining = DEFAULT_PLAYBACK_DELAY;
                }
            }
            break;
        case game_state_input:
            const InputEvent input_event;
            if (pollInput(&input_event)) {
                switch (input_event.type) {
                case event_button_down:
                    if (waiting_for_button_release) break;
                    if (input_event.choice == sequence[cur_sequence_index]) {
                        startTone(sequence[cur_sequence_index]);
                        turnOffLeds();
                        turnOnLed(sequence[cur_sequence_index]);
                        waiting_for_button_release = true;
                    } else {
                        game_state = game_state_game_over;
                    }
                    break;
                case event_button_up:
                    if (waiting_for_button_release && input_event.choice == sequence_len[cur_sequence_index]) {
                        stopTone();
                        turnOffLeds();
                        waiting_for_button_release = false;
                        cur_sequence_index++;
                        if (cur_sequence_index >= sequence_len) {
                            sequence[sequence_len] = getRandom() % NUM_CHOICES;
                            sequence_len++;
                            game_state = game_state_playback;
                        }
                    }
                    break;
                } 
            }
        }
    }
    
    return 0;
}
