# RC-BLE-Car
# 🏎️ BLE RC Car with Audio Effects

An interactive, web-controlled Remote Control (RC) car driven by an **Arduino Uno R4 WiFi**, utilizing Bluetooth Low Energy (BLE) and an **ISD1820 Voice Playback Module** for custom audio/sound effects (e.g., horn, engine rev, or custom voice clips).

---

## 🛠️ Hardware Components

* **Microcontroller:** Arduino Uno R4 WiFi (Native BLE support)
* **Motor Driver:** L293D Motor Shield
* **Audio Module:** ISD1820 Voice Playback Module with Mini Speaker
* **Chassis:** 2WD / 4WD Smart Car Chassis Kit
* **Power Source:** 2x 18650 Li-ion Batteries (or 6–9V battery pack)

---

## 🚀 Features

* **Web-Based BLE Control:** Connect directly from any modern web browser (Chrome/Edge on mobile or desktop) using Web Bluetooth—no native mobile app installation required!
* **Dual Motor Drive:** Full direction control (Forward, Reverse, Left, Right, Stop) powered by the L293D shield.
* **Audio FX:** Trigger recorded sounds or horn effects using the ISD1820 module via web UI buttons.

---

## 🔌 Pin Wiring & Connections

| Component | Pin / Connection | Notes |
| :--- | :--- | :--- |
| **L293D Shield** | Mounted directly on Arduino Uno R4 WiFi | Drives DC Motors M1–M4 |
| **ISD1820 (Trigger)** | Digital Pin (e.g., `D2` or `A0`) | Connected to `PE` (Play Edge) or `P-L` pin |
| **ISD1820 (Power)** | `5V` & `GND` | Powered from Arduino |
| **Motors** | Shield Terminals `M1`, `M2`, `M3`, `M4`, | Standard DC Motors |

> ⚠️ **Power Note:** Always power the L293D motor shield with an external battery pack to prevent resetting the Arduino board when motors start spinning. Remove the shield jumper if supplying power separately.

---

## 💻 Web Interface

The web interface connects to the Arduino's BLE service using the Web Bluetooth API. 

### How to Connect:
1. Turn on the RC Car.
2. Open the web interface (`ble.html`) in Google Chrome or Microsoft Edge (make sure Bluetooth is enabled on your device).
3. Click **Connect BLE**.
4. Select your car from the device list (e.g., `BLE-RC-Car`).
5. Use the on-screen controls or keyboard arrows to steer and trigger audio playback!

---

## 📜 How to Flash the Code

1. Install the latest [Arduino IDE](https://www.arduino.cc/en/software).
2. Install the **Arduino UNO R4 Boards** core in `Tools` > `Board` > `Boards Manager`.
3. Install required libraries (e.g., `AFMotor` for the L293D shield, `ArduinoBLE`).
4. Select **Arduino UNO R4 WiFi** under `Tools` > `Board`.
5. Upload the sketch to your board.

---

## 📂 Project Structure

```text
├── README.md
├── testingcar.ino   # Arduino Uno R4 WiFi BLE & motor logic
└── ble.html
