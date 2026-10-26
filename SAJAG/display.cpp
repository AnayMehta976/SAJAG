// 16x2 LCD: cycles through a few pages every LCD_PAGE_MS
#include "app.h"

static unsigned long lastPageMs = 0;
static int page = 0;

void lcdShow(const char *line1, const char *line2) {
  char a[17], b[17];
  snprintf(a, sizeof(a), "%-16s", line1);
  snprintf(b, sizeof(b), "%-16s", line2);
  lcd.setCursor(0, 0);
  lcd.print(a);
  lcd.setCursor(0, 1);
  lcd.print(b);
}

void lcdUpdate() {
  if (millis() - lastPageMs < LCD_PAGE_MS) return;
  lastPageMs = millis();

  char l1[24], l2[24];
  Bill b = calcBill(data.monthKwh, data.s);

  switch (page) {
    case 0:  // live readings
      snprintf(l1, sizeof(l1), "V:%5.1f I:%5.2fA", live.vrms, live.irms);
      snprintf(l2, sizeof(l2), "P:%5.0fW PF:%4.2f", live.realW, live.pf);
      break;
    case 1:  // today
      snprintf(l1, sizeof(l1), "Today:%6.2fkWh", data.todayKwh);
      snprintf(l2, sizeof(l2), "Cost: Rs%6.1f", data.todayKwh * b.ratePerUnit);
      break;
    case 2:  // month and bill
      snprintf(l1, sizeof(l1), "Month:%6.1fkWh", data.monthKwh);
      snprintf(l2, sizeof(l2), "Bill: Rs%6.0f", b.totalRs);
      break;
    default: {  // status
      struct tm t;
      if (timeValid && getLocalTime(&t, 5))
        snprintf(l1, sizeof(l1), "%02d:%02d   %02d/%02d", t.tm_hour, t.tm_min, t.tm_mday, t.tm_mon + 1);
      else
        snprintf(l1, sizeof(l1), "Time not synced");
      snprintf(l2, sizeof(l2), "WiFi:%s", WiFi.status() == WL_CONNECTED ? "OK" : "offline");
    }
  }
  lcdShow(l1, l2);
  page = (page + 1) % 4;
}
