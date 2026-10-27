# IoT Smart Energy Meter using ESP32 and Blynk 2.0

A Wi-Fi based smart energy meter that measures voltage and current with an ESP32, calculates power and energy consumption, and shows everything live on a **Blynk IoT 2.0** web and mobile dashboard. A **Telegram bot** sends energy consumption and bill details.



---

> ⚠️ **SAFETY WARNING**
> This project connects to **AC mains voltage**, which can cause serious injury or death. Only build it if you know how to work safely with mains wiring. Use proper insulation, a closed enclosure, and a fuse, and never touch the circuit while it is powered.

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware Components](#hardware-components)
- [Software Requirements](#software-requirements)
- [Circuit Diagram](#circuit-diagram)
- [Circuit Connections](#circuit-connections)
- [Blynk 2.0 Setup](#blynk-20-setup)
- [Telegram Bot Setup](#telegram-bot-setup)
- [Code](#code)
- [How It Works](#how-it-works)
- [Project Structure](#project-structure)
- [Output](#output)
- [Troubleshooting and Tips](#troubleshooting-and-tips)
- [Credits](#credits)

---

## Overview

This project is an open-source, modular IoT energy meter for monitoring household power use from a phone or browser. The ESP32 reads:

- **Current** from an ACS712 sensor (single phase)
- **Voltage** from a ZMPT101B sensor module

From these readings, along with the phase angle, the code can work out real power, apparent power, and power factor. The values are sent over Wi-Fi (TCP/IP) to the Blynk cloud, which provides the web and mobile dashboards. A Telegram bot delivers consumption and bill information.

The project falls under Consumer IoT (CIoT): monitoring appliances like water heaters, air conditioners, and dishwashers to manage energy use more efficiently than with traditional manual meter reading.

## Features

- Real-time voltage, current, and power monitoring
- Units (kWh) consumed and total cost calculation
- Energy saved in EEPROM so it survives power loss, with flash-wear protection
- Push-button voltage calibration and data reset
- Blynk 2.0 web dashboard and mobile app
- Telegram bot for billing and consumption messages
- Local readout on a 16x2 I2C LCD
- Low-cost, open-source, easy to extend

## Hardware Components

| Component | Qty | Purpose |
|---|---|---|
| ESP32 development board | 1 | Microcontroller with Wi-Fi |
| ACS712 current sensor | 1 | Current measurement |
| ZMPT101B voltage sensor module | 1 | AC voltage measurement |
| 16x2 I2C LCD display | 1 | Local display |
| 40 W bulb (load) | 1 | Test load |
| 5 V AC-DC converter | 1 | Power supply |
| Connecting wires | - | Wiring |
| Multimeter | 1 | Optional, for calibration/verification |
| AC mains supply | - | Power source for the load |

**Why ESP32 and not ESP8266 NodeMCU?** The NodeMCU has only one analog input, but this project needs two analog readings (voltage and current). The ESP32 has many ADC pins.

## Software Requirements

- [Arduino IDE](https://www.arduino.cc/en/software) with ESP32 board support installed
- [Blynk IoT 2.0](https://blynk.io/) account (web and mobile)
- [Telegram](https://telegram.org/) account
- Arduino libraries used by the sketch: `Blynk`, `LiquidCrystal_I2C`, `ACS712`, `ZMPT101B`, `ArduinoJson` (plus the built-in `WiFi`, `HTTPClient`, `EEPROM` and `Wire`)

## Circuit Diagram

![IoT Smart Energy Meter using ESP32 circuit diagram](images/circuit-diagram.png)

## Circuit Connections

**ZMPT101B voltage sensor**

| Sensor pin | ESP32 pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| OUT | GPIO 35 (ADC) |

**ACS712 current sensor**

| Sensor pin | ESP32 pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| OUT | GPIO 34 (ADC) |

**EEPROM reset / calibration button**

| Button | ESP32 pin |
|---|---|
| One leg | GPIO 4 |
| Other leg | GND |

(The sketch uses `INPUT_PULLUP`, so no external resistor is needed.)

Pin numbers above match the sketch in [`code/`](code/). The full wiring, including the HLK-10M05 5 V supply and the mains side, is in the circuit diagram.

**I2C LCD display**

| LCD pin | ESP32 pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

## Blynk 2.0 Setup

1. **Sign up** on the Blynk 2.0 platform.
2. **Create a template:** Templates tab → New Template → name it (for example "IoT Energy Meter") → hardware: ESP32.
3. **Create datastreams** (Virtual Pins):

   | Parameter | Virtual Pin |
   |---|---|
   | Voltage | V0 |
   | Current | V1 |
   | Power | V2 |
   | Units consumed | V3 |
   | Total cost | V4 |

4. **Save the template** and note the **Template ID** and **Template Name**, plus the **Auth Token** after creating a device.
5. **Create a device** from the template (Devices tab → New Device).
6. **Web dashboard:** open the device's web dashboard, add Gauge and Label widgets for each value, and link each widget to its datastream.
7. **Mobile dashboard:** install the Blynk IoT app, log in with the same account, add the device, and add Gauge / Value Display widgets linked to the same datastreams.

Notifications and alerts can also be added through Blynk's built-in features.

## Telegram Bot Setup

1. Open Telegram, search for **BotFather**, and send `/start`.
2. Send `/newbot`, then choose a name and a username (it must end in `bot`).
3. Copy the **Bot Token** that BotFather gives you.
4. Send any message to your new bot, then find your **Chat ID** with a Telegram ID bot or the Bot API `getUpdates` method.
5. In the sketch, set `telegramBotToken` and `telegramChatID`. The ESP32 posts to the Telegram Bot API over HTTPS (using `HTTPClient` and `ArduinoJson`) and sends an energy report every 10 minutes (`TELEGRAM_INTERVAL`).

## Code

The ESP32 sketch is [`code/iot_smart_energy_meter.ino`](code/iot_smart_energy_meter.ino).

1. Install the libraries listed above in Arduino IDE.
2. Edit the **User Configuration** block at the top: Wi-Fi name and password, Blynk Template ID / Name / Auth Token, Telegram bot token and chat ID, `COST_PER_KWH`, and `REFERENCE_VOLTAGE`.
3. Select your ESP32 board and port, then upload.

**Settings in the sketch**

| Setting | Default | Meaning |
|---|---|---|
| `COST_PER_KWH` | 8.0 | Tariff per unit |
| `REFERENCE_VOLTAGE` | 235.1 | Your actual mains voltage, used for calibration |
| `EEPROM_SAVE_THRESHOLD` | 0.01 kWh | Energy is saved to flash only after this much accumulates, to limit flash wear |
| `TELEGRAM_INTERVAL` | 600000 ms | How often the Telegram report is sent |

**Button (GPIO 4)**
- Hold about 1 second and release: calibrate the voltage reading against `REFERENCE_VOLTAGE` (stored in EEPROM).
- Hold 3 seconds or more and release: reset the stored energy and cost to zero.

**LCD pages** (change every 4 seconds): voltage and current, power and energy, total cost.

> The sketch ships with placeholder credentials. Do not commit your real Wi-Fi password, Blynk token, or Telegram token to a public repository.

## How It Works

1. The ZMPT101B and ACS712 produce analog signals proportional to mains voltage and load current.
2. The ESP32 samples both on its ADC pins and computes RMS voltage and RMS current.
3. Power values (real power, apparent power, power factor) are derived from these readings and the phase angle.
4. Energy (kWh) is accumulated over time and multiplied by the tariff to get the cost.
5. Values show on the LCD and are pushed over Wi-Fi to Blynk (V0 to V4).
6. The Telegram bot sends the consumption and bill summary.

## Project Structure

```
iot-smart-energy-meter-esp32/
├── README.md
├── .gitignore
├── code/
│   └── iot_smart_energy_meter.ino
└── images/
    ├── circuit-diagram.png
    └── hardware-and-blynk-app.png
```

## Output

The LCD shows live voltage, current and power while the Blynk mobile dashboard shows voltage, current, power, total energy and total cost.

![Hardware setup with LCD and Blynk mobile dashboard](images/hardware-and-blynk-app.png)

## Troubleshooting and Tips

- Calibrate both sensors against a multimeter, since the ZMPT101B and ACS712 need calibration for accurate readings.
- The ACS712 variants differ in sensitivity (5A: 185 mV/A, 20A: 100 mV/A, 30A: 66 mV/A). Set the value in code to match your module.
- Check your sensor module's voltage rating and the ESP32 ADC input range before powering up.
- Add alerts for high consumption or commands for the current bill as extra features.

## Credits

Original tutorial and circuit design: [circuitdiagrams.in](https://circuitdiagrams.in/iot-smart-energy-meter-using-esp32/). This repository is a personal project documenting my build based on that tutorial.

## License

Add a license of your choice (for example MIT) before publishing.
