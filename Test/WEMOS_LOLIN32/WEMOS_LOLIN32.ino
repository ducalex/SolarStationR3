#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_pm.h>
#include <esp_bt.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <rom/rtc.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>;
#include <Wire.h>

const int   POLL_INTERVAL = 15; // in seconds

const int   HTTP_REQUEST_TIMEOUT_MS = 5*1000;


#define VBAT_PIN 36 // GPIO. The cal code assumes it's on ADC1
#define VBAT_OFFSET 0.0 // If there is a diode or transistor in the way
#define VBAT_MULTIPLIER 2 // If there is a voltage divider
#define VBAT_SAMPLE 50 // How many sample ?


const char* WIFI_SSID = "Maxou_TestIOT";
const char* WIFI_PASSWORD = "!UnitedStatesOfSmash97!";
const char* HTTP_UPDATE_URL = "http://192.168.5.214";
const int   HTTP_UPDATE_PORT = 9999;

RTC_DATA_ATTR unsigned int wake_count = 0;

#define DHTPIN 15     // what pin we're connected to
#define DHTTYPE DHT11   // DHT 22  (AM2302)
DHT dht(DHTPIN, DHTTYPE);

RTC_DATA_ATTR bool m_wifiMode = true;

static float voltage_bat = 0;
static float dht_TempC = 0;
static float dht_TempHum = 0;

void setup() {
  //btStart();

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector. maybe we should set it to lowest instead of disabling

  Serial.begin(115200);
  dht.begin();
  
  pinMode(LED_BUILTIN, OUTPUT); 
  pinMode(A0, INPUT);
  
  digitalWrite(LED_BUILTIN, HIGH); // No light ...
}

void loop() {
  
  unsigned long startMS = millis();

  if (!m_wifiMode) {
    wake_count++;
    
    Serial.println("WORK MODE");
    
    // Do some useful task ...
    while(WiFi.status() != WL_CONNECTED && (millis() - startMS) < 120*1000) {
      //
      // 100 ms timeslot for usefull task
      // followed by 900 ms until Quokka kiss him.
      Serial.println(" ... ...");
      Serial.flush();
      delay(100);

      // Sleep for 900 ms
      esp_sleep_enable_timer_wakeup(900 * 1000); // wake up after interval minus time wasted here  
      esp_light_sleep_start();
      wake_count++;
    }

    // Ask wifi to operate on next boot ...
    m_wifiMode = true;
    esp_sleep_enable_timer_wakeup(250); // Basically we just want to reset ...
    esp_deep_sleep_start(); // Good night
  }
  else {
    wake_count++;
    
    Serial.println("WIFI MODE");
    
    digitalWrite(LED_BUILTIN, LOW);
  
    // put your setup code here, to run once:
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setSleep(true);
    while(WiFi.status() != WL_CONNECTED && millis() < 10000) {
      delay(500);
      Serial.print(".");
    }
   
    if (WiFi.status() == WL_CONNECTED) {
      readBattery();
      readTemp();
        
      Serial.printf("Battery: %.2f V\n", voltage_bat);
      Serial.print("Time: ");
      Serial.print(millis() - startMS);
      Serial.println(" ms");
      
      Serial.print("Humidity: ");
      Serial.print(dht_TempHum);
      Serial.print(" %, Temp: ");
      Serial.println(dht_TempC);
    
      httpRequest();

      Serial.println("GOING TO SLEEP ...");
      Serial.flush();
      
      Serial.println("Time to sleep now, I go gently into that good night");
    }
    else {
      Serial.println("Time to sleep now, can't connect to wifi :(");
    }
    
    digitalWrite(LED_BUILTIN, HIGH);
    
    //esp_sleep_enable_timer_wakeup((POLL_INTERVAL - (millis() - startMS) / 1000) * 1000000); // wake up after interval minus time wasted here
    m_wifiMode = false;    
    
    esp_sleep_enable_timer_wakeup(250); // Basically we just want to reset ...
    esp_deep_sleep_start(); // Good night
    //esp_light_sleep_start();
  }
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

  sprintf(payload, "s=%s&v=%f&wc=%d&temp=%f&humidity=%f", 
      "TEST2", voltage_bat, wake_count,dht_TempC, dht_TempHum);
  
  int httpCode = http.POST(payload);
  
  if (httpCode > 0) {
    //String payload = http.getString();
    Serial.printf("%d\n", httpCode);
  } else {
    Serial.printf("error: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
}
