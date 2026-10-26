// Voltage / current sampling and power calculation
#pragma once
#include <Arduino.h>

struct Reading {
  float vrms;       // volts
  float irms;       // amps
  float realW;      // real (active) power, watts
  float apparentVA; // V x I
  float pf;         // power factor
};

void meterBegin();
Reading meterRead(float voltCal, float currCal);
