# 🌦️ Mini Weather & AQI Monitoring System

![Platform](https://img.shields.io/badge/Platform-ESP32-blue) ![Stack](https://img.shields.io/badge/Stack-C%2B%2B%20%7C%20HTML%20%7C%20Chart.js-orange)

A full-stack IoT solution that monitors environmental conditions (Temperature, Humidity, Air Quality) and hosts a **responsive, real-time web dashboard** directly on the ESP32.

Features persistent data logging via SPIFFS, dynamic charts, and a RESTful API.

---

## 🚀 Key Features

* **⚡ Real-Time Dashboard:** Hosted on the ESP32 using Async Web Server. Features **Chart.js** for live gradient-filled data visualization.
* **💾 Persistent Logging:** Automatically saves sensor readings to an internal CSV file (`/data.csv`) using **SPIFFS**. Data survives reboots.
* **⚠️ Smart Alerts:** Configurable MQ2 gas threshold. Triggers LED alert and UI warning when air quality drops.
* **🌍 NTP Time Sync:** Synchronizes with `pool.ntp.org` to timestamp all data entries accurately.
* **📱 Data Export:** Download historical data directly from the dashboard as a `.csv` file.

---

## 🛠️ Hardware & Pinout

| Component | ESP32 Pin (GPIO) | Description |
| :--- | :--- | :--- |
| **DHT11** | `GPIO 4` | Temperature & Humidity Sensor |
| **MQ2** | `GPIO 34` | Analog Gas/Smoke Sensor |
| **Status LED** | `GPIO 2` | Active High (Alert Trigger) |

---

## 📡 API Documentation

The system exposes a lightweight REST API for integration with other services.

| Endpoint | Method | Params | Description |
| :--- | :--- | :--- | :--- |
| `/sensor` | `GET` | None | Returns JSON data: `{"temperature": 24.5, "humidity": 60, "mq2": 450, "alert": false}` |
| `/setthreshold` | `GET` | `?value=INT` | Updates the gas alert threshold dynamically (e.g., `/setthreshold?value=800`). |
| `/export` | `GET` | None | Downloads the `data.csv` file containing historical logs. |
| `/clear` | `GET` | None | **Dangerous:** Wipes the CSV log file from SPIFFS storage. |
| `/meta` | `GET` | None | Returns the device's local IP address. |

---

## ⚙️ Installation & Setup

### 1. Prerequisites
* [Arduino IDE](https://www.arduino.cc/en/software)
* **Libraries Required:**
    * `WiFi.h` (Built-in)
    * `WebServer.h` (Built-in)
    * `DHT Sensor Library` by Adafruit
    * `Adafruit Unified Sensor`

### 2. Configuration
To keep your WiFi credentials safe, this project uses a separate `secrets.h` file.

1.  Clone the repository.
2.  Create a file named `secrets.h` in the project root.
3.  Add the following code to it:
    ```cpp
    const char* SECRET_SSID = "YOUR_WIFI_NAME";
    const char* SECRET_PASS = "YOUR_WIFI_PASSWORD";
    ```
    *(Note: `secrets.h` is included in `.gitignore` to prevent accidental publication)*

### 3. Flash the Firmware
1.  Open `Mini_Weather_AQI_System.ino`.
2.  Select your Board (e.g., DOIT ESP32 DEVKIT V1).
3.  **Important:** Ensure you do not need to upload SPIFFS data manually; the code handles file creation (`ensureCsvHeader()`) automatically on the first boot.
4.  Upload & Monitor Serial at `115200` baud.

---

## 🤝 Contribution
1.  Fork the Project
2.  Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3.  Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4.  Push to the Branch (`git push origin feature/AmazingFeature`)
5.  Open a Pull Request