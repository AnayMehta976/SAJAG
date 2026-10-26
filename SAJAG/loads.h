// Appliance detection from a single sensor.
// When an appliance switches on or off, total power jumps up or down by
// roughly that appliance's wattage. SAJAG watches for these "steps" and
// matches them against the appliances the user has registered (/addapp).
// A big step that matches nothing is an unknown load - possibly someone
// else connected to your supply.
#pragma once
#include <Arduino.h>

enum StepKind { STEP_NONE, STEP_KNOWN_ON, STEP_KNOWN_OFF, STEP_UNKNOWN_ON, STEP_UNKNOWN_OFF };

struct StepEvent {
  StepKind kind;
  float deltaW;
  int applianceIndex;   // -1 if unknown
};

void loadsReset();
StepEvent loadsUpdate(float watts, float dtSec);
bool loadsIsOn(int index);
uint16_t loadsUnknownCountToday();
void loadsNewDay();
