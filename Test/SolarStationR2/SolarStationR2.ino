#include "Config.h"
#include "AVGr.h"

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

#if OLED_SCREENACTIVE > 0
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>
#endif

#include <Adafruit_BMP085.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>;
#include <Wire.h>


// Should survive a reboot requence ...
RTC_DATA_ATTR unsigned int wake_count = 0;
RTC_DATA_ATTR bool m_firstStart = true;
RTC_DATA_ATTR bool m_powerSaveMode = false;

// Devices
DHT dht(DHTPIN, DHTTYPE);
Adafruit_ADS1115 ads; 
#if OLED_SCREENACTIVE > 0
SSD1306AsciiWire oled;
#endif

#if BMPSENSORACTIVE > 0
Adafruit_BMP085 bmp;
#endif

// Global sensor values
RTC_DATA_ATTR AVGr m_batteryVolt(MEASUREMENTAVGCOUNT);
RTC_DATA_ATTR AVGr m_lightsensorRAW(MEASUREMENTAVGCOUNT);


RTC_DATA_ATTR AVGr m_exteriorTemp(12);
RTC_DATA_ATTR AVGr m_exteriorPressure(12);
RTC_DATA_ATTR AVGr m_interiorTempC(12);
RTC_DATA_ATTR AVGr m_interiorHumidityPERC(12);


void setup() {
  Serial.println("Starting ...");

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector. maybe we should set it to lowest instead of disabling

  Serial.begin(115200);
  dht.begin();

  Serial.println("Device init...");
  Wire.begin();
  #if OLED_SCREENACTIVE > 0
  oled.begin(&Adafruit128x64, SCREEN_OLED_I2CADDRESS);
  #endif
  ads.begin();
  ads.setGain(VADC_SENSORGAIN);

  #if BMPSENSORACTIVE > 0
  bmp.begin();   
  #endif
  
  pinMode(LED_BUILTIN, OUTPUT); 
  pinMode(A0, INPUT);
  
  digitalWrite(LED_BUILTIN, HIGH); // No light ...
}

void loop() {
  unsigned long startMS = millis();
  unsigned long lastOLEDUpdateMS = 0;
  unsigned long lastBMPUpdateMS = 0;
  wake_count++;

  bool isDebug = (wake_count < 5);

  #if OLED_SCREENACTIVE > 0
  Serial.println("OLED init...");
  oled.setFont(System5x7);
  oled.setScrollMode(SCROLL_MODE_AUTO);
  oled.clear();
  #endif
  
  // Do some useful task ...
  int measureCount = 
    (m_powerSaveMode ? 300 : (isDebug ? 30 : 120));
  
  if (m_firstStart)
    measureCount = 1; // We want to send a message immediately ...

  m_firstStart = false;

  Serial.println("Read BMP Data...");
  readBMPData();
  Serial.println("Read BMP Data...");
  readTemp();
  
  while(measureCount--) {
    //
    // 100 ms timeslot for usefull task
    // followed by 900 ms until Quokka kiss him. 
    unsigned long timeuS =  micros();
    
    readBattery();
    readLightSensor();

    /* Slow to measure ... */
    if (lastBMPUpdateMS == 0 || (millis() - lastBMPUpdateMS) > 10000) {
      //readBMPData();
      readTemp();
      
      lastBMPUpdateMS = millis();
    }
    
      // Display on screen ...
    if ((lastOLEDUpdateMS == 0 || (millis() - lastOLEDUpdateMS) > 5000) && isDebug ) {    //
      #if OLED_SCREENACTIVE > 0
      oled.setCursor(0, 0);
      oled.println("Potatoes industries");
      oled.println("values:");
      char buffer[150];
      sprintf(buffer, 
        "battery: %.2f v\ntemp: %.2f C / %.2f C\nhumidity: %.2f %%\nlight: %.2f /bits\nwakeup: %d",
        m_batteryVolt.getAvg(),
        m_interiorTempC.getAvg(),
        m_exteriorTemp.getAvg(),
        m_interiorHumidityPERC.getAvg(),
        m_lightsensorRAW.getAvg(),
        wake_count);
      oled.print(buffer);
      
      delay(10); // This delays seems to resolve a lot of problem related to OLED and ADC communication.
      #endif
      
      lastOLEDUpdateMS = millis();
    }

    Serial.println("Loop time: ");
    Serial.println((micros() - timeuS));
    Serial.flush();
        
    // Sleep as much as possible ...
    // 
    long diff = (micros() - timeuS);
    if (diff >= 0) {
      esp_sleep_enable_timer_wakeup(1000*1000 - diff); // wake up after interval minus time wasted here  
      esp_light_sleep_start();
    }
  }

  if (!m_powerSaveMode) {
    digitalWrite(LED_BUILTIN, LOW); // No light ...
  }
  
  // Next loop in power saving mode ?
  if ((!m_powerSaveMode && m_batteryVolt.getAvg() < POWERMNG_EMERGENCY_POWER_VOLT_MIN) || (wake_count == 1 && m_batteryVolt.getAvg() < POWERMNG_EMERGENCY_POWER_VOLT_BOOT)) 
    m_powerSaveMode = true;
  else if ((m_powerSaveMode && m_batteryVolt.getAvg() > POWERMNG_EMERGENCY_POWER_VOLT_MAX )) 
    m_powerSaveMode = false;

  #if OLED_SCREENACTIVE > 0
  oled.clear();
  oled.println("Potatoes industries");
  oled.println("Sending data WIFI ...");
  #endif

  // WIFI PART ....
  // put your setup code here, to run once:
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(true);

  long startConnMS = millis();
  
  while(WiFi.status() != WL_CONNECTED && (millis()-startConnMS) < WIFI_CONNECTIONTIMEOUTMS) {
    delay(500);
    #if OLED_SCREENACTIVE > 0
    oled.print(".");
    #endif
  }

  #if OLED_SCREENACTIVE > 0
  oled.print("WIFI Connected");
  #endif
    
  if (WiFi.status() == WL_CONNECTED) {
    httpRequest();  

    #if OLED_SCREENACTIVE > 0
    oled.println("GOING TO SLEEP ...");
    oled.println("Time to sleep now");
    oled.println("I go gently into that good night");
    #endif
  }
  else {
    #if OLED_SCREENACTIVE > 0
    oled.println("Time to sleep now, can't connect to wifi :(");
    #endif
  }

  #if OLED_SCREENACTIVE > 0
  oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
  #endif
  digitalWrite(LED_BUILTIN, HIGH); // No light ...

  // Ask wifi to operate on next boot ...
  esp_sleep_enable_timer_wakeup(250); // Basically we just want to reset wihtout destroying RTC RAM content...
  esp_deep_sleep_start(); // Good night
}

void readBMPData() {
    #if BMPSENSORACTIVE > 0
    m_exteriorTemp.add(bmp.readTemperature() + BMPTEMPC_OFFSET);
    m_exteriorPressure.add((float)bmp.readPressure());    
    #else
    m_exteriorTemp.add(0);
    m_exteriorPressure.add(0);    
    #endif
    delay(10); // This delays seems to resolve a lot of problem related to OLED and ADC communication.
}

void readBattery() { 
  int batteryVoltRAW = ads.readADC_SingleEnded(VADC_INPUT_BATTERY);
  float finalVoltage = (float)batteryVoltRAW * VADC_PERBIT * VBAT_MULTIPLIER;

  Serial.print("volt: ");
  Serial.println(finalVoltage);
  m_batteryVolt.add( finalVoltage );
}

void readLightSensor() {
  int lightsensorRAW = ads.readADC_SingleEnded(VADC_INPUT_LIGHTSENSOR);
  
  m_lightsensorRAW.add( lightsensorRAW );
}

void readTemp() {
  m_interiorTempC.add( (float)dht.readTemperature() );
  m_interiorHumidityPERC.add( (float)dht.readHumidity() );
}


void httpRequest() {
  #if OLED_SCREENACTIVE > 0
  oled.print("HTTP: POST...");
  #endif
  
  HTTPClient http;
  char payload[512];
  
  http.begin(HTTP_UPDATE_URL);
  
  http.setTimeout(HTTP_REQUEST_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  sprintf(payload, "s=%s&battv=%f&wc=%d&boxtemp=%.2f&boxhumidity=%.2f&light=%.2f&powersave=%d&exttempc=%.2f&pressurepa=%.2f&m=coucoualex", 
      "TEST2", m_batteryVolt.getAvg(), wake_count,m_interiorTempC.getAvg(), m_interiorHumidityPERC.getAvg(), m_lightsensorRAW.getAvg(), m_powerSaveMode ? 1 : 0, m_exteriorTemp.getAvg(), m_exteriorPressure.getAvg() );
  
  int httpCode = http.POST(payload);

  #if OLED_SCREENACTIVE > 0
  if (httpCode > 0) {
    //String payload = http.getString();
    oled.printf("%d\n", httpCode);
  } else {
    oled.printf("error: %s\n", http.errorToString(httpCode).c_str());
  }
  #endif
  
  http.end();
}

