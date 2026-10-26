# SAJAG — Smart Electricity Monitor

**SAJAG** (सजग, "alert") is an IoT device that sits at a home's main power supply and shows exactly how electricity is being used. Normal meters only give one number at the end of the month. SAJAG measures voltage, current and power every second, works out the bill from the current tariff slabs, and sends alerts and a daily report on **Telegram**. It also flags unusual usage that could mean someone else is drawing power from your connection.

Built for **Bal Vigyan 2024**.

## The problem

- About **30%** of electricity used in Indian homes is wasted (≈444 TWh a year), mostly from appliances left on, standby loads and old, inefficient appliances.
- Over **60%** of consumers don't know how they use electricity, so bills come as a surprise.
- **Electricity theft** (people tapping into someone else's supply, common in apartments, or illegal direct connections) costs India about ₹90,000 crore a year. It also means honest consumers pay for power they never used.

## What SAJAG does

| Feature | How |
|---|---|
| Live readings | Voltage, current, real power (W) and power factor every second on a 16x2 LCD and on Telegram (`/now`) |
| Energy and cost | Counts units (kWh) for today, the month and lifetime, and estimates the bill with the **MP tariff slabs** |
| Adjustable tariff and subsidy | Slab limit, rates, fixed charges, sanctioned load, urban/rural and government subsidy can all be changed from Telegram; settings survive power cuts (EEPROM) |
| Daily report | Every evening on Telegram: units, cost, peak load, comparison with the last 7 days, appliance breakdown, projected month bill |
| Appliance breakdown | Recognises appliances you register (e.g. `/addapp fridge 150`) by the jump in power when they switch on and off |
| Unauthorised use alerts | Alerts on a large load that matches none of your appliances, on an hour that uses far more than normal for that time, and on a load that stays above a limit |
| Meter cross-check | `/compare <units>` checks the board meter's monthly units against SAJAG's own measurement to spot meter tampering, tapping or billing errors |
| Budget alerts | Warns at 80% and 100% of a monthly budget |

## Hardware

| Part | Purpose |
|---|---|
| ESP32 DevKit (ESP32 Dev Module) | Microcontroller with WiFi for Telegram |
| ZMPT101B | AC voltage sensor |
| ACS712 (30A module) | AC current sensor |
| 16x2 LCD + 10k potentiometer | Local display (contrast) |
| 10k + 20k resistors | Divider to bring the ACS712's 5 V output down to the ESP32's 3.3 V range |
| Jumper wires, breadboard, 5 V supply | Connections and power |

### Wiring

| Module pin | ESP32 pin |
|---|---|
| ZMPT101B OUT | GPIO 34 |
| ZMPT101B VCC / GND | 3.3V / GND |
| ACS712 OUT → 10k → **GPIO 35** → 20k → GND | GPIO 35 |
| ACS712 VCC / GND | 5V (VIN) / GND |
| LCD RS, E | GPIO 19, GPIO 23 |
| LCD D4, D5, D6, D7 | GPIO 18, 17, 16, 15 |
| LCD VSS, RW, K | GND |
| LCD VDD, A | 5V |
| LCD V0 | Middle pin of 10k pot (ends to 5V and GND) |

Most 5 V LCDs read the ESP32's 3.3 V signals fine. If the screen shows garbage, use a 3.3 V LCD or a level shifter.

The sensors use ADC1 pins (GPIO 32–39) because ADC2 pins don't work while WiFi is on. Pins are set in [`SAJAG/config.h`](SAJAG/config.h).

> ⚠️ **Mains safety.** The ZMPT101B input and the ACS712 terminals carry 230 V AC. Wire them with the supply switched off, use insulated terminals and an enclosure, and get an adult or an electrician to make the mains connections. The ESP32 side is low voltage.

## Software setup

1. Install the [Arduino IDE](https://www.arduino.cc/en/software).
2. Add ESP32 support: **File → Preferences → Additional boards manager URLs** →
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`, then **Tools → Board → Boards Manager → esp32 by Espressif** (version 2.0.x).
3. Install these libraries from **Sketch → Include Library → Manage Libraries**:
   - **UniversalTelegramBot** by Brian Lough
   - **ArduinoJson** by Benoit Blanchon (version 6.x)
   - **LiquidCrystal** (built in)
4. Create a Telegram bot: message **@BotFather**, send `/newbot` and copy the token. Then message **@userinfobot** to get your chat ID.
5. In the `SAJAG` folder, copy `secrets.example.h` to `secrets.h` and fill in your WiFi name, password, bot token and chat ID. `secrets.h` is git-ignored so it never gets uploaded.
6. Open `SAJAG/SAJAG.ino`, select **ESP32 Dev Module** and the right COM port, and upload.

### Calibration

Each ZMPT101B and ACS712 module is slightly different, so calibrate once against a multimeter:

1. Measure the mains voltage with a multimeter and send `/calv 231` (your reading).
2. Switch on a steady load such as a heater or kettle, measure its current with a clamp meter, and send `/cali 4.3`.

The calibration is saved in EEPROM. Also turn the small potentiometer on the ZMPT101B so its output isn't clipping. The Serial Monitor at 115200 baud shows what the board is doing.

## Telegram commands

| Command | What it does |
|---|---|
| `/now` | Live voltage, current, power, power factor |
| `/today`, `/month` | Units and cost so far |
| `/bill` | Full bill breakdown with the month-end projection |
| `/report` | Daily report right now |
| `/addapp <name> <watts>`, `/delapp <name>`, `/apps` | Manage your appliance list |
| `/compare <units>` | Check the board meter's units for this month against SAJAG |
| `/setload`, `/setarea`, `/setslab`, `/setrates`, `/setfixed`, `/setsubsidy` | Tariff settings |
| `/setbudget`, `/setthreshold`, `/setunknown`, `/setreport` | Alert settings |
| `/calv <volts>`, `/cali <amps>` | Calibration |
| `/settings`, `/resetmonth`, `/help` | Misc |

The bot only answers the chat ID in `secrets.h`, so nobody else can read or change your meter.

## Tariff used

Default values follow the MP domestic tariff that was explained during our visit to the Kanadia Power Grid sub-station. The energy charge applies to **all** units, and the rate depends on the month's total:

| Monthly consumption | Energy charge | Fixed charge (urban) | Fixed charge (rural) |
|---|---|---|---|
| Up to 50 units | 630 paise/unit | ₹82 per kW | ₹67 per kW |
| Above 50 units | 780 paise/unit | ₹138 per kW | ₹117 per kW |

Example: 120 units in an urban home with a 1 kW sanctioned load comes to 120 × ₹7.80 + ₹138 = **₹1,074**. When the tariff or subsidy changes, update it from Telegram; there's no need to reprogram.

## How the detection works

- **Power measurement.** About 1,500 voltage and current samples are taken over 10 mains cycles (200 ms). Removing each sensor's DC offset gives RMS voltage and current, and averaging V×I sample by sample gives real power, so power factor is accounted for.
- **Appliance recognition.** When an appliance switches on, total power steps up by roughly its wattage and stays there. SAJAG waits for the readings to settle and then matches the step against your registered appliances (within ±20%). It tracks each appliance's on-time and energy.
- **Unknown loads.** A large step that matches nothing (default 1,000 W or more) triggers an immediate alert.
- **Unusual hours.** SAJAG learns how much energy each hour of the day normally uses. After 3 days, an hour that uses more than 2× its usual amount triggers an alert.
- **Meter cross-check.** If the board meter shows much more (default over 10%) than SAJAG measured for the month, the extra energy was not used by the house SAJAG monitors.

Limitations: one sensor can't separate two appliances that switch at the same moment or have nearly the same wattage, and loads that change continuously (inverter ACs, washing machines) show up as "other". Accuracy depends on calibration.

## Project structure

```
SAJAG/
├── SAJAG.ino           setup() and loop()
├── app.h               shared declarations
├── config.h            pins, calibration, tariff and alert defaults
├── secrets.example.h   template for WiFi and Telegram details
├── meter.h / .cpp      sampling and RMS / power maths
├── tariff.h / .cpp     bill calculation
├── loads.h / .cpp      appliance on/off detection
├── storage.h / .cpp    EEPROM save and load
├── monitor.cpp         energy totals, day/month rollover, alerts, daily report
├── telegram.cpp        Telegram commands
└── display.cpp         LCD pages
```

## Future work

- Predictive maintenance: spot appliances whose power draw drifts as they age
- Automatic load shifting to off-peak hours with smart plugs
- EV charging schedules
- A mobile app with graphs
- Reporting usage data to the utility for demand response programmes
