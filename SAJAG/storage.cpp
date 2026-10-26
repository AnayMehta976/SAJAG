#include "storage.h"
#include <EEPROM.h>

#define PERSIST_MAGIC 0x53414A47UL  // "SAJG"
#define PERSIST_VERSION 1

Persist data;

void storageResetSettings() {
  Settings &s = data.s;
  s.sanctionedKw = DEF_SANCTIONED_KW;
  s.isUrban = DEF_IS_URBAN;
  s.slabUnits = DEF_SLAB_UNITS;
  s.rateLowPaise = DEF_RATE_LOW_PAISE;
  s.rateHighPaise = DEF_RATE_HIGH_PAISE;
  s.fixedUrbanLow = DEF_FIXED_URBAN_LOW;
  s.fixedUrbanHigh = DEF_FIXED_URBAN_HIGH;
  s.fixedRuralLow = DEF_FIXED_RURAL_LOW;
  s.fixedRuralHigh = DEF_FIXED_RURAL_HIGH;
  s.subsidyRs = DEF_SUBSIDY_RS;
  s.budgetRs = DEF_BUDGET_RS;
  s.thresholdW = DEF_THRESHOLD_W;
  s.reportHour = DEF_REPORT_HOUR;
  s.spikeFactor = DEF_SPIKE_FACTOR;
  s.unknownLoadW = DEF_UNKNOWN_LOAD_W;
  s.theftPercent = DEF_THEFT_PERCENT;
  s.voltCal = DEFAULT_VOLT_CAL;
  s.currCal = DEFAULT_CURR_CAL;
}

static void storageFormat() {
  memset(&data, 0, sizeof(data));
  data.magic = PERSIST_MAGIC;
  data.version = PERSIST_VERSION;
  data.lastDay = -1;
  data.lastMonth = -1;
  storageResetSettings();
}

void storageBegin() {
  EEPROM.begin(sizeof(Persist));
  EEPROM.get(0, data);
  if (data.magic != PERSIST_MAGIC || data.version != PERSIST_VERSION) {
    // First boot (or the layout changed): start fresh
    storageFormat();
    storageSave();
  }
}

void storageSave() {
  EEPROM.put(0, data);
  EEPROM.commit();
}
