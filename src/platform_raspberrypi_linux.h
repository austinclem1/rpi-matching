#ifndef PLATFORM_RASPBERRYPI_LINUX_H
#define PLATFORM_RASPBERRYPI_LINUX_H

typedef struct LedsDevice {
    int fd;
} LedsDevice;

typedef struct InputDevice {
    int fd;
} InputDevice;

typedef struct SoundDevice {
    int fd;
} SoundDevice;

#endif
