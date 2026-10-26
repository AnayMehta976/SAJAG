#include "meter.h"
#include "config.h"

static uint16_t vBuf[SAMPLE_PAIRS_MAX];
static uint16_t iBuf[SAMPLE_PAIRS_MAX];

void meterBegin() {
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_VOLTAGE, ADC_11db);  // full 0-3.3 V range
  analogSetPinAttenuation(PIN_CURRENT, ADC_11db);
}

// Samples both sensors together over a few mains cycles.
// Both sensors sit on a DC offset (about half the supply), so we first find
// that offset (the average) and then work with the AC part only.
Reading meterRead(float voltCal, float currCal) {
  int n = 0;
  uint32_t start = micros();
  while (micros() - start < SAMPLE_WINDOW_US && n < SAMPLE_PAIRS_MAX) {
    vBuf[n] = analogReadMilliVolts(PIN_VOLTAGE);
    iBuf[n] = analogReadMilliVolts(PIN_CURRENT);
    n++;
  }

  double vSum = 0, iSum = 0;
  for (int k = 0; k < n; k++) {
    vSum += vBuf[k];
    iSum += iBuf[k];
  }
  double vOff = vSum / n, iOff = iSum / n;

  double vSq = 0, iSq = 0, viSum = 0;
  for (int k = 0; k < n; k++) {
    double v = vBuf[k] - vOff;
    double i = iBuf[k] - iOff;
    vSq += v * v;
    iSq += i * i;
    viSum += v * i;
  }

  // mV at the ADC -> real-world units
  const double ampsPerMv = currCal / (ACS712_MV_PER_A * CURRENT_DIVIDER);
  Reading r;
  r.vrms = sqrt(vSq / n) * voltCal;
  r.irms = sqrt(iSq / n) * ampsPerMv;
  r.realW = (viSum / n) * voltCal * ampsPerMv;

  if (r.vrms < VOLTAGE_MIN_V) r.vrms = 0;
  if (r.irms < CURRENT_NOISE_A || r.vrms == 0) {
    r.irms = 0;
    r.realW = 0;
  }
  // If the current sensor is wired the other way round, power comes out negative
  r.realW = fabs(r.realW);

  r.apparentVA = r.vrms * r.irms;
  r.pf = r.apparentVA > 1 ? r.realW / r.apparentVA : 0;
  if (r.pf > 1) r.pf = 1;
  if (r.realW > r.apparentVA) r.realW = r.apparentVA;
  return r;
}
