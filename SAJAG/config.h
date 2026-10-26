// SAJAG - hardware pins, calibration defaults and tariff defaults.
// Change these to match your own wiring and your electricity board.
#pragma once

// ---------------- Pins ----------------
// Use ADC1 pins only (GPIO 32-39). ADC2 pins stop working while WiFi is on.
#define PIN_VOLTAGE 34   // ZMPT101B "OUT"
#define PIN_CURRENT 35   // ACS712 "OUT" through a 10k/20k divider (5V -> 3.3V range)

// 16x2 LCD in 4-bit mode (LiquidCrystal library)
#define LCD_RS 19
#define LCD_EN 23
#define LCD_D4 18
#define LCD_D5 17
#define LCD_D6 16
#define LCD_D7 15

// ---------------- Sensor settings ----------------
// ACS712 sensitivity: 5A = 185 mV/A, 20A = 100 mV/A, 30A = 66 mV/A
#define ACS712_MV_PER_A 66.0f
// Divider on the ACS712 output: Vadc = Vout * R2/(R1+R2) = 20k/(10k+20k)
#define CURRENT_DIVIDER 0.6667f

// Starting calibration. Fine-tune from Telegram with /calv and /cali
// while comparing against a multimeter; the result is saved in EEPROM.
#define DEFAULT_VOLT_CAL 0.75f    // mains volts per mV (RMS) at the ZMPT101B output
#define DEFAULT_CURR_CAL 1.0f     // multiplier on the computed current

// Readings below these are treated as noise (ACS712 is noisy near zero)
#define CURRENT_NOISE_A 0.08f
#define VOLTAGE_MIN_V 40.0f

// Sampling window: 200 ms = 10 mains cycles at 50 Hz
#define SAMPLE_WINDOW_US 200000UL
#define SAMPLE_PAIRS_MAX 1600

// ---------------- Timing ----------------
#define READ_INTERVAL_MS 1000UL        // one power reading per second
#define TELEGRAM_POLL_MS 3000UL
#define LCD_PAGE_MS 3000UL
#define EEPROM_SAVE_MS (10UL * 60UL * 1000UL)
#define ALERT_COOLDOWN_MS (30UL * 60UL * 1000UL)

// Time zone (India = UTC+5:30)
#define TZ_OFFSET_SEC 19800
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"

// ---------------- Tariff defaults (MPMKVVCL domestic LV-1.2) ----------------
// Energy charge applies on ALL units, rate depends on whether monthly
// consumption is up to the slab limit or above it.
#define DEF_SLAB_UNITS 50
#define DEF_RATE_LOW_PAISE 630      // <= 50 units
#define DEF_RATE_HIGH_PAISE 780     // > 50 units
// Monthly fixed charge, Rs per kW of sanctioned load
#define DEF_FIXED_URBAN_LOW 82
#define DEF_FIXED_URBAN_HIGH 138
#define DEF_FIXED_RURAL_LOW 67
#define DEF_FIXED_RURAL_HIGH 117
#define DEF_SANCTIONED_KW 1.0f
#define DEF_IS_URBAN true
#define DEF_SUBSIDY_RS 0
#define DEF_BUDGET_RS 1000

// ---------------- Alert defaults ----------------
#define DEF_THRESHOLD_W 3000        // alert if load stays above this for a minute
#define DEF_REPORT_HOUR 21          // daily Telegram report at 9 PM
#define DEF_SPIKE_FACTOR 2.0f       // hour uses 2x its usual energy -> alert
#define DEF_UNKNOWN_LOAD_W 1000     // unrecognised appliance this big -> alert
#define DEF_THEFT_PERCENT 10        // board meter vs SAJAG difference -> alert

#define MAX_APPLIANCES 10
