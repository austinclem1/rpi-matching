#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <time.h>

#include "game.h"

int initLeds(void);
void turnOnLed(int fd, Choice choice);
void turnOffAllLeds(int fd);

int initButtons(void);
bool pollInput(InputEvent *ev_out);
void clearInputEvents(void);

int initBuzzer(void);
void startTone(Choice choice);
void stopTone(void);

time_t nanoTimestamp(void);

#endif
