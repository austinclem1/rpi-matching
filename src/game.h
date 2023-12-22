#ifndef GAME_H
#define GAME_H

typedef enum {
    choice_left,
    choice_mid,
    choice_right,
    NUM_CHOICES,
} Choice;

typedef enum {
    event_button_down,
    event_button_up,
} EventType;

typedef struct {
    EventType type;
    Choice choice;
} InputEvent;

#endif
