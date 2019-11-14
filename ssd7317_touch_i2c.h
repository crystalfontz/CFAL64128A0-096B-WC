#pragma once
#include <Arduino.h>
#include "prefs.h"

//////////////////////////////////////////////

//touch CS
#define SSD7317_TOUCH_CS_CLR	digitalWrite(SSD7317_TOUCH_CS, LOW)
#define SSD7317_TOUCH_CS_SET	digitalWrite(SSD7317_TOUCH_CS, HIGH)
//touch IRQ pin
#define SSD7317_TOUCH_IRQ_RD	digitalRead(SSD7317_TOUCH_IRQ)
//touch I2C SCL
#define SSD7317_TOUCH_SCL_CLR	digitalWrite(SSD7317_TOUCH_SCL, LOW)
#define SSD7317_TOUCH_SCL_SET	digitalWrite(SSD7317_TOUCH_SCL, HIGH)
//touch I2C SDA
#define SSD7317_TOUCH_SDA_CLR	digitalWrite(SSD7317_TOUCH_SDA, LOW)
#define SSD7317_TOUCH_SDA_SET	digitalWrite(SSD7317_TOUCH_SDA, HIGH)
// #define SSD7317_TOUCH_SDA_RD	digitalRead(SSD7317_TOUCH_SDA) /*this is a function*/

//////////////////////////////////////////////

extern volatile bool SSD7317_TouchData_Waiting;
extern volatile uint16_t SSD7317_Gesture_Data;

void SSD7317_Touch_Init(void);
void SSD7317_Touch_HWI2C(bool enable);
void SSD7317_Touch_Handle(void);
