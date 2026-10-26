#include "tariff.h"

// MP domestic tariff: the energy rate applies to ALL units, and both the rate
// and the fixed charge depend on whether the month stays within the slab.
Bill calcBill(float units, const Settings &s) {
  Bill b;
  bool lowSlab = units <= s.slabUnits;
  b.units = units;
  b.ratePerUnit = (lowSlab ? s.rateLowPaise : s.rateHighPaise) / 100.0f;
  b.energyRs = units * b.ratePerUnit;

  uint16_t fixedPerKw;
  if (s.isUrban) fixedPerKw = lowSlab ? s.fixedUrbanLow : s.fixedUrbanHigh;
  else fixedPerKw = lowSlab ? s.fixedRuralLow : s.fixedRuralHigh;
  b.fixedRs = fixedPerKw * s.sanctionedKw;

  float gross = b.energyRs + b.fixedRs;
  b.subsidyRs = s.subsidyRs > gross ? gross : s.subsidyRs;
  b.totalRs = gross - b.subsidyRs;
  return b;
}
