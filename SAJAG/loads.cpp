#include "loads.h"
#include "storage.h"

#define STEP_MIN_W 40.0f      // smaller changes are treated as noise / drift
#define STEP_SPREAD_W 30.0f   // last few readings must agree with each other
#define HISTORY 3

static float baseW = 0;               // the "settled" power level
static float recent[HISTORY];
static int recentCount = 0;
static bool isOn[MAX_APPLIANCES];
static uint16_t unknownToday = 0;

void loadsReset() {
  baseW = 0;
  recentCount = 0;
  for (int k = 0; k < MAX_APPLIANCES; k++) isOn[k] = false;
}

bool loadsIsOn(int index) { return index >= 0 && index < MAX_APPLIANCES && isOn[index]; }
uint16_t loadsUnknownCountToday() { return unknownToday; }

void loadsNewDay() {
  unknownToday = 0;
  for (int k = 0; k < data.applianceCount; k++) {
    data.appliances[k].todayKwh = 0;
    data.appliances[k].todayOnSec = 0;
  }
}

static bool closeEnough(float stepW, float applianceW) {
  float tolerance = max(25.0f, applianceW * 0.2f);
  return fabs(stepW - applianceW) <= tolerance;
}

// Best matching appliance for a step, or -1.
// needOn: the state the appliance must be in right now (a switch-on step can
// only come from an appliance that is currently off, and vice versa).
static int matchAppliance(float stepW, bool needOn) {
  int best = -1;
  float bestErr = 1e9;
  for (int k = 0; k < data.applianceCount; k++) {
    if (isOn[k] != needOn) continue;
    float err = fabs(stepW - data.appliances[k].watts);
    if (closeEnough(stepW, data.appliances[k].watts) && err < bestErr) {
      best = k;
      bestErr = err;
    }
  }
  return best;
}

StepEvent loadsUpdate(float watts, float dtSec) {
  StepEvent ev = {STEP_NONE, 0, -1};

  // Charge running appliances for the time that just passed
  for (int k = 0; k < data.applianceCount; k++) {
    if (isOn[k]) {
      data.appliances[k].todayOnSec += (uint32_t)dtSec;
      data.appliances[k].todayKwh += data.appliances[k].watts * dtSec / 3600000.0f;
    }
  }

  // Keep the last few readings
  if (recentCount < HISTORY) {
    recent[recentCount++] = watts;
  } else {
    for (int k = 0; k < HISTORY - 1; k++) recent[k] = recent[k + 1];
    recent[HISTORY - 1] = watts;
  }
  if (recentCount < HISTORY) {
    baseW = watts;
    return ev;
  }

  float lo = recent[0], hi = recent[0], sum = 0;
  for (int k = 0; k < HISTORY; k++) {
    lo = min(lo, recent[k]);
    hi = max(hi, recent[k]);
    sum += recent[k];
  }
  float avg = sum / HISTORY;
  float delta = avg - baseW;

  if (fabs(delta) >= STEP_MIN_W && (hi - lo) <= STEP_SPREAD_W) {
    // A clean, settled step up or down
    baseW = avg;
    ev.deltaW = delta;
    if (delta > 0) {
      int k = matchAppliance(delta, false);
      if (k >= 0) {
        isOn[k] = true;
        ev.kind = STEP_KNOWN_ON;
        ev.applianceIndex = k;
      } else {
        ev.kind = STEP_UNKNOWN_ON;
        unknownToday++;
      }
    } else {
      int k = matchAppliance(-delta, true);
      if (k >= 0) {
        isOn[k] = false;
        ev.kind = STEP_KNOWN_OFF;
        ev.applianceIndex = k;
      } else {
        ev.kind = STEP_UNKNOWN_OFF;
      }
    }
  } else if (fabs(delta) < STEP_MIN_W) {
    // Slow drift (fridge compressor warming up, voltage changes): follow it gently
    baseW += 0.1f * delta;
  }

  // Nothing can be on if there is no load at all
  if (watts < 5) {
    for (int k = 0; k < MAX_APPLIANCES; k++) isOn[k] = false;
    baseW = 0;
  }
  return ev;
}
