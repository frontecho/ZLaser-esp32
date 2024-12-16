#ifndef __BUTTON_H
#define __BUTTON_H

#include "Button2.h"

extern Button2 buttonL, buttonR;
extern int buttonState;

void setupButton();
void click(Button2 &btn);

#endif