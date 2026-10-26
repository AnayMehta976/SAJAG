// Shared objects and functions used across the SAJAG source files
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <LiquidCrystal.h>
#include <UniversalTelegramBot.h>
#include <time.h>

#include "config.h"
#include "meter.h"
#include "storage.h"
#include "tariff.h"
#include "loads.h"

// Defined in SAJAG.ino
extern LiquidCrystal lcd;
extern UniversalTelegramBot bot;
extern Reading live;
extern bool timeValid;
bool sendMsg(const String &text);

// monitor.cpp
void monitorUpdate(const Reading &r, float dtSec);
String dailyReport(const struct tm &t);
float projectedMonthUnits(const struct tm &t);
String fmt(float v, int decimals);

// telegram.cpp
void telegramPoll();

// display.cpp
void lcdShow(const char *line1, const char *line2);
void lcdUpdate();
