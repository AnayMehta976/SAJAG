// Electricity bill estimate from the adjustable tax/tariff slabs
#pragma once
#include "storage.h"

struct Bill {
  float units;
  float ratePerUnit;   // Rs
  float energyRs;
  float fixedRs;
  float subsidyRs;
  float totalRs;
};

Bill calcBill(float units, const Settings &s);
