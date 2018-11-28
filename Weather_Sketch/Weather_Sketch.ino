#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_pm.h>
#include <esp_wifi.h>
#include <rom/rtc.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include <WiFi.h>
#include <HTTPClient.h>

#include <Wire.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>
#include <SimpleDHT.h>
#include <Adafruit_BMP085.h>

#define VBAT_PIN 36 // GPIO. The cal code assumes it's on ADC1
#define VBAT_OFFSET 0.0 // If there is a diode or transistor in the way
#define VBAT_MULTIPLIER 2 // If there is a voltage divider
#define OLED_ADDRESS 0x3C // Usually 0x3C or 0x3D
#define DHT_PIN 19 // comment to use DHT11 instead
#define DHT_TYPE SimpleDHT22 // or SimpleDHT11

#define STATION_NAME "PATATE"

// #define WITHOLED

const char* WIFI_SSID = "Maxou_TestIOT";
const char* WIFI_PASSWORD = "!UnitedStatesOfSmash97!";
const char* HTTP_UPDATE_URL = "http://";
const char* HTTP_USERNAME = "";
const char* HTTP_PASSWORD = "";
const char* STATION_ID = "";
const char* NORTH_OFFSET = "";
const int   POLL_INTERVAL = 30; // in seconds

static float temperature = 0;
static float humidity = 0;
static float temperature2 = 0;
static float pressure = 0;
static float altitude = 0;
static float voltage_bat = 0;

RTC_DATA_ATTR unsigned int wake_count = 0;

#if WITHOLED
static SSD1306AsciiWire oled;
#else
#define oled Serial
#endif


void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector. maybe we should set it to lowest instead of disabling
  wake_count++;

  Serial.begin(115200);
  
  if (rtc_get_reset_reason(0) != DEEPSLEEP_RESET) {
    Serial.println("Weather Station version ");
    Serial.println("Type config to enter configuration mode");
  }

  #if WITHOLED
  Wire.begin();
  oled.begin(&Adafruit128x64, OLED_ADDRESS);
  oled.setFont(System5x7);
  oled.setScrollMode(SCROLL_MODE_AUTO);
  oled.clear();
  oled.print("Connecting...");
  #endif
  
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(true);
  
  readSensors(); // Do that while it's connecting
  readBattery(); // maybe we should do that before turning on wifi? voltage drops up to .2V when wifi is active
  
  while(WiFi.status() != WL_CONNECTED && millis() < 10000) {
    delay(500);
    oled.print(".");
  }
  
  refreshDisplay();
  
  if (WiFi.status() == WL_CONNECTED) {
    httpRequest();
  }
  
  delay(3000); // keep display on for a moment
  
  #if WITHOLED
  oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF); // It doesn't clear the ram but we do it on bootup
  #endif
  WiFi.disconnect(true, true); // turn off wifi, wipe wifi credentials
  //esp_wifi_stop();
  esp_sleep_enable_timer_wakeup((POLL_INTERVAL - millis() / 1000) * 1000000); // wake up after interval minus time wasted here
  esp_deep_sleep_start(); // Good night
}


void readSensors() {
  // Power on and init sensors
  Adafruit_BMP085 bmp;
  DHT_TYPE dht(DHT_PIN);
  
  bmp.begin();
  
  temperature = bmp.readTemperature();
  pressure = (float)bmp.readPressure() / 1000;
  altitude = bmp.readAltitude(); // not very accurate without sealevel pressure
  
  if (dht.read2(&temperature2, &humidity, NULL) != SimpleDHTErrSuccess) {
    delay(2000);
    dht.read2(&temperature2, &humidity, NULL); // try again
  }
  
  // Here we should put sensors to sleep if possible
}


void readBattery() {
  static esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars); // We could calibrate vref...

  float value = 0.0;
  int samples = 50;
  
  for(int i = 0; i < samples; i++) {
    value += (float)analogRead(VBAT_PIN) / samples; // this seems to be a bit closer to reality
    delay(1);
  }
  
  voltage_bat = (float)esp_adc_cal_raw_to_voltage((int)value, &adc_chars) / 1000 * VBAT_MULTIPLIER + VBAT_OFFSET;
}


void refreshDisplay() {
  
  #if WITHOLED
  oled.clear();
  #endif
  
  if (WiFi.status() == WL_CONNECTED) {
    oled.println("Wifi: connected");
  } else {
    oled.println("Wifi: NOT connected");
  }

  // arduino printf's implementation doesn't support floating points but it works for us... YAY ESP32?
  oled.printf("Battery: %.2f V\n", voltage_bat);
  oled.printf("Temperature: %.2f C\n", temperature2);
  oled.printf("Pressure: %.2f kPa\n", pressure);
  oled.printf("Altitude: %.2f m\n", altitude);
  //oled.println();
  
  if (humidity != 0.0 || temperature != 0.0) {
    oled.printf("DHT: %.2f C, %.2f%%\n", temperature, humidity);
  }
}


void httpRequest() {
  oled.print("HTTP: GET...");
  
  HTTPClient http;
  char payload[512];
  
  http.begin(HTTP_UPDATE_URL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  sprintf(payload, "s=%st=%f&t2=%f&h=%f&p=%f&v=%f&wc=%d", 
			STATION_NAME, temperature, temperature2, humidity, pressure, voltage_bat, wake_count);
  
  int httpCode = http.POST(payload);
  
  if (httpCode > 0) {
    //String payload = http.getString();
    oled.printf("%d\n", httpCode);
  } else {
    oled.printf("error: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
}


void loop() {

}
