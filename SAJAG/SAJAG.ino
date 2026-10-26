/*
  SAJAG - smart electricity monitor
  Bal Vigyan 2024

  ESP32 + ZMPT101B (voltage) + ACS712 (current) + 16x2 LCD.
  Measures voltage, current, power and energy, estimates the bill from the
  tariff slabs, spots unusual or unauthorised usage, and talks to the user
  on Telegram.

  Files in this sketch:
    SAJAG.ino     setup() and loop()
    app.h         shared declarations
    monitor.cpp   energy totals, day/month rollover, alerts, daily report
    telegram.cpp  Telegram commands
    display.cpp   LCD pages
    meter.*       sensor sampling and power maths
    tariff.*      bill calculation
    loads.*       appliance on/off detection
    storage.*     EEPROM save/load
    config.h      pins, calibration and tariff defaults
    secrets.h     WiFi and Telegram details (copy from secrets.example.h)
*/

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy secrets.example.h to secrets.h and fill in your WiFi and Telegram details"
#endif

#include <WiFiClientSecure.h>
#include "app.h"

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
WiFiClientSecure tlsClient;
UniversalTelegramBot bot(BOT_TOKEN, tlsClient);

Reading live = {0, 0, 0, 0, 0};
unsigned long lastReadMs = 0;
unsigned long lastPollMs = 0;
unsigned long lastSaveMs = 0;
unsigned long lastWifiTryMs = 0;
bool timeValid = false;

void setup() {
  Serial.begin(115200);
  Serial.println("\nSAJAG starting");

  lcd.begin(16, 2);
  lcdShow("    SAJAG", "  starting...");

  storageBegin();
  meterBegin();
  loadsReset();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lcdShow("Connecting WiFi", WIFI_SSID);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(250);

  tlsClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  configTime(TZ_OFFSET_SEC, 0, NTP_SERVER_1, NTP_SERVER_2);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP ");
    Serial.println(WiFi.localIP());
    sendMsg("SAJAG is online. Send /help to see what I can do.");
  } else {
    Serial.println("WiFi not connected yet - measuring offline");
  }
  lastReadMs = millis();
  lastSaveMs = millis();
}

void loop() {
  unsigned long now = millis();

  // 1. Measure once a second and update all totals and alerts
  if (now - lastReadMs >= READ_INTERVAL_MS) {
    float dtSec = (now - lastReadMs) / 1000.0f;
    lastReadMs = now;
    live = meterRead(data.s.voltCal, data.s.currCal);
    monitorUpdate(live, dtSec);
  }

  // 2. Keep WiFi alive without blocking the measurements
  if (WiFi.status() != WL_CONNECTED && now - lastWifiTryMs > 30000) {
    lastWifiTryMs = now;
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  // 3. Check Telegram for commands
  if (WiFi.status() == WL_CONNECTED && now - lastPollMs >= TELEGRAM_POLL_MS) {
    lastPollMs = now;
    telegramPoll();
  }

  // 4. Save to EEPROM every few minutes (flash wears out if we write too often)
  if (now - lastSaveMs >= EEPROM_SAVE_MS) {
    lastSaveMs = now;
    storageSave();
  }

  lcdUpdate();
}

// Sends a Telegram message to the owner (plain text, no formatting)
bool sendMsg(const String &text) {
  if (WiFi.status() != WL_CONNECTED) return false;
  return bot.sendMessage(CHAT_ID, text, "");
}
