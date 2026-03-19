# 🌦️ SkyCast: Environment Monitoring & Analysis System

![Platform](https://img.shields.io/badge/Platform-ESP32-blue)
![Stack](https://img.shields.io/badge/Stack-C%2B%2B%20%7C%20HTML%20%7C%20CSS%20%7C%20Chart.js%20%7C%20GSAP-orange)
![Storage](https://img.shields.io/badge/Storage-SPIFFS-green)

A **full-stack IoT environmental monitoring system** powered by ESP32 that tracks **Temperature, Humidity, Air Quality (AQI), and Rain Intensity** in real-time.

SkyCast features a **futuristic animated dashboard**, **live analytics**, **CSV logging**, and **smart alerting system**, all hosted directly on the ESP32.

---

## 🚀 Key Features

### ⚡ Real-Time Dashboard
- Hosted directly on ESP32 using WebServer
- Modern UI with **GSAP animations**
- Live updates every **2 seconds**
- Responsive design

### 🌡️ Environmental Monitoring
- Temperature & Humidity (DHT11)
- Air Quality Index (MQ2 → AQI conversion)
- Rain Detection (Analog + Digital)

### 📊 Advanced Data Visualization
- Live charts using **Chart.js**
- Real-time trend analysis
- Historical analytics:
  - Max / Min / Average values

### ⚠️ Smart Alert System
- AQI threshold-based alert
- Rain detection alert
- LED indicator on ESP32
- Dashboard warning banner

### 💾 Persistent Data Logging
- Stored in **SPIFFS (`/data.csv`)**
- Automatic logging every **60 seconds**
- Data survives reboot

### 📥 Data Export & Control
- Download CSV directly from dashboard
- Clear stored data
- Adjust AQI threshold dynamically

### 🌍 Time Synchronization
- Uses **NTP (pool.ntp.org, time.google.com)**
- Accurate timestamps for logs

### 🌐 Network Features
- WiFi-enabled access
- Local web server
- REST API support

---

## 🛠️ Hardware & Pinout

| Component        | ESP32 Pin (GPIO) | Description |
|-----------------|-----------------|------------|
| **DHT11**       | GPIO 4          | Temperature & Humidity |
| **MQ2 Sensor**  | GPIO 34         | Gas / Air Quality |
| **Rain Analog** | GPIO 35         | Rain Intensity |
| **Rain Digital**| GPIO 26         | Rain Detection |
| **LED**         | GPIO 2          | Alert Indicator |

---

## 📡 API Documentation

| Endpoint | Method | Description |
|----------|--------|------------|
| `/` | GET | Web Dashboard |
| `/sensor` | GET | Returns JSON data |
| `/setthreshold?value=INT` | GET | Set AQI threshold |
| `/export` | GET | Download CSV data |
| `/clear` | GET | Clear stored data |
| `/meta` | GET | Get ESP32 IP |

### Example JSON Response
```json
{
  "temperature": 28.5,
  "humidity": 65.2,
  "aqi": 120,
  "rainPct": 10,
  "isRaining": false,
  "aqiAlert": false
}
