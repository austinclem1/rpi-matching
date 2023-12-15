#include <linux/gpio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>

#include "game.h"
#include "platform_raspberrypi_linux.h"

#define GPIO_CHARDEV_PATH "/dev/gpiochip0"
#define PWM_DEV_PATH "/sys/class/pwm/pwmchip0"

#define LED_PIN_LEFT 2
#define LED_PIN_MID 3
#define LED_PIN_RIGHT 4
#define BUTTON_PIN_LEFT 17
#define BUTTON_PIN_MID 27
#define BUTTON_PIN_RIGHT 22
#define BUZZER_PIN 12

#define DEBOUNCE_PERIOD_US 10000

#define NS_PER_SEC 1000000000

static const int ledPins[NUM_CHOICES] = { LED_PIN_LEFT, LED_PIN_MID, LED_PIN_RIGHT };
static const int buttonPins[NUM_CHOICES] = { BUTTON_PIN_LEFT, BUTTON_PIN_MID, BUTTON_PIN_RIGHT };
static const int freqs[NUM_CHOICES] = { 440, 550, 660 };

bool initLeds(LedsDevice *dev_out) {
    bool success = false;
    int gpio_chardev_fd;
    struct gpio_v2_line_request request = { 0 };

    dev_out->fd = -1;
    
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
    
    dev_out->fd = request.fd;
    success = true;

exit_close_gpio_chardev:
    if (close(gpio_chardev_fd) < 0) {
        fprintf(stderr, "Failed to close gpio device \"%s\": %s\n", GPIO_CHARDEV_PATH, strerror(errno));
    }

exit:
    return success;
}

void turnOnLed(LedsDevice *dev, Choice choice) {
    struct gpio_v2_line_values values = { 0 };
    values.bits = -1;
    values.mask = 1 << choice;
    
    if (ioctl(dev->fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
        fprintf(stderr, "Failed to set led line value to 1: %s\n", strerror(errno));
    }
}

void turnOffAllLeds(LedsDevice *dev) {
    struct gpio_v2_line_values values = { 0 };
    values.bits = 0;
    values.mask = 0b111;
    
    if (ioctl(dev->fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
        fprintf(stderr, "Failed to set led line values to 0: %s\n", strerror(errno));
    }
}

void deinitLeds(LedsDevice *dev) {
    int ret;

    ret = close(dev->fd);
    if (ret < 0) {
        fprintf(stderr, "Failed to close gpio LEDs line file: %s\n", strerror(errno));
    }
}

bool initInput(InputDevice *dev_out) {
    bool success = false;
    int gpio_chardev_fd;
    struct gpio_v2_line_request request = { 0 };

    dev_out->fd = -1;
    
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
    
    dev_out->fd = request.fd;
    success = true;

exit_close_gpio_chardev:
    if (close(gpio_chardev_fd) < 0) {
        fprintf(stderr, "Failed to close gpio device \"%s\": %s\n", GPIO_CHARDEV_PATH, strerror(errno));
    }

exit:
    return success;
}

bool pollInput(InputDevice *dev, InputEvent *ev_out) {
    ev_out->type = event_button_down;
    ev_out->choice = choice_left;
    return true;
}

void clearInputEvents(InputDevice *dev) {}

void deinitInput(InputDevice *dev) {
    int ret;
    
    ret = close(dev->fd);
    if (ret < 0) {
        fprintf(stderr, "Failed to close gpio buttons line file: %s\n", strerror(errno));
    }
}

bool initSound(SoundDevice *dev_out) {
    return true;
}

void startTone(SoundDevice *dev, Choice choice) {
    const int freq = freqs[choice];
    const int period_ns = NS_PER_SEC / freq;
    const int duty_cycle_ns = period_ns / 2;
}

void stopTone(SoundDevice *dev) {}

void deinitSound(SoundDevice *dev) {}

time_t nanoTimestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    return (ts.tv_sec * NS_PER_SEC) + ts.tv_nsec;
}
