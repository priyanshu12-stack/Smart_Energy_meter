/*
 * =========================================================================
 * Project: IoT Smart Energy Meter
 * Version: 1.3.0 (Commercial Release - EEPROM Flash Wear Protection)
 * Description: Real-time AC voltage, current, power, and energy monitoring.
 *              Features interactive LCD display, non-volatile EEPROM storage,
 *              Blynk IoT cloud integration, and Telegram alert reporting.
 *              Includes hardware button for local calibration and memory reset.
 *              
 *              *MEMORY PROTECTION ENABLED: EEPROM only writes when energy 
 *               accumulates past a defined threshold to prevent flash burnout.
 * 
 * Copyright (c) 2026 Circuit Diagrams. All Rights Reserved.
 * =========================================================================
 */

// ======================= USER CONFIGURATION =======================

// 1. Blynk IoT Credentials
#define BLYNK_TEMPLATE_ID "YOUR_BLYNK_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "Net Energy Meter"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"

// 2. WiFi Credentials
char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";

// 3. Telegram Bot Credentials (Leave blank if not using)
const String telegramBotToken = "YOUR_BOT_TOKEN_HERE"; 
const String telegramChatID = "YOUR_CHAT_ID_HERE";     
#define TELEGRAM_INTERVAL 600000 

// 4. Energy, Pricing & Memory Settings
#define COST_PER_KWH 8.0  
#define REFERENCE_VOLTAGE 235.1
#define EEPROM_SAVE_THRESHOLD 0.01 // Save memory only every 0.01 kWh to prevent flash wear

// 5. Hardware Pin Assignments (ESP32)
#define ACS_PIN 34           
#define ZMPT_PIN 35          
#define RESET_BUTTON_PIN 4   

// ==================================================================

#include <WiFi.h>
#include "time.h"
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "ACS712.h"
#include <ZMPT101B.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

BLYNK_CONNECTED() {
  Blynk.syncVirtual(V0);
  Blynk.syncVirtual(V1);
  Blynk.syncVirtual(V2);
  Blynk.syncVirtual(V3);
  Blynk.syncVirtual(V4);
}

ACS712 ACS(ACS_PIN, 3.3, 4095, 125);
ZMPT101B voltageSensor(ZMPT_PIN, 50.0);
LiquidCrystal_I2C lcd(0x27, 16, 2);  

float energy_kWh; 
float volt, power;
float current_A;  
float total_cost; 
float vcal = 1.0; 
float smoothed_voltage = 0.0; 

// Tracking variable for EEPROM wear protection
float last_saved_energy = 0.0; 

#define EEPROM_SIZE 512
#define ENERGY_ADDRESS 0  
#define VCAL_ADDRESS 4 

unsigned long lastTime = 0;
unsigned long lastLcdChange = 0;
unsigned long lastTelegramTime = 0;
int displayPage = 1; 

void sendTelegramReport() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + telegramBotToken + "/sendMessage";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(256);
  doc["chat_id"] = telegramChatID;
  
  doc["text"] =
    "📊 Energy Report\n"
    "Voltage: " + String(volt, 1) + " V\n" +
    "Current: " + String(current_A, 2) + " A\n" +
    "Power: " + String(power, 1) + " W\n" +
    "Energy: " + String(energy_kWh, 3) + " kWh\n" +
    "Cost: ₹" + String(total_cost, 2);

  String payload;
  serializeJson(doc, payload);
  http.POST(payload);
  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  EEPROM.begin(EEPROM_SIZE);

  // Load Saved Energy Data
  energy_kWh = EEPROM.readFloat(ENERGY_ADDRESS);
  if (isnan(energy_kWh)) energy_kWh = 0.0;  
  
  // Initialize the protection tracker
  last_saved_energy = energy_kWh; 

  // Load Voltage Calibration Factor
  vcal = EEPROM.readFloat(VCAL_ADDRESS);
  if (isnan(vcal) || vcal <= 0.0 || vcal > 10.0) vcal = 1.0; 

  ACS.autoMidPoint();
  voltageSensor.setSensitivity(500.0f);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Circuit Diagrams"); 
  lcd.setCursor(2, 1);
  lcd.print("Energy Meter");   
  delay(2000);                 
  lcd.clear();

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  lastTime = millis();
  lastLcdChange = millis();
  lastTelegramTime = millis(); 
}

void loop() {
  Blynk.run();

  // --- Multi-function Hardware Button Logic ---
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    unsigned long pressStartTime = millis();
    int action = 0; 
    lcd.clear(); 

    while (digitalRead(RESET_BUTTON_PIN) == LOW) {
      unsigned long heldDuration = millis() - pressStartTime;
      
      if (heldDuration >= 3000) {
        lcd.setCursor(0, 0);
        lcd.print("Release to RESET");
        action = 2;
      } else if (heldDuration >= 1000) {
        lcd.setCursor(0, 0);
        lcd.print("Release to CAL. ");
        action = 1;
      } else {
        lcd.setCursor(0, 0);
        lcd.print("Hold button...");
      }
      Blynk.run(); 
      delay(50);
    }

    if (action == 1) { 
      // ACTION: CALIBRATE VOLTAGE
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(" Calibrating... ");
      
      float raw_v = voltageSensor.getRmsVoltage();
      if (raw_v > 20.0) { 
        vcal = REFERENCE_VOLTAGE / raw_v;
        EEPROM.writeFloat(VCAL_ADDRESS, vcal);
        EEPROM.commit();
        
        lcd.setCursor(0, 1);
        lcd.print("Factor: ");
        lcd.print(vcal, 3);
      } else {
        lcd.setCursor(0, 1);
        lcd.print("Error: No Mains!");
      }
      delay(2000);

    } else if (action == 2) { 
      // ACTION: FACTORY RESET DATA
      energy_kWh = 0.0;
      total_cost = 0.0;
      
      // Force save on manual reset
      EEPROM.writeFloat(ENERGY_ADDRESS, energy_kWh);
      EEPROM.commit();
      last_saved_energy = 0.0; // Reset tracker

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("   DATA RESET   ");
      
      Blynk.virtualWrite(V3, energy_kWh);
      Blynk.virtualWrite(V4, total_cost);
      delay(1500); 
    }

    if (action > 0) {
      lastLcdChange = millis(); 
      lcd.clear();
    }
  }
  
  // 1. Current Processing
  int noise = ACS.getNoisemV();
  float average = 0;
  for (int i = 0; i < 100; i++) {
    average += ACS.mA_AC();
  }
  float mA = (average / 100.0) - noise;
  float amps = mA / 1000.0;
  (amps > 0.05) ? current_A = amps : current_A = 0.0; 

  // 2. Voltage Processing with DIGITAL FILTER
  float raw_voltage = voltageSensor.getRmsVoltage();
  float calibrated_voltage = raw_voltage * vcal; 

  if (calibrated_voltage < 50.0) {
    smoothed_voltage = 0.0;
    volt = 0.0;
  } else {
    if (smoothed_voltage < 50.0) smoothed_voltage = calibrated_voltage; 
    smoothed_voltage = (smoothed_voltage * 0.8) + (calibrated_voltage * 0.2);
    volt = smoothed_voltage;
  }

  // 3. Power Processing
  power = volt * current_A;

  // 4. Energy & Cost Processing 
  unsigned long currentTime = millis();
  float elapsedTimeHours = (currentTime - lastTime) / 3600000.0; 
  lastTime = currentTime;

  float kW = power / 1000.0;
  energy_kWh += (kW * elapsedTimeHours);
  total_cost = energy_kWh * COST_PER_KWH;

  // --- 3-Page User Interface Logic ---
  if (currentTime - lastLcdChange >= 4000) {
    displayPage++;                
    if (displayPage > 3) {        
      displayPage = 1;
    }
    lastLcdChange = currentTime;  
    lcd.clear();                  
  }

  if (displayPage == 1) {
    lcd.setCursor(0, 0);
    lcd.print("Voltage: ");
    lcd.print(volt, 1); 
    lcd.print("V  "); 
    lcd.setCursor(0, 1);
    lcd.print("Current: ");
    lcd.print(current_A, 2); 
    lcd.print("A  ");
  } 
  else if (displayPage == 2) {
    lcd.setCursor(0, 0);
    lcd.print("Power: ");
    lcd.print(power, 1); 
    lcd.print("W  "); 
    lcd.setCursor(0, 1);
    lcd.print("Energy: ");
    lcd.print(energy_kWh, 2); 
    lcd.print("kWh");
  } 
  else if (displayPage == 3) {
    lcd.setCursor(0, 0);
    lcd.print("Total Cost: ");
    lcd.setCursor(0, 1);
    lcd.print("Rs. "); 
    lcd.print(total_cost, 2);
    lcd.print("      ");
  }

  // --- Cloud & Serial Communications ---
  if (currentTime - lastTelegramTime >= TELEGRAM_INTERVAL) {
    sendTelegramReport();
    lastTelegramTime = currentTime; 
  }

  Serial.print("Voltage: "); Serial.print(volt, 1); Serial.println(" V");
  Serial.print("Current: "); Serial.print(current_A, 3); Serial.println(" A");
  Serial.print("Power: "); Serial.print(power, 1); Serial.println(" W");
  delay(500); 

  // --- EEPROM WEAR PROTECTION LOGIC ---
  // Only write to memory if the energy has increased by the defined threshold
  if (energy_kWh - last_saved_energy >= EEPROM_SAVE_THRESHOLD) {
    EEPROM.writeFloat(ENERGY_ADDRESS, energy_kWh);
    EEPROM.commit();
    last_saved_energy = energy_kWh; // Update the tracker
    Serial.println("*** EEPROM SAVED: Threshold reached ***");
  }

  Blynk.virtualWrite(V0, volt);
  Blynk.virtualWrite(V1, current_A); 
  Blynk.virtualWrite(V2, power);
  Blynk.virtualWrite(V3, energy_kWh);
  Blynk.virtualWrite(V4, total_cost); 
}