#ifndef PLATFORM_H
#define PLATFORM_H

#define NS_PER_SEC 1000000000

#include <stdbool.h>
#include <time.h>

#include "game.h"

typedef struct LedsDevice LedsDevice;
typedef struct InputDevice InputDevice;
typedef struct SoundDevice SoundDevice;

LedsDevice *initLedsDevice(void);
void turnOnLed(LedsDevice *dev, Choice choice);
void turnOffAllLeds(LedsDevice *dev);
void deinitLedsDevice(LedsDevice *dev);

InputDevice *initInputDevice(void);
bool pollInput(InputDevice *dev, InputEvent *ev_out);
void clearInputEvents(InputDevice *dev);
void deinitInputDevice(InputDevice *dev);

SoundDevice *initSoundDevice(void);
void startTone(SoundDevice *dev, Choice choice);
void stopTone(SoundDevice *dev);
void deinitSoundDevice(SoundDevice *dev);

time_t nanoTimestamp(void);

#endif
