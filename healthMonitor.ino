#include <Wire.h>
#include <LiquidCrystal.h>
#include <WiFi.h>

// IMPORTANT: Blynk template must be defined BEFORE including BlynkSimpleEsp32.h
#define BLYNK_TEMPLATE_ID "TMPL62wFbO2QV"
#define BLYNK_TEMPLATE_NAME "Health Monitor"
#define BLYNK_AUTH_TOKEN "h-LuJy0eMjxSaNl8kBRRzwYvqRFil9ob"

#include <BlynkSimpleEsp32.h>
#include "MAX30105.h"
#include "heartRate.h"

// WiFi credentials
char ssid[] = "dlink_DWR-930M_4E49";
char pass[] = "W33f774df4";

// Initialize LCD (RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(19, 18, 5, 17, 16, 4);

// Initialize MAX30102
MAX30105 particleSensor;

// Pin definitions
const int LED_PIN = 15;
const int BUZZER_PIN = 12;

// Buzzer timing
unsigned long buzzerStartTime = 0;
int buzzerDuration = 0;
bool buzzerActive = false;
unsigned long lastAlertTime = 0;
const long ALERT_INTERVAL = 3000; // Beep every 3 seconds for alerts

// Heart rate variables - IMPROVED
const byte RATE_SIZE = 10;  // Increased from 4 to 10 for better averaging
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;
int validBeatsCount = 0;  // Track how many valid beats we have

// SpO2 variables - IMPROVED
double avered = 0;
double aveir = 0;
int i = 0;
int Num = 30;  // Reduced from 100 to 30 for faster updates
double SpO2 = 0;
double lastValidSpO2 = 0;  // Store last valid reading

#define FINGER_ON 7000
#define MINIMUM_SPO2 90.0

// Blood pressure estimation variables
int systolicBP = 0;
int diastolicBP = 0;
long lastBPUpdate = 0;
const long BP_UPDATE_INTERVAL = 5000;

// Display update timing
unsigned long lastDisplayUpdate = 0;
const long DISPLAY_UPDATE_INTERVAL = 500;  // Update display every 500ms

// Virtual Pins for Blynk
#define VPIN_HEARTRATE V0
#define VPIN_SPO2 V1
#define VPIN_SYSTOLIC V2
#define VPIN_DIASTOLIC V3
#define VPIN_STATUS V4

BlynkTimer timer;

// Function to estimate blood pressure (simplified algorithm)
void estimateBloodPressure() {
  if (beatAvg > 0 && beatAvg < 200) {
    int baseSystolic = 110;
    int baseDiastolic = 70;
    
    int hrDeviation = beatAvg - 70;
    
    systolicBP = baseSystolic + (hrDeviation * 0.3);
    diastolicBP = baseDiastolic + (hrDeviation * 0.2);
    
    systolicBP += random(-5, 5);
    diastolicBP += random(-3, 3);
    
    systolicBP = constrain(systolicBP, 80, 180);
    diastolicBP = constrain(diastolicBP, 50, 100);
    
    if (systolicBP <= diastolicBP) {
      systolicBP = diastolicBP + 20;
    }
  }
}

void sendToBlynk() {
  // Only send to Blynk if connected
  if (!Blynk.connected()) {
    return;
  }
  
  if (beatAvg > 0 && validBeatsCount >= 3) {  // Only send after getting some valid beats
    Blynk.virtualWrite(VPIN_HEARTRATE, beatAvg);
    Blynk.virtualWrite(VPIN_SPO2, lastValidSpO2);
    Blynk.virtualWrite(VPIN_SYSTOLIC, systolicBP);
    Blynk.virtualWrite(VPIN_DIASTOLIC, diastolicBP);
    
    String status = "Normal";
    if (lastValidSpO2 < MINIMUM_SPO2) {
      status = "LOW SpO2!";
    } else if (beatAvg < 40 || beatAvg > 120) {
      status = "Abnormal HR!";
    } else if (systolicBP > 140 || diastolicBP > 90) {
      status = "High BP (Est)";
    }
    Blynk.virtualWrite(VPIN_STATUS, status);
  } else {
    Blynk.virtualWrite(VPIN_STATUS, "Reading...");
  }
}

void activateBuzzer(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerStartTime = millis();
  buzzerDuration = duration;
  buzzerActive = true;
}

void handleBuzzer() {
  if (buzzerActive && (millis() - buzzerStartTime >= buzzerDuration)) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }
}

void updateDisplay() {
  // Clear and display line by line
  lcd.setCursor(0, 0);
  lcd.print("Health Monitor      ");
  
  // Row 2 - Heart Rate
  lcd.setCursor(0, 1);
  if (beatAvg > 0 && validBeatsCount >= 2) {
    char hrBuffer[21];
    sprintf(hrBuffer, "HR:%-3d bpm         ", beatAvg);
    lcd.print(hrBuffer);
  } else {
    lcd.print("HR:Reading...       ");
  }
  
  // Row 3 - SpO2 and BP
  lcd.setCursor(0, 2);
  if (lastValidSpO2 > 0) {
    char line3Buffer[21];
    if (systolicBP > 0) {
      sprintf(line3Buffer, "SpO2:%-2d%% BP:%-3d/%-3d", (int)lastValidSpO2, systolicBP, diastolicBP);
    } else {
      sprintf(line3Buffer, "SpO2:%-2d%%           ", (int)lastValidSpO2);
    }
    lcd.print(line3Buffer);
  } else {
    lcd.print("SpO2:---%           ");
  }
  
  // Row 4 - Status
  lcd.setCursor(0, 3);
  if (beatAvg > 0 && validBeatsCount >= 3) {
    bool alertCondition = false;
    
    if (lastValidSpO2 < MINIMUM_SPO2 && lastValidSpO2 > 0) {
      lcd.print("LOW SpO2 WARNING!   ");
      alertCondition = true;
    } else if (beatAvg < 40 || beatAvg > 120) {
      lcd.print("Abnormal HR!        ");
      alertCondition = true;
    } else if (systolicBP > 140 || diastolicBP > 90) {
      lcd.print("High BP (Estimated) ");
    } else {
      lcd.print("Status: Normal      ");
    }
    
    // Trigger buzzer for alerts
    if (alertCondition && (millis() - lastAlertTime > ALERT_INTERVAL)) {
      activateBuzzer(300);
      lastAlertTime = millis();
      Serial.println("ALERT! Buzzer activated");
    }
  } else {
    lcd.print("Measuring...        ");
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  lcd.begin(20, 4);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Health Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi...");
  
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  lcd.setCursor(0, 1);
  lcd.print("WiFi Connected!     ");
  lcd.setCursor(0, 2);
  lcd.print("Initializing...     ");
  
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MAX30102 Error!");
    lcd.setCursor(0, 1);
    lcd.print("Check Wiring");
    while (1);
  }
  
  // IMPROVED: Better sensor configuration for faster, more accurate readings
  particleSensor.setup(); 
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);
  
  // Set sample rate to 400 samples per second for faster response
  particleSensor.setSampleRate(400);
  
  // Setup timer
  timer.setInterval(2000L, sendToBlynk);
  
  // Test buzzer on startup
  Serial.println("Testing buzzer...");
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);
  delay(200);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("Buzzer test complete");
  
  delay(1000);
  lcd.clear();
}

void loop() {
  Blynk.run();
  timer.run();
  handleBuzzer();
  
  long irValue = particleSensor.getIR();
  long redValue = particleSensor.getRed();
  
  // Check if finger is detected
  if (irValue < FINGER_ON) {
    lcd.setCursor(0, 0);
    lcd.print("Place finger on     ");
    lcd.setCursor(0, 1);
    lcd.print("sensor...           ");
    lcd.setCursor(0, 2);
    lcd.print("                    ");
    lcd.setCursor(0, 3);
    lcd.print("                    ");
    
    digitalWrite(LED_PIN, LOW);
    beatAvg = 0;
    validBeatsCount = 0;
    systolicBP = 0;
    diastolicBP = 0;
    
    // Reset arrays
    for (byte x = 0; x < RATE_SIZE; x++) {
      rates[x] = 0;
    }
    rateSpot = 0;
    
    return;
  }
  
  digitalWrite(LED_PIN, HIGH);
  
  // Heart rate detection - IMPROVED
  if (checkForBeat(irValue) == true) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    
    beatsPerMinute = 60 / (delta / 1000.0);
    
    // More lenient range and better filtering
    if (beatsPerMinute < 200 && beatsPerMinute > 30) {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;
      
      if (validBeatsCount < RATE_SIZE) {
        validBeatsCount++;
      }
      
      // Calculate average
      int sum = 0;
      int count = (validBeatsCount < RATE_SIZE) ? validBeatsCount : RATE_SIZE;
      for (byte x = 0; x < count; x++) {
        sum += rates[x];
      }
      beatAvg = sum / count;
      
      // BEEP ON EVERY HEARTBEAT!
      digitalWrite(BUZZER_PIN, HIGH);
      delay(50); // Short 50ms beep
      digitalWrite(BUZZER_PIN, LOW);
      
      // Print immediate feedback
      Serial.print("Beat detected! BPM: ");
      Serial.print(beatsPerMinute);
      Serial.print(" | Avg: ");
      Serial.println(beatAvg);
    }
  }
  
  // SpO2 calculation - IMPROVED
  double fred = (double)redValue;
  double fir = (double)irValue;
  
  avered += fred / Num;
  aveir += fir / Num;
  i++;
  
  if (i == Num) {
    double R = (avered / aveir);
    SpO2 = -23.3 * (R - 0.4) + 100;
    
    if (SpO2 > 100) SpO2 = 100;
    if (SpO2 < 0) SpO2 = 0;
    
    // Only update if reading seems valid
    if (SpO2 > 70 && SpO2 <= 100) {
      lastValidSpO2 = SpO2;
    }
    
    // Estimate blood pressure
    if (millis() - lastBPUpdate > BP_UPDATE_INTERVAL && validBeatsCount >= 3) {
      estimateBloodPressure();
      lastBPUpdate = millis();
    }
    
    // Print to Serial
    Serial.print("HR: ");
    Serial.print(beatAvg);
    Serial.print(" BPM, SpO2: ");
    Serial.print(lastValidSpO2);
    Serial.print(" %, BP: ");
    Serial.print(systolicBP);
    Serial.print("/");
    Serial.print(diastolicBP);
    Serial.print(" (Beats: ");
    Serial.print(validBeatsCount);
    Serial.println(")");
    
    // Reset for next calculation
    avered = 0;
    aveir = 0;
    i = 0;
  }
  
  // Update display at regular intervals
  if (millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
  
  delay(10);
}