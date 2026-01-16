# Real-Time IoT Health Monitor

![Project Poster](poster.jpg) 

## 📝 Project Description
A real-time health monitoring system built with ESP32 and the MAX30102 sensor. This project tracks Heart Rate, SpO2, and estimated Blood Pressure, syncing data wirelessly to the Blynk app. It features a local LCD interface and a buzzer that provides real-time audible feedback for every detected heartbeat.

## 🚀 Features
- **Live Vitals:** High-accuracy Heart Rate and SpO2 tracking.
- **Heartbeat Sync:** Real-time buzzer beeps on every pulse.
- **Cloud Connectivity:** IoT dashboard for remote monitoring via Blynk.
- **Emergency Alerts:** Visual warnings for abnormal health metrics.

## 🛠️ Hardware Used
- ESP32s Microcontroller
- MAX30102 High-Sensitivity Pulse Oximeter
- 20x4 LCD
- Buzzer
- LED

## 📂 Repository Structure
- `Health_Monitor.ino`: Main project firmware.
- `heartRate.h / .cpp`: Custom signal processing library for beat detection.
