// Telegram commands. Only the owner's chat (CHAT_ID) is answered.
#include "app.h"
#include "secrets.h"

static void telegramMessage(const String &chatId, const String &text);

static const char HELP_TEXT[] =
  "SAJAG commands\n\n"
  "Readings\n"
  "/now - live voltage, current, power\n"
  "/today - today's units and cost\n"
  "/month - this month's units and bill\n"
  "/bill - full bill breakdown\n"
  "/report - daily report right now\n\n"
  "Appliances\n"
  "/apps - list your appliances\n"
  "/addapp <name> <watts> - e.g. /addapp fridge 150\n"
  "/delapp <name>\n\n"
  "Theft check\n"
  "/compare <units> - units your board meter shows for this month\n\n"
  "Settings\n"
  "/settings - show all settings\n"
  "/setload <kW> - sanctioned load\n"
  "/setarea urban|rural\n"
  "/setslab <units> - slab limit (default 50)\n"
  "/setrates <low> <high> - paise per unit\n"
  "/setfixed <urbanLow> <urbanHigh> <ruralLow> <ruralHigh> - Rs per kW\n"
  "/setsubsidy <Rs> - monthly subsidy\n"
  "/setbudget <Rs> - monthly budget (0 = off)\n"
  "/setthreshold <W> - high load alert\n"
  "/setunknown <W> - unknown load alert size\n"
  "/setreport <hour> - daily report time (0-23)\n\n"
  "Calibration (use a multimeter)\n"
  "/calv <volts> - set true mains voltage\n"
  "/cali <amps> - set true current\n\n"
  "/resetmonth - start month count from zero";

void telegramPoll() {
  int n = bot.getUpdates(bot.last_message_received + 1);
  while (n) {
    for (int k = 0; k < n; k++) {
      telegramMessage(bot.messages[k].chat_id, bot.messages[k].text);
    }
    n = bot.getUpdates(bot.last_message_received + 1);
  }
}

// Splits "/cmd arg1 arg2" into words
static int splitWords(const String &text, String *out, int maxWords) {
  int count = 0, start = 0;
  String t = text;
  t.trim();
  while (count < maxWords && start < (int)t.length()) {
    int sp = t.indexOf(' ', start);
    if (sp < 0) sp = t.length();
    if (sp > start) out[count++] = t.substring(start, sp);
    start = sp + 1;
  }
  return count;
}

static void reply(const String &msg) { sendMsg(msg); }

static String nowText() {
  return "Live reading\nVoltage: " + fmt(live.vrms, 1) + " V\nCurrent: " + fmt(live.irms, 2) +
         " A\nPower: " + fmt(live.realW, 0) + " W\nApparent: " + fmt(live.apparentVA, 0) +
         " VA\nPower factor: " + fmt(live.pf, 2);
}

static String billText() {
  Bill b = calcBill(data.monthKwh, data.s);
  String msg = "Bill so far this month\n";
  msg += "Units: " + fmt(b.units, 1) + " kWh (" + (b.units <= data.s.slabUnits ? "up to " : "above ") +
         String(data.s.slabUnits) + " unit slab)\n";
  msg += "Energy: " + fmt(b.units, 1) + " x Rs " + fmt(b.ratePerUnit, 2) + " = Rs " + fmt(b.energyRs, 0) + "\n";
  msg += "Fixed charge: " + fmt(data.s.sanctionedKw, 1) + " kW (" + (data.s.isUrban ? "urban" : "rural") +
         ") = Rs " + fmt(b.fixedRs, 0) + "\n";
  if (b.subsidyRs > 0) msg += "Subsidy: -Rs " + fmt(b.subsidyRs, 0) + "\n";
  msg += "Total: Rs " + fmt(b.totalRs, 0);
  struct tm t;
  if (timeValid && getLocalTime(&t, 10)) {
    Bill p = calcBill(projectedMonthUnits(t), data.s);
    msg += "\n\nAt this rate the month ends near " + fmt(p.units, 0) + " kWh, Rs " + fmt(p.totalRs, 0);
  }
  return msg;
}

static String settingsText() {
  Settings &s = data.s;
  return "Settings\nSanctioned load: " + fmt(s.sanctionedKw, 1) + " kW\nArea: " + (s.isUrban ? "urban" : "rural") +
         "\nSlab: " + String(s.slabUnits) + " units\nRates: " + String(s.rateLowPaise) + " / " +
         String(s.rateHighPaise) + " paise per unit\nFixed (Rs/kW): urban " + String(s.fixedUrbanLow) + "/" +
         String(s.fixedUrbanHigh) + ", rural " + String(s.fixedRuralLow) + "/" + String(s.fixedRuralHigh) +
         "\nSubsidy: Rs " + String(s.subsidyRs) + "\nBudget: Rs " + String(s.budgetRs) +
         "\nHigh load alert: " + String(s.thresholdW) + " W\nUnknown load alert: " + String(s.unknownLoadW) +
         " W\nDaily report: " + String(s.reportHour) + ":00\nCalibration: V x" + fmt(s.voltCal, 4) + ", I x" +
         fmt(s.currCal, 4) + "\nLifetime energy: " + fmt(data.totalKwh, 1) + " kWh";
}

static String appsText() {
  if (data.applianceCount == 0) return "No appliances added yet. Example: /addapp fridge 150";
  String msg = "Your appliances\n";
  for (int k = 0; k < data.applianceCount; k++) {
    Appliance &a = data.appliances[k];
    msg += String(a.name) + ": " + String(a.watts) + " W" + (loadsIsOn(k) ? " (on now)" : "") + ", today " +
           fmt(a.todayKwh, 2) + " kWh\n";
  }
  return msg;
}

static int findApp(const String &name) {
  for (int k = 0; k < data.applianceCount; k++)
    if (name.equalsIgnoreCase(data.appliances[k].name)) return k;
  return -1;
}

static void telegramMessage(const String &chatId, const String &text) {
  if (chatId != CHAT_ID) {
    bot.sendMessage(chatId, "Sorry, this SAJAG meter only talks to its owner.", "");
    return;
  }

  String w[6];
  int n = splitWords(text, w, 6);
  if (n == 0) return;
  String cmd = w[0];
  cmd.toLowerCase();
  int at = cmd.indexOf('@');  // "/now@MyBot" in group chats
  if (at > 0) cmd = cmd.substring(0, at);

  bool changed = false;

  if (cmd == "/start" || cmd == "/help") {
    reply(HELP_TEXT);
  } else if (cmd == "/now") {
    reply(nowText());
  } else if (cmd == "/today") {
    Bill b = calcBill(data.monthKwh, data.s);
    reply("Today: " + fmt(data.todayKwh, 2) + " kWh, approx Rs " + fmt(data.todayKwh * b.ratePerUnit, 1) +
          "\nPeak load: " + fmt(data.todayPeakW, 0) + " W");
  } else if (cmd == "/month") {
    Bill b = calcBill(data.monthKwh, data.s);
    reply("This month: " + fmt(data.monthKwh, 1) + " kWh\nBill so far: Rs " + fmt(b.totalRs, 0));
  } else if (cmd == "/bill") {
    reply(billText());
  } else if (cmd == "/report") {
    struct tm t;
    if (timeValid && getLocalTime(&t, 10)) reply(dailyReport(t));
    else reply("Time is not synced yet, try again in a minute.");
  } else if (cmd == "/settings") {
    reply(settingsText());
  } else if (cmd == "/apps") {
    reply(appsText());

  } else if (cmd == "/addapp" && n >= 3) {
    int watts = w[2].toInt();
    if (watts < 10) {
      reply("Watts must be at least 10. Example: /addapp fridge 150");
    } else if (findApp(w[1]) >= 0) {
      Appliance &a = data.appliances[findApp(w[1])];
      a.watts = watts;
      changed = true;
      reply("Updated " + w[1] + " to " + String(watts) + " W");
    } else if (data.applianceCount >= MAX_APPLIANCES) {
      reply("Appliance list is full (" + String(MAX_APPLIANCES) + "). Remove one with /delapp first.");
    } else {
      Appliance &a = data.appliances[data.applianceCount++];
      memset(&a, 0, sizeof(a));
      strncpy(a.name, w[1].c_str(), sizeof(a.name) - 1);
      a.watts = watts;
      changed = true;
      reply("Added " + w[1] + " (" + String(watts) + " W). SAJAG will now recognise it switching on and off.");
    }
  } else if (cmd == "/delapp" && n >= 2) {
    int k = findApp(w[1]);
    if (k < 0) {
      reply("No appliance called " + w[1]);
    } else {
      for (int j = k; j < data.applianceCount - 1; j++) data.appliances[j] = data.appliances[j + 1];
      data.applianceCount--;
      loadsReset();
      changed = true;
      reply("Removed " + w[1]);
    }

  } else if (cmd == "/compare" && n >= 2) {
    // Compares the board meter's units with what SAJAG measured this month
    float board = w[1].toFloat();
    float mine = data.monthKwh;
    if (mine < 1) {
      reply("SAJAG has measured less than 1 unit this month, too little to compare yet.");
    } else {
      float diffPct = (board - mine) / mine * 100;
      String msg = "Board meter: " + fmt(board, 1) + " units\nSAJAG: " + fmt(mine, 1) + " units\nDifference: " +
                   (diffPct >= 0 ? "+" : "") + fmt(diffPct, 1) + "%\n\n";
      if (diffPct > data.s.theftPercent)
        msg += "WARNING: the board meter shows much more than you actually used. This can mean a connection "
               "tapped before SAJAG, a faulty or tampered meter, or a wrong reading. Report it to your "
               "electricity office with these numbers.";
      else if (diffPct < -(float)data.s.theftPercent)
        msg += "SAJAG measured more than the board meter. Check SAJAG's calibration with /calv and /cali.";
      else
        msg += "The readings match. No sign of theft or billing error.";
      reply(msg);
    }

  } else if (cmd == "/setload" && n >= 2) {
    data.s.sanctionedKw = w[1].toFloat();
    changed = true;
    reply("Sanctioned load set to " + fmt(data.s.sanctionedKw, 1) + " kW");
  } else if (cmd == "/setarea" && n >= 2) {
    String a = w[1];
    a.toLowerCase();
    data.s.isUrban = (a != "rural");
    changed = true;
    reply(String("Area set to ") + (data.s.isUrban ? "urban" : "rural"));
  } else if (cmd == "/setslab" && n >= 2) {
    data.s.slabUnits = w[1].toInt();
    changed = true;
    reply("Slab limit set to " + String(data.s.slabUnits) + " units");
  } else if (cmd == "/setrates" && n >= 3) {
    data.s.rateLowPaise = w[1].toInt();
    data.s.rateHighPaise = w[2].toInt();
    changed = true;
    reply("Rates set to " + w[1] + " / " + w[2] + " paise per unit");
  } else if (cmd == "/setfixed" && n >= 5) {
    data.s.fixedUrbanLow = w[1].toInt();
    data.s.fixedUrbanHigh = w[2].toInt();
    data.s.fixedRuralLow = w[3].toInt();
    data.s.fixedRuralHigh = w[4].toInt();
    changed = true;
    reply("Fixed charges updated");
  } else if (cmd == "/setsubsidy" && n >= 2) {
    data.s.subsidyRs = w[1].toInt();
    changed = true;
    reply("Monthly subsidy set to Rs " + w[1]);
  } else if (cmd == "/setbudget" && n >= 2) {
    data.s.budgetRs = w[1].toInt();
    data.budget80Sent = data.budgetSent = false;
    changed = true;
    reply(data.s.budgetRs ? "Budget set to Rs " + w[1] : String("Budget alerts turned off"));
  } else if (cmd == "/setthreshold" && n >= 2) {
    data.s.thresholdW = w[1].toInt();
    changed = true;
    reply("High load alert set to " + w[1] + " W");
  } else if (cmd == "/setunknown" && n >= 2) {
    data.s.unknownLoadW = w[1].toInt();
    changed = true;
    reply("Unknown load alert set to " + w[1] + " W");
  } else if (cmd == "/setreport" && n >= 2) {
    data.s.reportHour = constrain(w[1].toInt(), 0, 23);
    changed = true;
    reply("Daily report will come at " + String(data.s.reportHour) + ":00");

  } else if (cmd == "/calv" && n >= 2) {
    float actual = w[1].toFloat();
    if (live.vrms < 1) {
      reply("No voltage detected - check the ZMPT101B wiring first.");
    } else {
      data.s.voltCal *= actual / live.vrms;
      changed = true;
      reply("Voltage calibrated. New factor " + fmt(data.s.voltCal, 4));
    }
  } else if (cmd == "/cali" && n >= 2) {
    float actual = w[1].toFloat();
    if (live.irms < 0.1f) {
      reply("Switch on a load (a heater or kettle works best) and try again.");
    } else {
      data.s.currCal *= actual / live.irms;
      changed = true;
      reply("Current calibrated. New factor " + fmt(data.s.currCal, 4));
    }
  } else if (cmd == "/resetmonth") {
    data.monthKwh = 0;
    data.budget80Sent = data.budgetSent = false;
    changed = true;
    reply("Month count reset to zero.");
  } else {
    reply("I didn't understand that. Send /help for the list of commands.");
  }

  if (changed) storageSave();
}
