// Energy totals, day/month rollover, alerts and the daily report
#include "app.h"

static void hourFinished(int hour, float kwh);
static void newDay(const struct tm &t);
static void newMonth(const struct tm &t);
static void checkBudget(const struct tm &t);

static float hourKwh = 0;              // energy used so far in the current hour
static int currentHour = -1;
static int reportSentDay = -1;
static unsigned long overSinceMs = 0;  // when the load first went above the threshold
static unsigned long lastHighAlertMs = 0;
static unsigned long lastUnknownAlertMs = 0;
static unsigned long lastSpikeAlertMs = 0;

static bool cooledDown(unsigned long last) {
  return last == 0 || millis() - last >= ALERT_COOLDOWN_MS;
}

String fmt(float v, int decimals) { return String(v, decimals); }

static int daysInMonth(int month0, int year) {
  static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  return (month0 == 1 && leap) ? 29 : d[month0];
}

// Projected units for the full month at the current rate of use
float projectedMonthUnits(const struct tm &t) {
  float elapsedDays = (t.tm_mday - 1) + (t.tm_hour + t.tm_min / 60.0f) / 24.0f;
  if (elapsedDays < 0.5f) return data.monthKwh;
  return data.monthKwh / elapsedDays * daysInMonth(t.tm_mon, t.tm_year + 1900);
}

void monitorUpdate(const Reading &r, float dtSec) {
  float kwh = r.realW * dtSec / 3600000.0f;
  data.totalKwh += kwh;
  data.monthKwh += kwh;
  data.todayKwh += kwh;
  hourKwh += kwh;
  if (r.realW > data.todayPeakW) data.todayPeakW = r.realW;

  // --- Appliance steps ---
  StepEvent ev = loadsUpdate(r.realW, dtSec);
  if (ev.kind == STEP_UNKNOWN_ON && ev.deltaW >= data.s.unknownLoadW && cooledDown(lastUnknownAlertMs)) {
    lastUnknownAlertMs = millis();
    sendMsg("ALERT: an unrecognised load of about " + fmt(ev.deltaW, 0) +
            " W just switched on. If none of your appliances should be running, "
            "someone may be using your connection.\nTotal load now: " + fmt(r.realW, 0) + " W");
  }

  // --- Sustained high load ---
  if (r.realW > data.s.thresholdW) {
    if (overSinceMs == 0) overSinceMs = millis();
    if (millis() - overSinceMs > 60000UL && cooledDown(lastHighAlertMs)) {
      lastHighAlertMs = millis();
      sendMsg("ALERT: load has been above " + String(data.s.thresholdW) + " W for over a minute.\nNow: " +
              fmt(r.realW, 0) + " W, " + fmt(r.irms, 2) + " A");
    }
  } else {
    overSinceMs = 0;
  }

  // --- Things that need the real date/time ---
  struct tm t;
  if (!getLocalTime(&t, 10) || t.tm_year + 1900 < 2024) return;
  timeValid = true;

  if (data.lastDay < 0) {  // first time we know the date
    data.lastDay = t.tm_yday;
    data.lastMonth = t.tm_mon;
    data.lastYear = t.tm_year + 1900;
    currentHour = t.tm_hour;
  }

  if (currentHour < 0) currentHour = t.tm_hour;
  if (t.tm_hour != currentHour) {
    hourFinished(currentHour, hourKwh);
    hourKwh = 0;
    currentHour = t.tm_hour;
  }

  if (t.tm_yday != data.lastDay) newDay(t);
  if (t.tm_mon != data.lastMonth) newMonth(t);

  checkBudget(t);

  if (t.tm_hour == data.s.reportHour && reportSentDay != t.tm_yday) {
    if (sendMsg(dailyReport(t))) reportSentDay = t.tm_yday;
  }
}

// Compares the hour that just ended with what is normal for that hour
static void hourFinished(int hour, float kwh) {
  float &base = data.hourBaselineKwh[hour];
  if (data.learnedDays >= 3 && base > 0.01f && kwh > base * data.s.spikeFactor && kwh - base > 0.3f &&
      cooledDown(lastSpikeAlertMs)) {
    lastSpikeAlertMs = millis();
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:00", hour);
    sendMsg("Unusual usage: between " + String(buf) + " and the next hour you used " + fmt(kwh, 2) +
            " kWh, about " + fmt(kwh / base, 1) + "x your usual " + fmt(base, 2) +
            " kWh for that time.");
  }
  // Learn: moving average so the baseline follows slow changes in habits
  base = (data.learnedDays == 0 && base == 0) ? kwh : base * 0.8f + kwh * 0.2f;
}

static void newDay(const struct tm &t) {
  // Shift the 7-day history
  for (int k = 6; k > 0; k--) data.lastDays[k] = data.lastDays[k - 1];
  data.lastDays[0] = data.todayKwh;
  if (data.learnedDays < 255) data.learnedDays++;

  data.todayKwh = 0;
  data.todayPeakW = 0;
  loadsNewDay();
  data.lastDay = t.tm_yday;
  storageSave();
}

static void newMonth(const struct tm &t) {
  Bill b = calcBill(data.monthKwh, data.s);
  sendMsg("Month finished.\nUnits used: " + fmt(data.monthKwh, 1) + " kWh\nEstimated bill: Rs " +
          fmt(b.totalRs, 0) + "\nCompare this with your electricity bill. Starting a new month now.");
  data.monthKwh = 0;
  data.budget80Sent = false;
  data.budgetSent = false;
  data.lastMonth = t.tm_mon;
  data.lastYear = t.tm_year + 1900;
  storageSave();
}

static void checkBudget(const struct tm &t) {
  if (data.s.budgetRs == 0) return;
  float billNow = calcBill(data.monthKwh, data.s).totalRs;
  if (!data.budgetSent && billNow >= data.s.budgetRs) {
    data.budgetSent = true;
    data.budget80Sent = true;
    sendMsg("Budget alert: this month's bill has reached Rs " + fmt(billNow, 0) + ", your budget is Rs " +
            String(data.s.budgetRs) + ".");
    storageSave();
  } else if (!data.budget80Sent && billNow >= 0.8f * data.s.budgetRs) {
    data.budget80Sent = true;
    float projected = calcBill(projectedMonthUnits(t), data.s).totalRs;
    sendMsg("Heads up: you have used 80% of your Rs " + String(data.s.budgetRs) +
            " budget (Rs " + fmt(billNow, 0) + "). At this rate the month will end near Rs " +
            fmt(projected, 0) + ".");
    storageSave();
  }
}

static float weekAverage() {
  float sum = 0;
  int n = 0;
  for (int k = 0; k < 7; k++) {
    if (data.lastDays[k] > 0) {
      sum += data.lastDays[k];
      n++;
    }
  }
  return n ? sum / n : 0;
}

static String applianceBreakdown() {
  String s;
  float known = 0;
  for (int k = 0; k < data.applianceCount; k++) {
    Appliance &a = data.appliances[k];
    if (a.todayOnSec == 0) continue;
    known += a.todayKwh;
    s += "  " + String(a.name) + ": " + fmt(a.todayKwh, 2) + " kWh (" + fmt(a.todayOnSec / 3600.0f, 1) +
         " h)\n";
  }
  float other = data.todayKwh - known;
  if (other < 0) other = 0;
  if (s.length() == 0 && data.applianceCount == 0) return "  Add your appliances with /addapp to see a breakdown.\n";
  s += "  Other / unrecognised: " + fmt(other, 2) + " kWh\n";
  return s;
}

String dailyReport(const struct tm &t) {
  Bill month = calcBill(data.monthKwh, data.s);
  Bill projected = calcBill(projectedMonthUnits(t), data.s);
  float avg = weekAverage();

  String msg = "SAJAG daily report\n\n";
  msg += "Today: " + fmt(data.todayKwh, 2) + " kWh (approx Rs " + fmt(data.todayKwh * month.ratePerUnit, 0) + ")\n";
  msg += "Peak load: " + fmt(data.todayPeakW, 0) + " W\n";
  if (avg > 0) {
    float change = (data.todayKwh - avg) / avg * 100;
    msg += "7-day average: " + fmt(avg, 2) + " kWh (" + (change >= 0 ? "+" : "") + fmt(change, 0) + "%)\n";
    if (data.todayKwh > avg * 1.5f && data.todayKwh - avg > 1)
      msg += "Note: today was much higher than usual. Check for appliances left on or unauthorised use.\n";
  }
  msg += "\nWhere it went:\n" + applianceBreakdown();
  uint16_t unknown = loadsUnknownCountToday();
  if (unknown) msg += "Unrecognised switch-ons today: " + String(unknown) + "\n";

  msg += "\nThis month: " + fmt(data.monthKwh, 1) + " kWh, bill so far Rs " + fmt(month.totalRs, 0) + "\n";
  msg += "Projected month: " + fmt(projectedMonthUnits(t), 0) + " kWh, about Rs " + fmt(projected.totalRs, 0);
  return msg;
}
