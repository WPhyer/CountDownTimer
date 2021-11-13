#include <SPI.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>
#include "MultiFunctionButton.h"

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
#define PUSH_BUTTON_ADJUST    4
#define DEBOUNCE_DELAY_MS    50
#define TIMER1_INTERVAL_MS   25
#define TIMER1_DURATION_MS    0 //(10 * TIMER1_INTERVAL_MS) // Duration = 0 or not specified => run indefinitely
#define DATE_FORMAT_LEN      24
#define TIME_FORMAT_NULL_POS 23
#define DATE_FORMAT_NULL_POS 11
#define MARQUEE_MAX_LENGTH  100
#define MAX_DISPLAY_LEVEL     7
#define MAX_YEAR           2050
#define MIN_YEAR           2000

#define ADJUST_IDLE   0
#define ADJUST_HOUR   1
#define ADJUST_MINUTE 2
#define ADJUST_SECOND 3
#define ADJUST_DAY    4
#define ADJUST_MONTH  5
#define ADJUST_YEAR   6

volatile int xpos = SCREEN_WIDTH - 1;
volatile bool isInterrupted = false;
volatile int scrollWidth;

const DateTime TargetDateTime = DateTime(2030, 10, 25, 17, 0, 0); // 25-Oct-2030 5:00:00PM
const int8_t daysInMonth[] PROGMEM = {31, -1, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static char marquee[MARQUEE_MAX_LENGTH];
const char *monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

bool scroll           = 1;
byte displayFormat    = 0;
int buttonState       = LOW;
int lastButtonState   = LOW;
long lastDebounceTime = 0;
int adjustState       = ADJUST_IDLE;
int adjustTime[]      = {2000, 1, 1, 0, 0, 0};

struct DateTimeSpan
{
  int8_t Years;
  int8_t Months;
  int8_t Days;
  int8_t Hours;
  int8_t Minutes;
  int8_t Seconds;

  int32_t TotalYears;
  int32_t TotalMonths;
  int32_t TotalDays;
  int32_t TotalHours;
  int32_t TotalMinutes;
  int32_t TotalSeconds;
};

RTC_DS3231 Clock;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MultiFunctionButton buttonAdjust;

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

  buttonAdjust.configure(4, PULL_DOWN, onPressAdjust, NULL, onHoldAdjust);
  buttonAdjust.debounce = DEBOUNCE_DELAY_MS;
  buttonAdjust.longHoldDelay = 750;
}

void loop() {
  buttonAdjust.check();
  
  if(adjustState == ADJUST_IDLE)
  {
    checkButtonPress();
    
    display.clearDisplay();
    display.setTextWrap(false);
    
    DateTime curDateTime = Clock.now();
    DateTimeSpan timeDateSpan = getAllTimes(curDateTime, TargetDateTime);
  
    showMarquee(timeDateSpan);
    showCurrentTime(curDateTime);
    
    display.display();
  }
}

void marquee_ISR()
{
  if(!isInterrupted && scroll)
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

void showMarquee(DateTimeSpan dateTimeSpan)
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
      scroll = true;
      setDurationText(tmpPtr, dateTimeSpan.Years, displayFormat);
      break;
  
    case 1:
      setDurationText(tmpPtr, dateTimeSpan.TotalMonths, displayFormat);
      break;

    case 2:
      setDurationText(tmpPtr, dateTimeSpan.TotalDays, displayFormat);
      break;
    
    case 3:
      setDurationText(tmpPtr, dateTimeSpan.TotalHours, displayFormat);
      break;
    
    case 4:
      setDurationText(tmpPtr, dateTimeSpan.TotalMinutes, displayFormat);
      break;
    
    case 5:
      setDurationText(tmpPtr, dateTimeSpan.TotalSeconds, displayFormat);
      break;
  
    case 6:
      if(dateTimeSpan.Years > 0)
      {
        tmpPtr = setDurationText(tmpPtr, dateTimeSpan.Years, 0);
        tmpPtr += setStaticPeriodGap(tmpPtr);
      }
      if(dateTimeSpan.Months > 0)
      {
        tmpPtr = setDurationText(tmpPtr, dateTimeSpan.Months, 1);
        tmpPtr += setStaticPeriodGap(tmpPtr);
      }
      if(dateTimeSpan.Days > 0)
      {
        tmpPtr = setDurationText(tmpPtr, dateTimeSpan.Days, 2);
        tmpPtr += setStaticPeriodGap(tmpPtr);
      }
      if(dateTimeSpan.Hours > 0)
      {
        tmpPtr = setDurationText(tmpPtr, dateTimeSpan.Hours, 3);
        tmpPtr += setStaticPeriodGap(tmpPtr);
      }
      if(dateTimeSpan.Minutes > 0)
      {
        tmpPtr = setDurationText(tmpPtr, dateTimeSpan.Minutes, 4);
        tmpPtr += setStaticPeriodGap(tmpPtr);
      }
      tmpPtr = setDurationText(tmpPtr, dateTimeSpan.Seconds, 5);
      tmpPtr += setStaticPeriodGap(tmpPtr);
      break;

    case 7:
      scroll = false;
      tmpPtr += itoax(dateTimeSpan.Years, tmpPtr, true, true);
      tmpPtr += setStaticColon(tmpPtr);
      tmpPtr += itoax(dateTimeSpan.Months, tmpPtr, true, true);
      tmpPtr += setStaticColon(tmpPtr);
      tmpPtr += itoax(dateTimeSpan.Days, tmpPtr, true, true);
      tmpPtr += setStaticColon(tmpPtr);
      tmpPtr += itoax(dateTimeSpan.Hours, tmpPtr, true, true);
      tmpPtr += setStaticColon(tmpPtr);
      tmpPtr += itoax(dateTimeSpan.Minutes, tmpPtr, true, true);
      tmpPtr += setStaticColon(tmpPtr);
      tmpPtr += itoax(dateTimeSpan.Seconds, tmpPtr, true, true);
      break;
      
    default:
      copyString(tmpPtr, "Huh?");
      break;
  }
  
  if(scroll)
  {
    display.setFont(&FreeSans9pt7b);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.getTextBounds(marquee, 0, 15, &x1, &y1, &w, &h);
    scrollWidth = w; // this is needed in order to calculate negative values because w is an unsigned int
    display.setCursor(xpos, 12);
  }
  else
  {
    display.fillRect(0, 0, SCREEN_WIDTH, 16, SSD1306_WHITE);
    display.setFont();
    display.setTextSize(1);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.getTextBounds(marquee, 0, 15, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 4);
  }
  display.print(marquee);
}

char *setDurationText(char *tmpPtr, int32_t value, byte durationType)
{
   bool isSingle;
  
  isSingle = value == 1;

  tmpPtr += itoax(value, tmpPtr, false, true);
  
  switch(durationType)
  {
    case 0:
      return copyString(tmpPtr, isSingle ? " Year" : " Years");
    case 1:
      return copyString(tmpPtr, isSingle ? " Month" : " Months");
    case 2:
      return copyString(tmpPtr, isSingle ? " Day" : " Days");
    case 3:
      return copyString(tmpPtr, isSingle ? " Hour" : " Hours");
    case 4:
      return copyString(tmpPtr, isSingle ? " Minute" : " Minutes");
    case 5:
      return copyString(tmpPtr, isSingle ? " Second" : " Seconds");
    default:
      return tmpPtr;
  }
}

int setStaticPeriodGap(char *value)
{
  value[0] = '.';
  value[1] = '.';
  value[2] = '.';
  value[3] = ' ';
  value[4] = 0x00;
  
  return 4;
}

int setStaticColon(char *value)
{
  value[0] = ':';
  value[1] = 0x00;

  return 1;
}

void showCurrentTime(DateTime curDateTime)
{
  char *dateTime;
  display.setFont();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
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

void showUpdateTime(bool resetCursor)
{
  char tmpValue[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
  int x, y;
  DateTime updateTime;
  
  if(resetCursor)
  {
      updateTime = DateTime(
        adjustTime[0],
        adjustTime[1],
        adjustTime[2],
        adjustTime[3],
        adjustTime[4],
        adjustTime[5]);

      display.fillRect(0, 16, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_BLACK);
      showCurrentTime(updateTime);
      return;
  }
  
  switch(adjustState)
  {
    case ADJUST_HOUR:
      itoax(adjustTime[3], tmpValue, true, false);
      x = 63; y = 17;
      break;
   
    case ADJUST_MINUTE:
      itoax(adjustTime[4], tmpValue, true, false);
      x = 81; y = 17;
      break;
   
    case ADJUST_SECOND:
      itoax(adjustTime[5], tmpValue, true, false);
      x = 99; y = 17;
      break;
   
    case ADJUST_MONTH:
      copyString(tmpValue, monthNames[adjustTime[1] - 1]);
      x = 81; y = 25;
      break;
   
    case ADJUST_DAY:
      itoax(adjustTime[2], tmpValue, true, false);
      x = 63; y = 25;
      break;
   
    case ADJUST_YEAR:
      itoax(adjustTime[0], tmpValue, true, false);
      x = 105; y = 25;
      break;
  }

  display.setFont();
  display.setTextSize(1);
  if(adjustState == ADJUST_HOUR)
  {
    display.fillRect(117, 17, 12, 8, SSD1306_BLACK);
  }
  display.fillRect(x - 1, y - 1, adjustState == ADJUST_MONTH ? 19 : adjustState == ADJUST_YEAR ? 25 : 13, 9, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setCursor(x, y);
  display.print(tmpValue);
  display.display();
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

DateTimeSpan getAllTimes(DateTime fromDate, DateTime toDate)
{
  DateTimeSpan dateTimeSpan;
  TimeSpan timeSpan = TimeSpan(toDate - fromDate);

  int8_t years = 0;
  int8_t months = 0;
  int8_t days = 0;
  int8_t hours = 0;
  int8_t minutes = 0;
  int8_t seconds = 0 ;

  byte minuteIncrement = 0;
  byte hourIncrement = 0;
  bool addDay = false;

  if(fromDate.second() > toDate.second())
  {
    seconds = 60 - (fromDate.second() - toDate.second());
    minuteIncrement = 1;
  }
  else
  {
    seconds = toDate.second() - fromDate.second();
  }

  if(fromDate.minute() + minuteIncrement > toDate.minute())
  {
    minutes = 60 - ((fromDate.minute() + minuteIncrement) - toDate.minute());
    hourIncrement = 1;
  }
  else
  {
    minutes = toDate.minute() - fromDate.minute();
  }

  if(fromDate.hour() + hourIncrement > toDate.hour())
  {
    hours = 24 - ((fromDate.hour() + hourIncrement) - toDate.hour());
    addDay = true;
  }
  else
  {
    hours = toDate.hour() - fromDate.hour() - hourIncrement;
  }

  int8_t newCurrentDay = fromDate.day();
  int8_t newCurrentMonth = fromDate.month();
  int8_t newCurrentYear = fromDate.year();

  if(addDay)
  {
    bool monthOverflow = (fromDate.day() + 1) > getDaysInMonth(fromDate.month(), fromDate.year());
    newCurrentDay = monthOverflow ? 1 : fromDate.day() + 1;
    newCurrentMonth = monthOverflow ? fromDate.month() + 1 : fromDate.month();
    if(newCurrentMonth > 12)
    {
      newCurrentMonth = 1;
    }
    newCurrentYear = fromDate.month() == 12 && monthOverflow ? fromDate.year() + 1 : fromDate.year();
  }

  years = toDate.year() - newCurrentYear;
  months = toDate.month() - newCurrentMonth;
  days = toDate.day() - newCurrentDay;

  bool dayOverflow = newCurrentDay > toDate.day();
  bool dateOverflow = (newCurrentMonth == toDate.month() && newCurrentDay > toDate.day()) || newCurrentMonth > toDate.month();

  if(dayOverflow)
  {
    days = getDaysInMonth(fromDate.month(), fromDate.year()) - (newCurrentDay - toDate.day());
  }

  if(dateOverflow)
  {
    years = years > 0 ? years - 1 : years;
    months = 12 - (newCurrentMonth - toDate.month()) - (dayOverflow ? 1 : 0);
  }

  dateTimeSpan.Years = years;
  dateTimeSpan.Months = months;
  dateTimeSpan.Days = days;
  dateTimeSpan.Hours = hours;
  dateTimeSpan.Minutes = minutes;
  dateTimeSpan.Seconds = seconds;
  
  dateTimeSpan.TotalSeconds = timeSpan.totalseconds();
  dateTimeSpan.TotalMinutes = dateTimeSpan.TotalSeconds / 60;
  dateTimeSpan.TotalHours = dateTimeSpan.TotalSeconds / 3600;
  dateTimeSpan.TotalDays = timeSpan.days();
  dateTimeSpan.TotalMonths = (years * 12) + months;
  dateTimeSpan.TotalYears = years;

  return dateTimeSpan;
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
        if(displayFormat > MAX_DISPLAY_LEVEL)
        {
          displayFormat = 0;
        }
      }
    }
  }
  lastButtonState = reading;
}

bool isLeapYear(int8_t year)
{
  return (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0)) ? true : false;
}

int8_t getDaysInMonth(int8_t month, int8_t year)
{
  int8_t days = pgm_read_byte(daysInMonth + (month - 1));
  return (days == -1)
    ? isLeapYear(year)
      ? 29
      : 28
    : days;
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

int itoax(int32_t number, char *str, bool leadingZero, bool hasComma)
{
  if(number < 10)
  {
    str[0] = leadingZero ? '0' : number + '0';
    str[1] = leadingZero ? number + '0' : 0x00;
    str[2] = 0x00;
    return leadingZero ? 2 : 1;
  }
  
  int index = 0;
  byte count = 0;
  while(number != 0)
  {
    int remainder = number % 10;
    str[index++] = remainder + '0';
    number = (int32_t)(number / 10L);
    if(hasComma)
    {
      count++;
      if(count % 3 == 0 && number != 0)
      {
        str[index++] = ',';
        count = 0;
      }
    }
  }
  str[index] = 0x00;
  if(index > 1)
  {
    reverse(str, index);
  }
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

void onPressAdjust(int pin)
{
  int monthDays;

  if(adjustState == ADJUST_IDLE)
  {
    return;
  }
  
  switch(adjustState)
  {
    case ADJUST_HOUR:
      adjustTime[3] = adjustTime[3] == 23 ? 0 : adjustTime[3] + 1;
      break;

    case ADJUST_MINUTE:
      adjustTime[4] = adjustTime[4] == 59 ? 0 : adjustTime[4] + 1;
      break;

    case ADJUST_SECOND:
      adjustTime[5] = adjustTime[5] == 59 ? 0 : adjustTime[5] + 1;
      break;

    case ADJUST_MONTH:
      adjustTime[1] = adjustTime[1] == 12 ? 1 : adjustTime[1] + 1;
      monthDays = getDaysInMonth(adjustTime[1], adjustTime[0]);
      if(adjustTime[2] > monthDays)
      {
        adjustTime[2] = monthDays;
      }
      break;
    
    case ADJUST_DAY:
      adjustTime[2] = adjustTime[2] >= getDaysInMonth(adjustTime[1], adjustTime[0]) ? 1 : adjustTime[2] + 1;
      break;

    case ADJUST_YEAR:
      adjustTime[0] = adjustTime[0] >= MAX_YEAR ? MIN_YEAR : adjustTime[0] + 1;
      monthDays = getDaysInMonth(adjustTime[1], adjustTime[0]);
      if(adjustTime[2] > monthDays)
      {
        adjustTime[2] = monthDays;
      }
      break;
  }

  showUpdateTime(false);
}

void onHoldAdjust(int pin)
{
  DateTime currentTime;

  switch(adjustState)
  {
    case ADJUST_IDLE:
      currentTime = Clock.now();
      adjustTime[0] = currentTime.year();
      adjustTime[1] = currentTime.month();
      adjustTime[2] = currentTime.day();
      adjustTime[3] = currentTime.hour();
      adjustTime[4] = currentTime.minute();
      adjustTime[5] = currentTime.second();
      adjustState = ADJUST_HOUR;
      showUpdateTime(true);
      showUpdateTime(false);
      break;
  
    case ADJUST_HOUR:
      adjustState = ADJUST_MINUTE;
      showUpdateTime(true);
      showUpdateTime(false);
      break;
  
    case ADJUST_MINUTE:
      adjustState = ADJUST_SECOND;
      showUpdateTime(true);
      showUpdateTime(false);
      break;
  
    case ADJUST_SECOND:
      adjustState = ADJUST_MONTH;
      showUpdateTime(true);
      showUpdateTime(false);
      break;
  
    case ADJUST_MONTH:
      adjustState = ADJUST_DAY;
      showUpdateTime(true);
      showUpdateTime(false);
      break;
  
    case ADJUST_DAY:
      adjustState = ADJUST_YEAR;
      showUpdateTime(true);
      showUpdateTime(false);
      break;
  
    case ADJUST_YEAR:
      adjustState = ADJUST_IDLE;
      Clock.adjust(DateTime(
        adjustTime[0],
        adjustTime[1],
        adjustTime[2],
        adjustTime[3],
        adjustTime[4],
        adjustTime[5]));
      showUpdateTime(true);
      break;
  }
}
