#include <linux/gpio.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>

#include "game.h"
#include "platform.h"

#define GPIO_CHARDEV_PATH "/dev/gpiochip0"
#define PWM_DEV_PATH "/sys/class/pwm/pwmchip0"

#define LED_PIN_LEFT 17
#define LED_PIN_MID 27
#define LED_PIN_RIGHT 22
#define BUTTON_PIN_LEFT 13
#define BUTTON_PIN_MID 19
#define BUTTON_PIN_RIGHT 26
#define BUZZER_PIN 12

#define DEBOUNCE_PERIOD_US 10000

typedef struct LedsDevice {
    int fd;
} LedsDevice;

typedef struct InputDevice {
    int fd;
} InputDevice;

typedef struct SoundDevice {
    int fd;
} SoundDevice;

static const int freqs[NUM_CHOICES] = { 440, 550, 660 };

LedsDevice *initLedsDevice(void) {
    LedsDevice *result_dev = NULL;
    int gpio_chardev_fd;
    struct gpio_v2_line_request request = { 0 };

    result_dev = (LedsDevice *) malloc(sizeof(LedsDevice));
    if (result_dev == NULL) goto exit;
    
    gpio_chardev_fd = open(GPIO_CHARDEV_PATH, O_RDONLY);
    if (gpio_chardev_fd < 0) {
        fprintf(stderr, "Failed to open gpio device \"%s\": %s\n", GPIO_CHARDEV_PATH, strerror(errno));
        result_dev = NULL;
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
        result_dev = NULL;
        goto exit_close_gpio_chardev;
    }
    
    result_dev->fd = request.fd;

exit_close_gpio_chardev:
    if (close(gpio_chardev_fd) < 0) {
        fprintf(stderr, "Failed to close gpio device \"%s\": %s\n", GPIO_CHARDEV_PATH, strerror(errno));
    }

exit:
    return result_dev;
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

void deinitLedsDevice(LedsDevice *dev) {
    int ret;

    ret = close(dev->fd);
    if (ret < 0) {
        fprintf(stderr, "Failed to close gpio LEDs line file: %s\n", strerror(errno));
    }

    free(dev);
}

InputDevice *initInputDevice(void) {
    InputDevice *result_dev = NULL;
    int gpio_chardev_fd;
    struct gpio_v2_line_request request = { 0 };

    result_dev = (InputDevice *) malloc(sizeof(InputDevice));
    if (result_dev == NULL) goto exit;
    
    gpio_chardev_fd = open(GPIO_CHARDEV_PATH, O_RDONLY);
    if (gpio_chardev_fd < 0) {
        fprintf(stderr, "Failed to open gpio device \"%s\": %s\n", GPIO_CHARDEV_PATH, strerror(errno));
        result_dev = NULL;
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
        GPIO_V2_LINE_FLAG_EDGE_RISING |
        GPIO_V2_LINE_FLAG_BIAS_PULL_UP;
    request.config.attrs[0].mask = 0b111;
    request.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_DEBOUNCE;
    request.config.attrs[0].attr.debounce_period_us = DEBOUNCE_PERIOD_US;
    request.config.num_attrs = 1;

    if (ioctl(gpio_chardev_fd, GPIO_V2_GET_LINE_IOCTL, &request) < 0) {
        fprintf(stderr, "Failed to get gpio line handle with GPIO_V2_GET_LINE_IOCTL: %s", strerror(errno));
        result_dev = NULL;
        goto exit_close_gpio_chardev;
    }
    
    result_dev->fd = request.fd;

exit_close_gpio_chardev:
    if (close(gpio_chardev_fd) < 0) {
        fprintf(stderr, "Failed to close gpio device \"%s\": %s\n", GPIO_CHARDEV_PATH, strerror(errno));
    }

exit:
    return result_dev;
}

bool pollInput(InputDevice *dev, InputEvent *ev_out) {
    struct pollfd poll_fd = { 0 };
    struct gpio_v2_line_event event;
    int ret;

    poll_fd.fd = dev->fd;
    poll_fd.events = POLLIN;

    ret = poll(&poll_fd, 1, 0);
    if (ret < 1) return false;

    ret = read(dev->fd, &event, sizeof(event));
    if (ret == -1) return false;
    if (ret != sizeof(event)) return false;
    switch (event.offset) {
    case BUTTON_PIN_LEFT:
        ev_out->choice = choice_left;
        break;
    case BUTTON_PIN_MID:
        ev_out->choice = choice_mid;
        break;
    case BUTTON_PIN_RIGHT:
        ev_out->choice = choice_right;
        break;
    default:
        return false;
        break;
    }
    switch (event.id) {
    case GPIO_V2_LINE_EVENT_FALLING_EDGE:
        ev_out->type = event_button_down;
        break;
    case GPIO_V2_LINE_EVENT_RISING_EDGE:
        ev_out->type = event_button_up;
        break;
    }
    return true;
}

void clearInputEvents(InputDevice *dev) {}

void deinitInputDevice(InputDevice *dev) {
    int ret;
    
    ret = close(dev->fd);
    if (ret < 0) {
        fprintf(stderr, "Failed to close gpio buttons line file: %s\n", strerror(errno));
    }

    free(dev);
}

SoundDevice *initSoundDevice(void) {
    FILE *file;
    file = fopen("/sys/class/pwm/pwmchip0/export", "w");
    fputs("0", file);
    fclose(file);
    return (SoundDevice *) malloc(sizeof(SoundDevice));
}

void startTone(SoundDevice *dev, Choice choice) {
    FILE *file;
    const int freq = freqs[choice];
    const int period_ns = NS_PER_SEC / freq;
    const int duty_cycle_ns = period_ns / 2;

    {
        file = fopen("/sys/class/pwm/pwmchip0/pwm0/period", "w");
        if (file == NULL) goto end;
        fprintf(file, "%d", period_ns);
        fclose(file);
    }
    {
        file = fopen("/sys/class/pwm/pwmchip0/pwm0/duty_cycle", "w");
        if (file == NULL) goto end;
        fprintf(file, "%d", duty_cycle_ns);
        fclose(file);
    }
    {
        file = fopen("/sys/class/pwm/pwmchip0/pwm0/enable", "w");
        if (file == NULL) goto end;
        fputs("1", file);
        fclose(file);
    }

end:
        return;
}

void stopTone(SoundDevice *dev) {
    FILE *file;
    file = fopen("/sys/class/pwm/pwmchip0/pwm0/enable", "w");
    if (file == NULL) return;
    fputs("0", file);
    fclose(file);
}

void deinitSoundDevice(SoundDevice *dev) {
    FILE *file;
    file = fopen("/sys/class/pwm/pwmchip0/unexport", "w");
    fputs("0", file);
    fclose(file);
    free(dev);
}

time_t nanoTimestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    return (ts.tv_sec * NS_PER_SEC) + ts.tv_nsec;
}
