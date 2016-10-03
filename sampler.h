#ifndef SAMPLER_H
#define SAMPLER_H

// config

// prototypes
void digitalRecord( byte pin, uint32_t period, byte* destBuffer, int len);
void digitalPlay( byte pin, uint32_t period, byte* srcBuffer, int len);
void digitalPlayInverted( byte pin, uint32_t period, byte* srcBuffer, int len);
 
#endif

