#include <SPI.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>

#define TIMER_INTERRUPT_DEBUG     0
#define _TIMERINTERRUPT_LOGLEVEL_ 0

#define USE_TIMER_1 true
#define USE_TIMER_2 false
#define USE_TIMER_3 false
#define USE_TIMER_4 false
#define USE_TIMER_5 false

#include <TimerInterrupt.h>

#define SCREEN_WIDTH        128 // OLED display width, in pixels
#define SCREEN_HEIGHT        64 // OLED display height, in pixels
#define OLED_RESET            4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS     0x3C // See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define PUSH_BUTTON           2
#define DEBOUNCE_DELAY_MS    50
#define TIMER1_INTERVAL_MS   25
#define TIMER1_DURATION_MS    0 //(10 * TIMER1_INTERVAL_MS) // Duration = 0 or not specified => run indefinitely
#define DATE_FORMAT_LEN      24
#define TIME_FORMAT_NULL_POS 23
#define DATE_FORMAT_NULL_POS 11
#define MARQUEE_MAX_LENGTH  100

volatile int xpos = SCREEN_WIDTH - 1;
volatile bool isInterrupted = false;
volatile int scrollWidth;

const DateTime TargetDateTime = DateTime(2030, 10, 25, 17, 0, 0); // 25-Oct-2030 5:00:00PM
const uint8_t daysInMonth[] PROGMEM = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static char marquee[MARQUEE_MAX_LENGTH];

int displayFormat     = 0;
int buttonState       = LOW;
int lastButtonState   = LOW;
long lastDebounceTime = 0;

struct DateTimeSpan
{
  byte Years;
  byte Months;
  byte Days;
  byte Hours;
  byte Minutes;
  byte Seconds;
};

RTC_DS3231 Clock;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    abort;
  }

  if(!Clock.begin()) {
    abort;
  }

  pinMode(PUSH_BUTTON, INPUT_PULLUP);

  ITimer1.init();
  ITimer1.attachInterruptInterval(TIMER1_INTERVAL_MS, marquee_ISR, TIMER1_DURATION_MS);

  // uncomment to set time
  //Clock.adjust(DateTime(2021, 4, 25, 17, 03, 50));
}

void loop() {
  checkButtonPress();
  
  display.clearDisplay();
  display.setTextWrap(false);
  
  DateTime curDateTime = Clock.now();
  TimeSpan duration = TimeSpan(TargetDateTime - curDateTime);
  DateTimeSpan timeSpan = CompareDates(curDateTime, TargetDateTime, duration);

  showMarquee(curDateTime, duration, timeSpan);
  showCurrentTime(curDateTime, duration);
  
  display.display();
}

void marquee_ISR()
{
  if(!isInterrupted)
  {
    isInterrupted = true;
    if(xpos > -scrollWidth)
    {
      xpos--;
    }
    else
    {
      xpos = SCREEN_WIDTH - 1;
    }
    isInterrupted = false;
  }
}

void showMarquee(DateTime curDateTime, TimeSpan duration, DateTimeSpan timeSpan)
{
  int x1;
  int y1;
  unsigned int w;
  unsigned int h;
  int months;
  int32_t totalHours;
  int32_t totalMinutes;
  int len;
  char *tmpPtr = &marquee[0];

  switch(displayFormat)
  {
    case 0:
      tmpPtr += itoa(timeSpan.Years, tmpPtr);
      copyString(tmpPtr, timeSpan.Years == 1 ? " Year" : " Years");
      break;
  
    case 1:
      months = (timeSpan.Years * 12) + timeSpan.Months;
      tmpPtr += itoa(months, tmpPtr);
      copyString(tmpPtr, months == 1 ? " Month" : " Months");
      break;

    case 2:
      tmpPtr += itoa(duration.days(), tmpPtr);
      copyString(tmpPtr, duration.days() == 1 ? " Day" : " Days");
      break;
    
    case 3:
      totalHours = (int32_t)((duration.totalseconds() / 60L) / 60L);
      tmpPtr += itoa(totalHours, tmpPtr);
      copyString(tmpPtr, totalHours == 1 ? " Hour" : " Hours");
      break;
    
    case 4:
      totalMinutes = (int32_t)(duration.totalseconds() / 60L);
      tmpPtr += itoa(totalMinutes, tmpPtr);
      copyString(tmpPtr, totalMinutes == 1 ? " Minute" : " Minutes");
      break;
    
    case 5:
      tmpPtr += itoa(duration.totalseconds(), tmpPtr);
      copyString(tmpPtr, duration.totalseconds() == 1 ? " Second" : " Seconds");
      break;
  }
  
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.getTextBounds(marquee, 0, 15, &x1, &y1, &w, &h);
  scrollWidth = w; // this is needed in order to calculate negative values because w is an unsigned int
  display.setCursor(xpos, 12);
  display.print(marquee);
}

void showCurrentTime(DateTime curDateTime, TimeSpan duration)
{
  char *dateTime;
  display.setFont();
  display.setTextSize(1);
  
  dateTime = getDateFormatted(curDateTime);
  display.setCursor(63, 17);
  display.print(dateTime + DATE_FORMAT_NULL_POS + 1);
  display.setCursor(63, 25);
  display.print(dateTime);

  dateTime = getDateFormatted(TargetDateTime);
  display.setCursor(63, 47);
  display.print(dateTime + DATE_FORMAT_NULL_POS + 1);
  display.setCursor(63, 55);
  display.print(dateTime);

  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);
  display.setCursor(0, 29);
  display.print("Today:");
  display.setCursor(18, 59);
  display.print("End:");
}

char *getDateFormatted(DateTime dateTime)
{
  static char dateFormat[DATE_FORMAT_LEN];
  strcpy(dateFormat, "DD-MMM-YYYY hh:mm:ss AP");
  dateTime.toString(dateFormat);
  dateFormat[DATE_FORMAT_NULL_POS] = 0x00;
  dateFormat[TIME_FORMAT_NULL_POS] = 0x00;
  return dateFormat;
}

DateTimeSpan CompareDates(DateTime date1, DateTime date2, TimeSpan curTimeSpan)
{
  DateTimeSpan span;

  span.Years = (date2.year() - date1.year()) - (date1.month() > date2.month() ? 1 : 0);
  span.Months = (date2.month() - date1.month()) - (date1.day() > date2.day() ? 1 : 0);
  span.Days = date2.day() >= date1.day()
    ? (date2.day() - date1.day())
    : (pgm_read_byte(daysInMonth + date1.month() - 1) - date1.day()) + date2.day();
  if((date2.year() % 4 == 0) && date1.month() == 2)
  {
    span.Days++;
  }
  span.Hours = curTimeSpan.hours();
  span.Minutes = curTimeSpan.minutes();
  span.Seconds = curTimeSpan.seconds();
  
  return span;
}

void checkButtonPress()
{
  int reading = digitalRead(PUSH_BUTTON);
  if(reading != lastButtonState)
  {
    lastDebounceTime = millis();
  }
  if(millis() - lastDebounceTime > DEBOUNCE_DELAY_MS)
  {
    if(reading != buttonState)
    {
      buttonState = reading;
      if(buttonState == HIGH)
      {
        displayFormat++;
        if(displayFormat > 5)
        {
          displayFormat = 0;
        }
      }
    }
  }
  lastButtonState = reading;
}

char *copyString(char *destination, const char *source)
{
  while(*source)
  {
    *destination++ = *source++;
  }
  *destination = 0x00;

  return &destination[0];
}

int itoa(int32_t number, char *str)
{
  int index = 0;
  byte count = 0;
  while(number != 0)
  {
    int remainder = number % 10;
    str[index++] = remainder + '0';
    number = (int32_t)(number / 10L);
    count++;
    if(count % 3 == 0 && number != 0)
    {
      str[index++] = ',';
      count = 0;
    }
  }
  str[index] = 0x00;
  reverse(str, index);
  return index;
}

void reverse(char str[], int len)
{
  int start = 0;
  int stop = len - 1;
  byte tmp;
  while(start < stop)
  {
    tmp = str[start];
    str[start] = str[stop];
    str[stop] = tmp;
    start++;
    stop--;
  }
}
