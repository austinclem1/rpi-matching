#define BUZZER_PIN 12

#define DEFAULT_PLAYBACK_DELAY 1000

typedef enum {
    choice_left,
    choice_mid,
    choice_right,
    NUM_CHOICES,
} Choice;

static const int ledPins[NUM_CHOICES] = { ?, ?, ? };
static const int freqs[NUM_CHOICES] = { 440, 550, 660 };
static const int buttonPins[NUM_CHOICES] = { ?, ?, ? };

typedef enum {
    game_state_playback,
    game_state_input,
} GameState;

void startTone(Choice choice) {
    const int freq = freqs[choice];
    const int period_ns = NS_PER_SEC / freq;
    const int duty_cycle_ns = period_ns / 2;
}
void stopTone() {}

void turnOnLed(Choice choice) {
    // TODO clear all leds
    // set led ledPins[choice] on
}

void turnOffLeds() {}

int main(int argc, char **argv) {
    int cur_time, last_time, elapsed_time;
    Choice sequence[MAX_SEQUENCE_LEN];
    int sequence_len;
    int playback_time_remaining;
    int cur_sequence_index;
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
        
        switch (game_state) {
        case game_state_playback:
            playback_time_remaining -= elapsed;
            if (playback_time_remaining <= 0) {
                cur_sequence_index++;
                if (cur_sequence_index >= sequence_len) {
                    stopTone();
                    turnOffLeds();
                    clearInputEvents();
                    game_state = game_state_input;
                } else {
                    Choice cur_playback_choice = sequence[cur_sequence_index];
                    startTone(cur_playback_choice);
                    turnOnLed(cur_playback_choice);
                    playback_time_remaining = DEFAULT_PLAYBACK_DELAY;
                }
            }
            break;
        case game_state_input:
            const InputEvent input = pollInput();
            break;
        }
    }
    
    return 0;
}
