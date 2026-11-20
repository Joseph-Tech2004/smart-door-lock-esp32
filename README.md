# Smart Door Lock — ESP32 · RFID · Telegram

An IoT door lock that opens with an **RFID card** or remotely from **Telegram**, and reports every lock/unlock event to the owner's phone.

> Status: working prototype (v1). A v2 with improved security and remote-management features is planned. Demo video coming soon.

---

## Features

- **RFID access** — an authorised MFRC522 card toggles the lock; unknown cards are ignored. Includes a debounce so one tap is read once.
- **Telegram control** — reply-keyboard buttons (🔓 Open / 🔒 Lock) and a `/open <code>` command protected by a secret code.
- **Owner-only bot** — messages from any other Telegram chat are refused.
- **Live notifications** — every state change is sent to the owner with the method used (RFID, Telegram button, Telegram code), plus a "System booted" message on power-up.
- **Local feedback** — red/green status LEDs and distinct buzzer patterns for lock and unlock.
- **Secure connection** — Telegram traffic over TLS using the Telegram root certificate.

## How it works

```
 RFID card ──► MFRC522 ──SPI──┐
                              ▼
 Telegram app ◄──TLS/Wi-Fi──► ESP32 ──PWM──► Servo (lock bolt)
                              │
                              ├──► Red / green LEDs
                              └──► Buzzer
```

The main loop polls Telegram for new messages every second and checks the RFID reader continuously. Both paths call the same `lockDoor()` / `unlockDoor()` functions, so the LEDs, buzzer and Telegram notification always stay in sync with the real lock state.

## Hardware

| Component | Notes |
|---|---|
| ESP32 Dev board | Wi-Fi + main controller |
| MFRC522 RFID reader (13.56 MHz) + card | Powered from **3.3 V only** |
| MG995 servo | Moves the lock bolt; powered from the 5 V supply, not the ESP32 |
| Red and green LEDs + 220 Ω resistors | Locked / unlocked indicator |
| Active buzzer | Audio feedback |
| 5 V 5 A power supply | Common ground with the ESP32 |

### Pin map (as used in the firmware)

| Signal | ESP32 GPIO |
|---|---|
| RFID SDA (SS) | 5 |
| RFID SCK | 18 |
| RFID MOSI | 23 |
| RFID MISO | 19 |
| RFID RST | 4 |
| Servo signal | 15 |
| Red LED | 25 |
| Green LED | 26 |
| Buzzer | 27 |

## Setup

1. Install the Arduino libraries: **UniversalTelegramBot**, **MFRC522**, **ESP32Servo** (and ArduinoJson, required by UniversalTelegramBot).
2. Open [`firmware/door_lock/door_lock.ino`](firmware/door_lock/door_lock.ino) and fill in your Wi-Fi name and password, your Telegram bot token (from @BotFather), your chat ID, and your own unlock code.
3. Upload, open the Serial Monitor at 115200 baud, tap your card, and copy its UID into `authorizedUID[]`.
4. Upload again and send `/start` to your bot.

## Next steps (v2)

- Multiple authorised cards managed from Telegram, stored in flash.
- Access log with timestamps.
- Auto-lock after a timeout and a door-position sensor.

## Author

**Yousif Ezzat Salih** — Computer Technology Engineering, Al-Bayan University, Baghdad
