// Everything that must survive a power cut lives in this struct,
// which is saved to the ESP32's EEPROM (flash) area.
#pragma once
#include <Arduino.h>
#include "config.h"

struct Settings {
  float sanctionedKw;
  bool isUrban;
  uint16_t slabUnits;
  uint16_t rateLowPaise;
  uint16_t rateHighPaise;
  uint16_t fixedUrbanLow, fixedUrbanHigh;
  uint16_t fixedRuralLow, fixedRuralHigh;
  uint16_t subsidyRs;       // government subsidy taken off the monthly bill
  uint16_t budgetRs;        // monthly budget for alerts
  uint16_t thresholdW;      // high-load alert level
  uint8_t reportHour;       // hour of the daily Telegram report
  float spikeFactor;
  uint16_t unknownLoadW;
  uint8_t theftPercent;
  float voltCal;
  float currCal;
};

struct Appliance {
  char name[16];
  uint16_t watts;
  float todayKwh;           // estimated energy used by this appliance today
  uint32_t todayOnSec;
};

struct Persist {
  uint32_t magic;
  uint16_t version;

  double totalKwh;          // lifetime
  double monthKwh;
  double todayKwh;
  float todayPeakW;
  int16_t lastDay;          // day of year, -1 = unknown
  int8_t lastMonth;         // 0-11
  int16_t lastYear;

  float lastDays[7];        // kWh for the previous 7 days, [0] = yesterday
  float hourBaselineKwh[24];// usual energy used in each hour of the day
  uint8_t learnedDays;      // how many days the baseline has learned from

  bool budget80Sent;
  bool budgetSent;

  uint8_t applianceCount;
  Appliance appliances[MAX_APPLIANCES];

  Settings s;
};

extern Persist data;

void storageBegin();
void storageSave();
void storageResetSettings();
