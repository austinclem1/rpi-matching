#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "game.h"
#include "platform.h"

#define DEFAULT_PLAYBACK_DELAY 1000
#define MAX_SEQUENCE_LEN 2000

int main(void) {
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
