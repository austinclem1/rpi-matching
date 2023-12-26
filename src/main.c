#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "game.h"
#include "platform.h"

#define PRE_PLAYBACK_DELAY 1000000000
#define PLAYBACK_DELAY_BETWEEN 500000000
#define MAX_SEQUENCE_LEN 2000

typedef enum {
    state_start_playback_mode,
    state_play_elem,
    state_pause_elem,
    state_start_input_mode,
    state_wait_for_input,
    state_play_correct_choice,
    state_play_gameover,
    STATE_COUNT,
} State;

typedef enum {
    signal_enter,
    signal_exit,
    signal_timeout,
    signal_input,
    SIGNAL_COUNT,
} Signal;

typedef struct {
    time_t started_at;
    time_t duration;
    bool active;
} Timer;

typedef struct {
    State state;
    LedsDevice *leds_dev;
    InputDevice *input_dev;
    SoundDevice *sound_dev;
    Choice sequence[MAX_SEQUENCE_LEN];
    int sequence_len;
    int cur_sequence_index;
    InputEvent input_event;
    Timer timer;
    bool running;
} StateMachine;

State startPlaybackMode(StateMachine *machine, Signal signal);
State playElem(StateMachine *machine, Signal signal);
State pauseElem(StateMachine *machine, Signal signal);
State startInputMode(StateMachine *machine, Signal signal);
State waitForInput(StateMachine *machine, Signal signal);
State playCorrectChoice(StateMachine *machine, Signal signal);
State playGameover(StateMachine *machine, Signal signal);

typedef State (*SignalHandler)(StateMachine *, Signal);

static const SignalHandler signal_handlers[STATE_COUNT] = {
    startPlaybackMode,
    playElem,
    pauseElem,
    startInputMode,
    waitForInput,
    playCorrectChoice,
    playGameover,
};

bool StateMachine_init(StateMachine *machine_out) {
    srand(time(NULL));

    machine_out->leds_dev = initLedsDevice();
    if (machine_out->leds_dev == NULL) goto error;
    machine_out->input_dev = initInputDevice();
    if (machine_out->input_dev == NULL) goto error_deinit_leds;
    machine_out->sound_dev = initSoundDevice();
    if (machine_out->sound_dev == NULL) goto error_deinit_input_deinit_leds;

    machine_out->sequence_len = 0;

    machine_out->timer.active = false;

    machine_out->state = state_start_playback_mode;
    machine_out->running = true;

    return true;

error_deinit_sound_deinit_input_deinit_leds:
    deinitSoundDevice(machine_out->sound_dev);
    
error_deinit_input_deinit_leds:
    deinitInputDevice(machine_out->input_dev);
    
error_deinit_leds:
    deinitLedsDevice(machine_out->leds_dev);

error:
    return false;
}

bool StateMachine_pollSignal(StateMachine *machine, Signal *signal_out) {
    time_t cur_time, elapsed;
    
    // check timer
    if (machine->timer.active) {
        cur_time = nanoTimestamp();
        elapsed = cur_time - machine->timer.started_at;
        if (elapsed >= machine->timer.duration) {
            machine->timer.active = false;
            *signal_out = signal_timeout;
            return true;
        }
    }

    if (pollInput(machine->input_dev, &machine->input_event)) {
        *signal_out = signal_input;
	return true;
    }

    return false;
}

void StateMachine_run(StateMachine *machine) {
    Signal signal;
    State next_state;

    next_state = signal_handlers[machine->state](machine, signal_enter);
    
    while (machine->running) {
        // handle state transition
        if (next_state != machine->state) {
            signal_handlers[machine->state](machine, signal_exit);
            machine->state = next_state;
            next_state = signal_handlers[machine->state](machine, signal_enter);
            continue;
        }
        
        if (StateMachine_pollSignal(machine, &signal)) {
            next_state = signal_handlers[machine->state](machine, signal);
        }
    }
}

void StateMachine_startTimer(StateMachine *machine, time_t duration) {
    machine->timer.duration = duration;
    machine->timer.started_at = nanoTimestamp();
    machine->timer.active = true;
}

State startPlaybackMode(StateMachine *machine, Signal signal) {
    State next_state = state_start_playback_mode;
    assert(machine->state == next_state);
    
    switch (signal) {
    case signal_enter:
        machine->sequence[machine->sequence_len] = rand() % NUM_CHOICES;
        machine->sequence_len++;
        machine->cur_sequence_index = 0;
        StateMachine_startTimer(machine, PRE_PLAYBACK_DELAY);
        break;
    case signal_timeout:
        next_state = state_play_elem;
        break;
    default:
        break;
    }

    return next_state;
}

State playElem(StateMachine *machine, Signal signal) {
    Choice elem;
    State next_state = state_play_elem;
    assert(machine->state == next_state);
    
    switch (signal) {
    case signal_enter:
        if (machine->cur_sequence_index >= machine->sequence_len) {
            next_state = state_start_input_mode;
            break;
        }
        elem = machine->sequence[machine->cur_sequence_index];
        turnOnLed(machine->leds_dev, elem);
        startTone(machine->sound_dev, elem);
        StateMachine_startTimer(machine, PLAYBACK_DELAY_BETWEEN);
        machine->cur_sequence_index++;
        break;
    case signal_timeout:
        next_state = state_pause_elem;
        break;
    case signal_exit:
        turnOffAllLeds(machine->leds_dev);
        stopTone(machine->sound_dev);
        break;
    default:
        break;
    }

    return next_state;
}

State pauseElem(StateMachine *machine, Signal signal) {
    State next_state = state_pause_elem;
    assert(machine->state == next_state);
    
    switch (signal) {
    case signal_enter:
        StateMachine_startTimer(machine, PLAYBACK_DELAY_BETWEEN);
        break;
    case signal_timeout:
        next_state = state_play_elem;
        break;
    default:
        break;
    }

    return next_state;
}

State startInputMode(StateMachine *machine, Signal signal) {
    State next_state = state_start_input_mode;
    assert(machine->state == next_state);

    switch (signal) {
    case signal_enter:
        machine->cur_sequence_index = 0;
        next_state = state_wait_for_input;
        break;
    default:
        break;
    }

    return next_state;
}

State waitForInput(StateMachine *machine, Signal signal) {
    Choice correct_choice = machine->sequence[machine->cur_sequence_index];
    State next_state = state_wait_for_input;
    assert(machine->state == next_state);

    switch (signal) {
    case signal_input:
        if (machine->input_event.type != event_button_down) break;
        next_state = machine->input_event.choice == correct_choice ?
            state_play_correct_choice :
            state_play_gameover;
    default:
        break;
    }

    return next_state;
}

State playCorrectChoice(StateMachine *machine, Signal signal) {
    Choice cur_choice = machine->sequence[machine->cur_sequence_index];
    State next_state = state_play_correct_choice;
    assert(machine->state == next_state);

    switch (signal) {
    case signal_enter:
        turnOnLed(machine->leds_dev, cur_choice);
        startTone(machine->sound_dev, cur_choice);
        break;
    case signal_input:
        if (machine->input_event.type != event_button_up) break;
        if (machine->input_event.choice != cur_choice) break;
        machine->cur_sequence_index++;
        if (machine->cur_sequence_index >= machine->sequence_len) {
            next_state = state_start_playback_mode;
        } else {
            next_state = state_wait_for_input;
        }
        break;
    case signal_exit:
        turnOffAllLeds(machine->leds_dev);
        stopTone(machine->sound_dev);
        break;
    default:
        break;
    }

    return next_state;
}

State playGameover(StateMachine *machine, Signal signal) {
    State next_state = state_play_gameover;
    assert(machine->state == next_state);

    return next_state;
}

int main(void) {
    StateMachine machine;

    if (!StateMachine_init(&machine)) {
        fprintf(stderr, "Failed to initialize game!\n");
        return 1;
    }

    StateMachine_run(&machine);
    
    return 0;
}
