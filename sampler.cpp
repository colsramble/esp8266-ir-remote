#include <ESP8266WiFi.h>
#include "sampler.h"

// private globals
static uint32_t tExpire;

// private functions
void play( byte pin, uint32_t period, byte* srcBuffer, int len, int invert);
void usDelay(uint32_t dly);

// function definitions
void digitalRecord( byte pin, uint32_t period, byte* destBuffer, int len) {
  int i,b;
  byte v,o;
  
  tExpire = micros();
  for(i=0; i<len; i++) {
    v = 0;
    for(b=0; b<8; b++) {
      usDelay(period);
      o = digitalRead(pin);
      // Set bit "b" 
      if (o == HIGH) 
        o = 0x01 << b;
      else
        o = 0x00;
      v = v | o;
    }
    destBuffer[i] = v;
  }
}

void digitalPlay( byte pin, uint32_t period, byte* srcBuffer, int len ) {
  play(pin, period, srcBuffer, len, 0);
}

void digitalPlayInverted( byte pin, uint32_t period, byte* srcBuffer, int len) {
  play(pin, period, srcBuffer, len, 1);
}

void play( byte pin, uint32_t period, byte* srcBuffer, int len, int invert) {
  int i,b;
  byte v,o;
  
  tExpire = micros();
  for(i=0; i<len; i++) {
    v = srcBuffer[i];
    for(b=0; b<8; b++) {
      o = 0x01 << b;
      if (invert)
        o = o & v ? LOW : HIGH;
      else 
        o = o & v ? HIGH : LOW;
        
      usDelay(period);
      digitalWrite(pin, o);
    }
  }  
}

void usDelay(uint32_t dly) {
  while (tExpire > micros()) {
    yield();  
  } 
  tExpire = micros() + dly;
}

