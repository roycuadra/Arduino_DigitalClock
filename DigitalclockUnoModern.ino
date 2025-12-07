#include <Wire.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <Adafruit_AHTX0.h>
#include <avr/wdt.h> // Watchdog for auto-reset

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 10

MD_Parola display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// DS1302 pins
#define RTC_DAT 4
#define RTC_CLK 5
#define RTC_RST 6

ThreeWire myWire(RTC_DAT, RTC_CLK, RTC_RST);
RtcDS1302<ThreeWire> rtc(myWire);

Adafruit_AHTX0 aht;

unsigned long lastTempUpdate = 0;
const unsigned long tempInterval = 1000;

int tempC = 0;
int hum = 0;

enum Pages {TIME_PAGE, DAY_PAGE, DATE_PAGE, TEMP_PAGE, GREETING_PAGE};
Pages currentPage = TIME_PAGE;
unsigned long pageStart = 0;

// Different durations per page (in milliseconds)
const unsigned long pageDurations[] = {
  2500, // TIME_PAGE
  1500, // DAY_PAGE
  2000, // DATE_PAGE
  1500, // TEMP_PAGE
  5000  // GREETING_PAGE
};

void setup() {
  Wire.begin();
  display.begin();
  display.setIntensity(0);
  display.setFont(nullptr);
  display.displayClear();

  rtc.Begin();

  // Uncomment first upload to set time, then comment it back
  // rtc.SetIsWriteProtected(false);
  // rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
  // rtc.SetIsWriteProtected(true);

  aht.begin();

  pageStart = millis();
  preparePage(currentPage);
  fadeIn();
}

void updateTempHum() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  tempC = (int)temp.temperature;
  hum   = (int)humidity.relative_humidity;
  lastTempUpdate = millis();
}

bool isGreetingTime() {
  RtcDateTime now = rtc.GetDateTime();
  int hour = now.Hour();
  return (hour >= 5 && hour < 10) ||  // Morning
         (hour >= 12 && hour < 14) || // Afternoon
         (hour >= 18 && hour < 20);   // Evening
}

const char* getGreeting() {
  RtcDateTime now = rtc.GetDateTime();
  int hour = now.Hour();
  if (hour >= 5 && hour < 10) return "GOOD MORNING";
  if (hour >= 12 && hour < 14) return "GOOD AFTERNOON";
  if (hour >= 18 && hour < 20) return "GOOD EVENING";
  return "";
}

void preparePage(Pages page) {
  RtcDateTime now = rtc.GetDateTime();
  const char *ampm = (now.Hour() < 12) ? "AM" : "PM";
  int hour12 = now.Hour() % 12;
  if (hour12 == 0) hour12 = 12;

  const char* daysOfWeek[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
  const char* months[] = {"JAN","FEB","MAR","APR","MAY","JUN",
                          "JUL","AUG","SEP","OCT","NOV","DEC"};

  char buffer[32];
  display.displayClear();

  switch(page) {
    case TIME_PAGE:
      sprintf(buffer, "%d:%02d%s", hour12, now.Minute(), ampm);
      display.displayText(buffer, PA_CENTER, 0, pageDurations[page], PA_NO_EFFECT, PA_NO_EFFECT);
      break;

    case DAY_PAGE:
      sprintf(buffer, "%s", daysOfWeek[now.DayOfWeek()]);
      display.displayText(buffer, PA_CENTER, 0, pageDurations[page], PA_NO_EFFECT, PA_NO_EFFECT);
      break;

    case DATE_PAGE:
      sprintf(buffer, "%s %d", months[now.Month()-1], now.Day());
      display.displayText(buffer, PA_CENTER, 0, pageDurations[page], PA_NO_EFFECT, PA_NO_EFFECT);
      break;

    case TEMP_PAGE:
      sprintf(buffer, "%d  %d", tempC, hum);
      display.displayText(buffer, PA_CENTER, 0, pageDurations[page], PA_NO_EFFECT, PA_NO_EFFECT);
      break;

    case GREETING_PAGE: {
      if (isGreetingTime()) {
        const char* message = getGreeting();
        display.displayText(message, PA_CENTER, 50, pageDurations[page], PA_SCROLL_LEFT, PA_SCROLL_LEFT);
      }
      break;
    }
  }

  display.displayAnimate();
}

void fadeIn() {
  for (int i = 0; i <= 2; i++) {
    display.setIntensity(i);
    display.displayAnimate();
    delay(100);
  }
}

void fadeOut() {
  for (int i = 2; i >= 0; i--) {
    display.setIntensity(i);
    display.displayAnimate();
    delay(100);
  }
}

void checkAutoRestart() {
  RtcDateTime now = rtc.GetDateTime();
  if (now.Hour() == 3 && now.Minute() == 0 && now.Second() == 0) {
    wdt_enable(WDTO_15MS); // Trigger watchdog for immediate reset
    while(1); // Wait for reset
  }
}

void loop() {
  if (millis() - lastTempUpdate >= tempInterval) {
    updateTempHum();
  }

  display.displayAnimate();

  // Check for auto restart at 3:00AM
  checkAutoRestart();

  // Determine next page
  if (millis() - pageStart >= pageDurations[currentPage]) {
      fadeOut();

      // Skip GREETING_PAGE if not greeting time
      do {
        currentPage = (Pages)((currentPage + 1) % 5);
      } while (currentPage == GREETING_PAGE && !isGreetingTime());

      preparePage(currentPage);
      fadeIn();
      pageStart = millis();
  }
}
