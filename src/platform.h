#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <time.h>

#include "game.h"
#include "platform_raspberrypi_linux.h"

typedef struct LedsDevice LedsDevice;
typedef struct InputDevice InputDevice;
typedef struct SoundDevice SoundDevice;

bool initLeds(LedsDevice *dev_out);
void turnOnLed(LedsDevice *dev, Choice choice);
void turnOffAllLeds(LedsDevice *dev);
void deinitLeds(LedsDevice *dev);

bool initInput(InputDevice *dev_out);
bool pollInput(InputDevice *dev, InputEvent *ev_out);
void clearInputEvents(InputDevice *dev);
void deinitInput(InputDevice *dev);

bool initSound(SoundDevice *dev_out);
void startTone(SoundDevice *dev, Choice choice);
void stopTone(SoundDevice *dev);
void deinitSound(SoundDevice *dev);

time_t nanoTimestamp(void);

#endif
