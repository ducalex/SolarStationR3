#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_pm.h>
#include <esp_bt.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <rom/rtc.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <Adafruit_ADS1015.h>

#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>;
#include <Wire.h>

const int   POLL_INTERVAL = 15; // in seconds

const int   HTTP_REQUEST_TIMEOUT_MS = 5*1000;


#define VBAT_PIN 36 // GPIO. The cal code assumes it's on ADC1
#define VBAT_OFFSET 0.0 // If there is a diode or transistor in the way
#define VBAT_MULTIPLIER 2.0 // If there is a voltage divider
#define VBAT_SAMPLE 50 // How many sample ?


const char* WIFI_SSID = "Maxou_TestIOT";
const char* WIFI_PASSWORD = "!UnitedStatesOfSmash97!";
const char* HTTP_UPDATE_URL = "http://192.168.5.214";
const int   HTTP_UPDATE_PORT = 9999;

RTC_DATA_ATTR unsigned int wake_count = 0;

#define OLED_ADDRESS 0x3C

#define DHTPIN 15     // what pin we're connected to
#define DHTTYPE DHT11   // DHT 22  (AM2302)
DHT dht(DHTPIN, DHTTYPE);
Adafruit_ADS1115 ads; 
SSD1306AsciiWire oled;

RTC_DATA_ATTR bool m_wifiMode = true;
RTC_DATA_ATTR int m_lastStartError = 0;

static float voltage_bat = 0;
static int voltage_batRAW = 0;
static int voltage_lightsensor = 0;
static float dht_TempC = 0;
static float dht_TempHum = 0;


void setup() {
  //btStart();

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector. maybe we should set it to lowest instead of disabling

  Serial.begin(115200);
  dht.begin();

  
  Wire.begin();
  oled.begin(&Adafruit128x64, OLED_ADDRESS);
  oled.setFont(System5x7);
  oled.setScrollMode(SCROLL_MODE_AUTO);
  oled.clear();

  ads.begin();
  ads.setGain(GAIN_ONE);
  
  pinMode(LED_BUILTIN, OUTPUT); 
  pinMode(A0, INPUT);
  
  digitalWrite(LED_BUILTIN, HIGH); // No light ...

  // Morse ....
  if (m_lastStartError > 0) {
    morseError(m_lastStartError);
    m_lastStartError = 0;
  }
}

void loop() {
  
  unsigned long startMS = millis();

  if (!m_wifiMode) {
    wake_count++;
    
    Serial.println("WORK MODE");
    oled.println("Potatoes industries");
    oled.println("Sampling ...");
    
    // Do some useful task ...
    while((millis() - startMS) < 120*1000) {
      //
      // 100 ms timeslot for usefull task
      // followed by 900 ms until Quokka kiss him.
      Serial.println(" ... ...");
      Serial.flush();

      readLightSensor();
      readBattery();
      oled.print("Light: ");
      oled.println(voltage_lightsensor);
      oled.print(", batt: ");
      oled.print(voltage_bat);
      oled.println("v");
      
      // Sleep for 900 ms
      esp_sleep_enable_timer_wakeup(900 * 1000); // wake up after interval minus time wasted here  
      esp_light_sleep_start();
      wake_count++;
    }

    oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
    
    // Ask wifi to operate on next boot ...
    m_wifiMode = true;
    esp_sleep_enable_timer_wakeup(250); // Basically we just want to reset ...
    esp_deep_sleep_start(); // Good night
  }
  else {
    wake_count++;
    
    Serial.println("WIFI MODE");

    oled.println("Potatoes industries");
  
    m_lastStartError = 1;
    
    digitalWrite(LED_BUILTIN, LOW);
  
    // put your setup code here, to run once:
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setSleep(true);
    
    m_lastStartError = 2;
    
    while(WiFi.status() != WL_CONNECTED && millis() < 10000) {
      delay(500);
      Serial.print(".");
    }
    
    m_lastStartError = 3;
   
    if (WiFi.status() == WL_CONNECTED) {
      readBattery();
      m_lastStartError = 4;
      readTemp();
      m_lastStartError = 5;
      readLightSensor();
        
      Serial.printf("Battery: %.2f V\n", voltage_bat);
      Serial.print("Time: ");
      Serial.print(millis() - startMS);
      Serial.println(" ms");
      
      Serial.print("light sensor: ");
      Serial.println(voltage_lightsensor);
      Serial.print("Humidity: ");
      Serial.print(dht_TempHum);
      Serial.print(" %, Temp: ");
      Serial.println(dht_TempC);
      m_lastStartError = 6;

      httpRequest();
      
      m_lastStartError = 7;

      Serial.println("GOING TO SLEEP ...");
      Serial.flush();
      
      Serial.println("Time to sleep now, I go gently into that good night");
    }
    else {
      Serial.println("Time to sleep now, can't connect to wifi :(");
    }

    m_lastStartError = 8;

    digitalWrite(LED_BUILTIN, HIGH);
    oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
    
    //esp_sleep_enable_timer_wakeup((POLL_INTERVAL - (millis() - startMS) / 1000) * 1000000); // wake up after interval minus time wasted here
    m_wifiMode = false;    
    
    m_lastStartError = 0; // no error
    esp_sleep_enable_timer_wakeup(250); // Basically we just want to reset ...
    esp_deep_sleep_start(); // Good night
    //esp_light_sleep_start();
  }
}

void readBattery() {
  voltage_batRAW = ads.readADC_SingleEnded(3);
  voltage_bat = voltage_batRAW * 0.125d * VBAT_MULTIPLIER * 0.001;
  /*
  static esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars); // We could calibrate vref...

  float value = 0.0;
  int samples = 50;
  
  for(int i = 0; i < samples; i++) {
    value += (float)analogRead(VBAT_PIN) / samples; // this seems to be a bit closer to reality
    delay(1);
  }
  
  voltage_bat = (float)esp_adc_cal_raw_to_voltage((int)value, &adc_chars) / 1000 * VBAT_MULTIPLIER + VBAT_OFFSET;*/
}

void readLightSensor() {
  voltage_lightsensor = ads.readADC_SingleEnded(1) ;
}

void readTemp() {
  dht_TempC = (float)dht.readTemperature();
  dht_TempHum = (float)dht.readHumidity();
}

void httpRequest() {
  Serial.print("HTTP: POST...");
  
  HTTPClient http;
  char payload[512];
  
  http.begin(HTTP_UPDATE_URL);
  
  http.setTimeout(HTTP_REQUEST_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  sprintf(payload, "s=%s&v=%f&wc=%d&temp=%f&humidity=%f&light=%d&battraw=%d", 
      "TEST2", voltage_bat, wake_count,dht_TempC, dht_TempHum, voltage_lightsensor, voltage_batRAW);
  
  int httpCode = http.POST(payload);
  
  if (httpCode > 0) {
    //String payload = http.getString();
    Serial.printf("%d\n", httpCode);
  } else {
    Serial.printf("error: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
}

void morseError(int morse) {
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("morse");
  Serial.println(morse);
  switch(morse) {
    case 0: // - - - - -
       morse_dash(); morse_dash(); morse_dash(); morse_dash(); morse_dash();
       break;
    case 1: // . - - - -
       morse_dot(); morse_dash(); morse_dash(); morse_dash(); morse_dash();
       break;
    case 2: // . . - - -
       morse_dot(); morse_dot(); morse_dash(); morse_dash(); morse_dash();
       break;
    case 3: // . . . - -
       morse_dot(); morse_dot(); morse_dot(); morse_dash(); morse_dash();
       break;
    case 4: // . . . . -
       morse_dot(); morse_dot(); morse_dot(); morse_dot(); morse_dash();
       break;
    case 5: // . . . . .
       morse_dot(); morse_dot(); morse_dot(); morse_dot(); morse_dot();
       break;
    case 6: // - - - - .
       morse_dash(); morse_dash(); morse_dash(); morse_dash(); morse_dot();
       break;
    case 7: // - - - . .
       morse_dash(); morse_dash(); morse_dash(); morse_dot(); morse_dot();
       break;
    case 8: // - - . . .
       morse_dash(); morse_dash(); morse_dot(); morse_dot(); morse_dot();
       break;
    case 9: // - . . . .
       morse_dash(); morse_dot(); morse_dot(); morse_dot(); morse_dot();
       break;
  }

  delay(3000);
}

void morse_dash() {
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
}

void morse_dot() {
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
}
