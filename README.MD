# ESP32 Email Notifier

A smart, Wi-Fi enabled email notification gadget built with an ESP32 Dev Board. It periodically polls your Gmail inbox over secure IMAP (SSL) and provides real-time ambient visual indications via a **WS2812B NeoPixel RGB LED** alongside audible alerts via a **passive piezo buzzer**.

---

# Features

* **Wi-Fi Connectivity**: Auto-connect and auto-reconnect if connection is lost.
* **Gmail IMAP Integration**: Secure SSL polling of Gmail using an App Password.
* **Non-Blocking FreeRTOS NeoPixel Engine**: Runs smooth 50 FPS animations on Core 0 so network polling never causes LED stuttering or lag.
* **Ambient Heartbeat Pulse**: Authentic organic double-beat ("lub-dub") breathing animation in color-coded states.
* **Smart Multi-Stage Email Escalation**:
  * **Green Heartbeat**: All caught up (0 unread emails).
  * **Orange Heartbeat**: 1 unread email detected.
  * **Red Alert Pulse**: Escalates to Red if >1 unread emails exist, OR if 1 unread email remains unread after 2 consecutive polls.
  * **Checking Email Animation**: Dynamic cyan/violet shimmer (or rotary chaser for multi-pixel strips) while polling IMAP.
* **Buzzer Melodies**:
  * Power ON melody
  * Wi-Fi Connected chime
  * New Email chime
  * Urgent/Escalated Email alarm
  * Error warning tone
* **PlatformIO Compatible**: Ready to build and upload out of the box.

---

# Hardware Requirements

| Component | Quantity | Notes |
| --- | --- | --- |
| ESP32 Dev Board (ESP32-WROOM-32) | 1 | 30-pin or 38-pin module |
| WS2812B / NeoPixel RGB LED | 1 or more | Single pixel, 8-LED stick, or ring |
| Passive Piezo Buzzer | 1 | Driven via ESP32 LEDC PWM |
| Single Blue/Red LED (optional) | 1 | Status indication |
| 220Ω - 330Ω Resistors | 1-2 | Current-limiting for LED and DIN |
| Jumper Wires & Breadboard/PCB | As required | |
| Micro-USB Cable | 1 | For power and programming |

---

# Circuit Diagram & Pinout

| Function | ESP32 GPIO | Connection |
| --- | --- | --- |
| **NeoPixel DIN (Data In)** | **GPIO 27** | Connect to DIN through a 330Ω resistor (VCC to 5V/VIN or 3.3V, GND to GND) |
| **Passive Piezo Buzzer (+)** | **GPIO 25** | Positive pin of buzzer to GPIO25, negative pin to GND |
| **Status LED (Optional)** | **GPIO 26** | Anode through 220Ω resistor to GPIO26, Cathode to GND |

### Wiring Diagram

```text
               +------------------------------------+
               |           ESP32 Dev Board          |
               +------------------------------------+
                                 |
       GPIO27 ----------------[ 330Ω ]------------ DIN (NeoPixel WS2812B)
       5V/VIN ------------------------------------ VDD / 5V
       GND    ------------------------------------ GND
                                 |
       GPIO25 ------------------------------------ Buzzer (+)
       GND    ------------------------------------ Buzzer (-)
                                 | (Optional Single LED)
       GPIO26 ----------------[ 220Ω ]------------ Anode (+)
       GND    ------------------------------------ Cathode (-)
```

---

# NeoPixel Visual States & Animations

| State | NeoPixel Color | Animation Behavior | Trigger Condition |
| --- | --- | --- | --- |
| **Booting** | Soft White / Sky Blue | Smooth gentle fade-in | Startup initialization |
| **Wi-Fi Connecting** | Sky Blue (`#0096FF`) | Smooth breathing pulse (~800ms period) | Trying to connect to Wi-Fi |
| **Wi-Fi Disconnected** | Magenta (`#FF0080`) | Blinking warning (400ms ON / 400ms OFF) | Wi-Fi lost / reconnecting |
| **Checking Email** | Vibrant Cyan / Electric Violet | Fast dual-color pulsating shimmer (or chaser) | Actively polling IMAP server |
| **All Clear (0 Unread)** | **Green** (`#00FF00`) | **Heartbeat pulse** (organic double "lub-dub") | 0 unread emails in INBOX |
| **1 Unread Email** | **Orange** (`#FF7800`) | **Heartbeat pulse** in warm orange | 1 unread email on 1st poll |
| **Urgent Unread** | **Red** (`#FF0000`) | **Fast Heartbeat / Alert pulse** | **>1 unread emails** OR **1 unread persisting ≥ 2 polls** |
| **Error / Auth Failure** | Amber / Dark Red | Rapid warning flash (250ms) | IMAP login failed / connection error |

---

# Melody Notifications

* **Power ON**: Ascending welcome arpeggio (`C5, E5, G5, C6`)
* **Wi-Fi Connected**: Double success chime (`G5, C6`)
* **New Email Received**: Playful alert chime (`C6, E6, G6`)
* **Urgent Unread Alert**: Attention-grabbing alarm (`A6, E6, A6, C7` repeated)
* **Error**: Low descending warning tones (`G4, E4`)

---

# Gmail Configuration

### Step 1 — Enable IMAP in Gmail
1. Open [Gmail Settings](https://mail.google.com/mail/u/0/#settings/fwdandpop).
2. Select the **Forwarding and POP/IMAP** tab.
3. Under **IMAP Access**, select **Enable IMAP**.
4. Click **Save Changes**.

### Step 2 — Generate a Google App Password
> **Important:** Embedded devices cannot use your regular Google account password if 2-Step Verification is enabled.
1. Go to [Google Account App Passwords](https://myaccount.google.com/apppasswords).
2. Sign in to your Google Account.
3. Name your app (e.g., `ESP32 Email Notifier`) and click **Create**.
4. Copy the generated **16-character password** (e.g., `abcd efgh ijkl mnop`).

---

# Configuration & Flashing

### 1. Configure Credentials in `src/main.cpp`

Open `src/main.cpp` and update the following settings:

```cpp
// Wi-Fi Credentials
#define WIFI_SSID           "YOUR_WIFI_NAME"
#define WIFI_PASSWORD       "YOUR_WIFI_PASSWORD"

// Gmail Credentials
#define AUTHOR_EMAIL        "yourgmail@gmail.com"
#define AUTHOR_PASSWORD     "abcdefghijklmnop" // 16-character App Password without spaces

// Hardware Pins (Adjust if you use different pins)
#define NEOPIXEL_PIN        27       // Data In pin for NeoPixel
#define NUM_PIXELS          1        // Number of NeoPixels (1 for single, 8 for ring/strip)
#define BUZZER_PIN          25       // Buzzer pin
```

### 2. Build and Upload

Using PlatformIO CLI:

```bash
# Compile project
pio run

# Upload firmware to ESP32
pio run --target upload

# Open Serial Monitor at 115200 baud
pio device monitor
```

---

# Serial Monitor Output Example

```text
========================================
      ESP32 EMAIL NOTIFIER STARTUP      
========================================
Connecting to Wi-Fi SSID: MyHomeWiFi
....
Wi-Fi Connected successfully!
IP Address: 192.168.1.105
Signal Strength (RSSI): -52 dBm

----------------------------------------
[Poll #1] Checking Gmail (yourgmail@gmail.com)...
----------------------------------------
INBOX Total Messages: 412
[✓] Status: ALL CLEAR (NeoPixel: GREEN Heartbeat)

----------------------------------------
[Poll #2] Checking Gmail (yourgmail@gmail.com)...
----------------------------------------
INBOX Total Messages: 413
[!] 1 Unread email detected (Poll count for this unread: 1)
[!] Status: 1 UNREAD EMAIL (NeoPixel: ORANGE Heartbeat)

----------------------------------------
[Poll #3] Checking Gmail (yourgmail@gmail.com)...
----------------------------------------
INBOX Total Messages: 413
[!] 1 Unread email detected (Poll count for this unread: 2)
[⚠] Status: 1 UNREAD STILL UNREAD AFTER 2+ POLLS! (NeoPixel: RED Alert)
```

---

# License

MIT License. Open-source and free to adapt for personal or commercial projects.
