#pragma once
#include <cstdint>
#include <cstddef>
#define HIGH 1
#define LOW 0
#define INPUT 1
#define OUTPUT 2
#define INPUT_PULLUP 3
#define pgm_read_byte(p) (*(const unsigned char*)(p))
void pinMode(int, int);
void digitalWrite(int, int);
int digitalRead(int);
void delay(unsigned long);
void delayMicroseconds(unsigned int);
unsigned long millis();
unsigned long micros();
