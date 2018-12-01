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

#include <Wire.h>

#define POLL_INTERVAL_MS 10*1000

RTC_DATA_ATTR unsigned int wake_count = 0;

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT); 
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("GOING TO SLEEP ...");

  digitalWrite(LED_BUILTIN, LOW);
  delay(10000);
  digitalWrite(LED_BUILTIN, HIGH);

  esp_sleep_enable_timer_wakeup((POLL_INTERVAL_MS / 1000) * 1000000); // wake up after interval minus time wasted here

  esp_light_sleep_start();
}
