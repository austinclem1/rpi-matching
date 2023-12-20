#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "game.h"
#include "platform.h"

typedef struct LedsDevice {
    bool left_led_on;
    bool mid_led_on;
    bool right_led_on;
} LedsDevice;

typedef struct InputDevice {
    bool button_is_down[NUM_CHOICES];
    time_t button_time_when_pressed[NUM_CHOICES];
} InputDevice;

typedef struct SoundDevice {} SoundDevice;

static void redrawLeds(LedsDevice *dev) {
    printf("\x1b[1F");
    printf("\x1b[2K");
    printf("[%c] [%c] [%c]\n",
        dev->left_led_on ? 'X' : '-',
        dev->mid_led_on ? 'X' : '-',
        dev->right_led_on ? 'X' : '-');
}

LedsDevice *initLeds(void) {
    LedsDevice *result_dev = NULL;

    result_dev = (LedsDevice *) malloc(sizeof(LedsDevice));
    if (result_dev == NULL) goto exit;

    result_dev->left_led_on = false;
    result_dev->mid_led_on = false;
    result_dev->right_led_on = false;

exit:
    return result_dev;
}

void turnOnLed(LedsDevice *dev, Choice choice) {
    switch (choice) {
    case choice_left:
        dev->left_led_on = true;
        break;
    case choice_mid:
        dev->mid_led_on = true;
        break;
    case choice_right:
        dev->right_led_on = true;
        break;
    default:
        break;
    }

    redrawLeds(dev);
}

void turnOffAllLeds(LedsDevice *dev) {
    dev->left_led_on = false;
    dev->mid_led_on = false;
    dev->right_led_on = false;

    redrawLeds(dev);
}

void deinitLeds(LedsDevice *dev) {
    free(dev);
}

InputDevice *initInput(void) {
    struct termios term_attr;
    InputDevice *result_dev = NULL;

    result_dev = (InputDevice *) malloc(sizeof(InputDevice));
    if (result_dev == NULL) goto exit;

    tcgetattr(STDIN_FILENO, &term_attr);
    term_attr.c_cc[VMIN] = 0;
    term_attr.c_cc[VTIME] = 0;
    term_attr.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term_attr);

    *result_dev = (InputDevice){ 0 };

exit:
    return result_dev;
}

bool pollInput(InputDevice *dev, InputEvent *ev_out) {
    const time_t simulated_press_len = NS_PER_SEC / 5;
    time_t elapsed = 0;
    ssize_t nread = 0;
    char buf[1] = { 0 };
    bool received_input = false;
    Choice choice;

    for (int i = 0; i < NUM_CHOICES; i++) {
        if (dev->button_is_down[i] == false) continue;
        
        elapsed = nanoTimestamp() - dev->button_time_when_pressed[i];
        if (elapsed < simulated_press_len) continue;
        
        ev_out->type = event_button_up;
        ev_out->choice = i;
        dev->button_is_down[i] = false;
        received_input = true;
        return received_input;
    }

    nread = read(STDIN_FILENO, buf, 1);
    if (nread > 0) {
        switch (buf[0]) {
        case '1':
            choice = choice_left;
            break;
        case '2':
            choice = choice_mid;
            break;
        case '3':
            choice = choice_right;
            break;
        default:
            choice = -1;
        }
        if (choice >= 0) {
            ev_out->type = event_button_down;
            ev_out->choice = choice;
            dev->button_is_down[choice] = true;
            dev->button_time_when_pressed[choice] = nanoTimestamp();
            received_input = true;
        }
    }

    return received_input;
}

void clearInputEvents(InputDevice *dev) {
    tcflush(STDIN_FILENO, TCIFLUSH);
}

void deinitInput(InputDevice *dev) {
    free(dev);
}

SoundDevice *initSound(void) {
    return (SoundDevice *) malloc(sizeof(SoundDevice));
}

void startTone(SoundDevice *dev, Choice choice) {
    // const int freq = freqs[choice];
    // const int period_ns = NS_PER_SEC / freq;
    // const int duty_cycle_ns = period_ns / 2;
}

void stopTone(SoundDevice *dev) {}

void deinitSound(SoundDevice *dev) {
    free(dev);
}

time_t nanoTimestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);

    return (ts.tv_sec * NS_PER_SEC) + ts.tv_nsec;
}
