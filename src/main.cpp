#include <Arduino.h>
#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include <Adafruit_NeoPixel.h>

/*
====================================================================
CONFIGURATION
====================================================================
*/

// Wi-Fi Credentials
#define WIFI_SSID           "YOUR_WIFI_NAME"
#define WIFI_PASSWORD       "YOUR_WIFI_PASSWORD"

// Gmail IMAP Credentials
// NOTE: For Gmail, you MUST use an App Password (16 characters, no spaces)
// Generated at: https://myaccount.google.com/apppasswords
#define IMAP_HOST           "imap.gmail.com"
#define IMAP_PORT           993
#define AUTHOR_EMAIL        "yourgmail@gmail.com"
#define AUTHOR_PASSWORD     "YOUR_APP_PASSWORD"

// Hardware Pin Configuration
#define NEOPIXEL_PIN        27       // GPIO connected to NeoPixel Data In (DIN)
#define NUM_PIXELS          1        // Number of NeoPixels (1 for single pixel, or N for strip/ring)
#define BUZZER_PIN          25       // GPIO connected to Passive Piezo Buzzer (+)
#define BUZZER_CHANNEL      0        // ESP32 LEDC PWM channel for buzzer
#define STATUS_LED_PIN      26       // Optional standard LED pin (GPIO26)

// Email Polling Timing
const unsigned long POLL_INTERVAL_MS = 60000; // Check every 60 seconds (adjust as needed)

// NeoPixel Brightness (0 - 255)
#define NEOPIXEL_BRIGHTNESS 180

/*
====================================================================
DEVICE STATES & GLOBAL VARIABLES
====================================================================
*/

enum DeviceState {
    STATE_BOOT,
    STATE_WIFI_CONNECTING,
    STATE_WIFI_DISCONNECTED,
    STATE_CHECKING_EMAIL,
    STATE_NO_UNREAD,         // Green: All emails read
    STATE_ONE_UNREAD,        // Orange: 1 unread email (1st poll)
    STATE_URGENT_UNREAD,     // Red: >1 unread OR 1 unread persisting >= 2 polls
    STATE_ERROR              // Error connecting to IMAP
};

volatile DeviceState currentState = STATE_BOOT;

// Email tracking
IMAPSession imap;
int lastUnreadCount = -1;
int consecutiveUnreadPolls = 0;
unsigned long lastPollTime = 0;
uint32_t totalPollCount = 0;

// NeoPixel Instance
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

/*
====================================================================
BUZZER HELPER FUNCTIONS (LEDC PWM)
====================================================================
*/

void initBuzzer() {
    ledcSetup(BUZZER_CHANNEL, 2000, 8);
    ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
    ledcWriteTone(BUZZER_CHANNEL, 0);
}

void stopBuzzer() {
    ledcWriteTone(BUZZER_CHANNEL, 0);
}

void playToneNote(int freq, int durationMs, int pauseMs = 30) {
    if (freq > 0) {
        ledcWriteTone(BUZZER_CHANNEL, freq);
        digitalWrite(STATUS_LED_PIN, HIGH);
    }
    delay(durationMs);
    stopBuzzer();
    digitalWrite(STATUS_LED_PIN, LOW);
    if (pauseMs > 0) {
        delay(pauseMs);
    }
}

// Startup Melody: C5, E5, G5, C6
void soundPowerOn() {
    playToneNote(523, 100);
    playToneNote(659, 100);
    playToneNote(784, 120);
    playToneNote(1046, 200, 50);
}

// WiFi Connected Chime: G5, C6
void soundWifiConnected() {
    playToneNote(784, 100);
    playToneNote(1046, 180, 50);
}

// New Email Alert: Playful chime (C6, E6, G6)
void soundNewEmailAlert() {
    playToneNote(1046, 110);
    playToneNote(1318, 110);
    playToneNote(1567, 180, 50);
}

// Urgent Unread Alert: Attention-grabbing alarm (A6, E6, A6, C7)
void soundUrgentEmailAlert() {
    for (int i = 0; i < 2; i++) {
        playToneNote(1760, 90, 20); // A6
        playToneNote(1318, 90, 20); // E6
        playToneNote(1760, 90, 20); // A6
        playToneNote(2093, 150, 60);// C7
    }
}

// IMAP/Network Error Buzzer
void soundError() {
    playToneNote(392, 150); // G4
    playToneNote(330, 250); // E4
}

/*
====================================================================
NEOPIXEL ANIMATION ENGINE (Non-blocking, running on FreeRTOS Task)
====================================================================
*/

// Color scaling utility
uint32_t scaleColor(uint8_t r, uint8_t g, uint8_t b, uint8_t factor) {
    uint8_t scR = (uint16_t(r) * factor) >> 8;
    uint8_t scG = (uint16_t(g) * factor) >> 8;
    uint8_t scB = (uint16_t(b) * factor) >> 8;
    return pixels.Color(scR, scG, scB);
}

// Fill all pixels with single color
void setAllPixels(uint32_t color) {
    for (int i = 0; i < NUM_PIXELS; i++) {
        pixels.setPixelColor(i, color);
    }
}

// Authentic human heartbeat pulse calculation:
// Returns a brightness factor (0 to 255) with a realistic dual-beat "lub-dub" rhythm
uint8_t calculateHeartbeatFactor(uint32_t cycleDurationMs = 1300) {
    uint32_t t = millis() % cycleDurationMs;
    float factor = 0.08f; // Ambient baseline glow (so LED is never completely dark)

    // Beat 1 ("Lub") - Fast rise and decay
    if (t < 220) {
        float phase = (float)t / 220.0f;
        float pulse = sinf(phase * 3.14159f);
        factor += 0.85f * (pulse * pulse);
    }
    // Beat 2 ("Dub") - Slightly softer second pulse
    else if (t >= 290 && t < 500) {
        float phase = (float)(t - 290) / 210.0f;
        float pulse = sinf(phase * 3.14159f);
        factor += 0.60f * (pulse * pulse);
    }
    // Rest / Diastole (500ms to cycleDurationMs): sits gently at baseline

    if (factor > 1.0f) factor = 1.0f;
    return (uint8_t)(factor * 255.0f);
}

void renderNeoPixelFrame() {
    uint32_t now = millis();

    switch (currentState) {
        case STATE_BOOT: {
            // Soft white/cyan fade-in during boot
            uint8_t b = (now % 1000 < 500) ? (now % 500) * 255 / 500 : (500 - (now % 500)) * 255 / 500;
            setAllPixels(scaleColor(100, 200, 255, b));
            break;
        }

        case STATE_WIFI_CONNECTING: {
            // Smooth Sky-Blue breathing pulse (~800ms period)
            float angle = (now % 800) * (2.0f * 3.14159f / 800.0f);
            uint8_t breath = (uint8_t)((sinf(angle) * 0.5f + 0.5f) * 230 + 25);
            setAllPixels(scaleColor(0, 150, 255, breath));
            break;
        }

        case STATE_WIFI_DISCONNECTED: {
            // Blinking Magenta warning (400ms ON, 400ms OFF)
            bool blinkOn = (now % 800) < 400;
            setAllPixels(blinkOn ? pixels.Color(255, 0, 128) : pixels.Color(0, 0, 0));
            break;
        }

        case STATE_CHECKING_EMAIL: {
            // "Check Email" Animation:
            // Fast, energetic cyan-purple swirling shimmer
            if (NUM_PIXELS == 1) {
                // Single pixel: Fast dual-color pulsating shimmer (period 400ms)
                float angle = (now % 450) * (2.0f * 3.14159f / 450.0f);
                uint8_t breath = (uint8_t)((sinf(angle) * 0.5f + 0.5f) * 220 + 35);
                // Transitions between vibrant cyan and electric violet
                if ((now / 450) % 2 == 0) {
                    setAllPixels(scaleColor(0, 220, 255, breath)); // Cyan
                } else {
                    setAllPixels(scaleColor(180, 50, 255, breath)); // Violet
                }
            } else {
                // Multi-pixel strip/ring: Rotary chaser animation
                int activePixel = (now / 70) % NUM_PIXELS;
                for (int i = 0; i < NUM_PIXELS; i++) {
                    int dist = (i - activePixel + NUM_PIXELS) % NUM_PIXELS;
                    uint8_t brightness = (dist == 0) ? 255 : (dist == 1 ? 120 : (dist == 2 ? 40 : 5));
                    pixels.setPixelColor(i, scaleColor(0, 220, 255, brightness));
                }
            }
            break;
        }

        case STATE_NO_UNREAD: {
            // Green Heartbeat (0 unread emails - all clear)
            uint8_t hb = calculateHeartbeatFactor(1300);
            setAllPixels(scaleColor(0, 255, 0, hb));
            break;
        }

        case STATE_ONE_UNREAD: {
            // Orange Heartbeat (1 unread email)
            uint8_t hb = calculateHeartbeatFactor(1200);
            setAllPixels(scaleColor(255, 120, 0, hb));
            break;
        }

        case STATE_URGENT_UNREAD: {
            // Urgent Red Heartbeat / Pulse:
            // Faster cadence (850ms cycle) for urgent unread
            uint8_t hb = calculateHeartbeatFactor(850);
            setAllPixels(scaleColor(255, 0, 0, hb));
            break;
        }

        case STATE_ERROR: {
            // Error: Flashing Amber/Red warning (250ms interval)
            bool blinkOn = (now % 500) < 250;
            setAllPixels(blinkOn ? pixels.Color(255, 30, 0) : pixels.Color(40, 0, 0));
            break;
        }
    }

    pixels.show();
}

// Background FreeRTOS task dedicated to silky smooth NeoPixel rendering (50 Hz)
void neoPixelAnimationTask(void *parameter) {
    pixels.begin();
    pixels.setBrightness(NEOPIXEL_BRIGHTNESS);
    pixels.show();

    while (true) {
        renderNeoPixelFrame();
        vTaskDelay(pdMS_TO_TICKS(20)); // Run at 50 FPS
    }
}

/*
====================================================================
WIFI CONNECTION
====================================================================
*/

void connectWiFi() {
    Serial.println();
    Serial.println(F("========================================"));
    Serial.printf("Connecting to Wi-Fi SSID: %s\n", WIFI_SSID);
    Serial.println(F("========================================"));

    currentState = STATE_WIFI_CONNECTING;
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(300);
        Serial.print(F("."));
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println(F("Wi-Fi Connected successfully!"));
        Serial.print(F("IP Address: "));
        Serial.println(WiFi.localIP());
        Serial.print(F("Signal Strength (RSSI): "));
        Serial.print(WiFi.RSSI());
        Serial.println(F(" dBm"));

        digitalWrite(STATUS_LED_PIN, HIGH);
        soundWifiConnected();
    } else {
        Serial.println();
        Serial.println(F("Wi-Fi Connection FAILED! Will retry in main loop."));
        currentState = STATE_WIFI_DISCONNECTED;
        soundError();
    }
}

/*
====================================================================
IMAP EMAIL CHECKING ENGINE
====================================================================
*/

bool pollGmailUnreadCount(int &unreadCount) {
    Serial.println();
    Serial.println(F("----------------------------------------"));
    Serial.printf("[Poll #%u] Checking Gmail (%s)...\n", totalPollCount + 1, AUTHOR_EMAIL);
    Serial.println(F("----------------------------------------"));

    Session_Config config;
    config.server.host_name = IMAP_HOST;
    config.server.port = IMAP_PORT;
    config.login.email = AUTHOR_EMAIL;
    config.login.password = AUTHOR_PASSWORD;

    IMAP_Data imap_data;
    imap_data.fetch.uid.clear();
    // Search criteria for unread emails in IMAP
    imap_data.search.criteria = F("SEARCH UNSEEN");
    imap_data.search.unseen_msg = true;
    imap_data.storage.type = esp_mail_file_storage_type_none;
    imap_data.download.header = false;
    imap_data.download.text = false;
    imap_data.download.html = false;
    imap_data.download.attachment = false;
    imap_data.download.inlineImg = false;
    imap_data.limit.search = 100; // Count up to 100 unread emails

    imap.debug(0); // Set to 1 if deep debug logs are needed

    if (!imap.connect(&config, &imap_data)) {
        Serial.printf("IMAP Connection Failed! Code: %d, Reason: %s\n", imap.errorCode(), imap.errorReason().c_str());
        return false;
    }

    if (!imap.selectFolder(F("INBOX"))) {
        Serial.printf("Failed to open INBOX! Code: %d, Reason: %s\n", imap.errorCode(), imap.errorReason().c_str());
        imap.closeSession();
        return false;
    }

    SelectedFolderInfo folder = imap.selectedFolder();
    Serial.printf("INBOX Total Messages: %d\n", folder.msgCount());

    // Check if there are any unseen messages
    if (folder.unseenIndex() == 0) {
        // unseenIndex == 0 indicates no unread messages in the folder
        unreadCount = 0;
    } else {
        // Execute search to get the exact unread count
        if (MailClient.readMail(&imap, false)) {
            unreadCount = imap.selectedFolder().searchCount();
        } else {
            // If unseenIndex > 0 but search failed, at least 1 unread message exists
            unreadCount = (folder.unseenIndex() > 0) ? 1 : 0;
        }
    }

    imap.closeSession();
    imap.empty();
    return true;
}

void performEmailCheck() {
    // Show email checking animation on NeoPixel
    currentState = STATE_CHECKING_EMAIL;

    int currentUnread = 0;
    bool success = pollGmailUnreadCount(currentUnread);

    totalPollCount++;

    if (!success) {
        Serial.println(F("[!] Email poll failed! Setting STATE_ERROR."));
        currentState = STATE_ERROR;
        soundError();
        return;
    }

    Serial.printf("[i] Unread Emails: %d | Consecutive Polls with Unread: %d\n", currentUnread, consecutiveUnreadPolls);

    if (currentUnread == 0) {
        // No unread emails found -> GREEN
        consecutiveUnreadPolls = 0;
        currentState = STATE_NO_UNREAD;
        Serial.println(F("[✓] Status: ALL CLEAR (NeoPixel: GREEN Heartbeat)"));
    }
    else if (currentUnread == 1) {
        consecutiveUnreadPolls++;
        Serial.printf("[!] 1 Unread email detected (Poll count for this unread: %d)\n", consecutiveUnreadPolls);

        if (consecutiveUnreadPolls >= 2) {
            // One unread is not read even after two polls -> RED
            currentState = STATE_URGENT_UNREAD;
            Serial.println(F("[⚠] Status: 1 UNREAD STILL UNREAD AFTER 2+ POLLS! (NeoPixel: RED Alert)"));
            // Sound urgent alert if this was the poll that escalated to RED
            if (consecutiveUnreadPolls == 2) {
                soundUrgentEmailAlert();
            } else {
                soundNewEmailAlert();
            }
        } else {
            // First poll with 1 unread email -> ORANGE
            currentState = STATE_ONE_UNREAD;
            Serial.println(F("[!] Status: 1 UNREAD EMAIL (NeoPixel: ORANGE Heartbeat)"));
            soundNewEmailAlert();
        }
    }
    else { // currentUnread > 1
        consecutiveUnreadPolls++;
        // More than one unread emails found -> RED
        currentState = STATE_URGENT_UNREAD;
        Serial.printf("[⚠] Status: %d UNREAD EMAILS (>1)! (NeoPixel: RED Alert)\n", currentUnread);
        soundUrgentEmailAlert();
    }

    // Detect if new unread emails arrived since last check
    if (lastUnreadCount != -1 && currentUnread > lastUnreadCount) {
        Serial.println(F("[🔔] NEW EMAIL DETECTED since last check!"));
        if (currentUnread > 1 || consecutiveUnreadPolls >= 2) {
            soundUrgentEmailAlert();
        } else {
            soundNewEmailAlert();
        }
    }

    lastUnreadCount = currentUnread;
}

/*
====================================================================
SETUP & INITIALIZATION
====================================================================
*/

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("      ESP32 EMAIL NOTIFIER STARTUP      "));
    Serial.println(F("========================================"));

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    // Initialize Passive Buzzer
    initBuzzer();

    // Start background FreeRTOS task for non-blocking NeoPixel animations
    // Pinned to Core 0 (leaving Core 1 free for Wi-Fi and IMAP polling)
    xTaskCreatePinnedToCore(
        neoPixelAnimationTask,
        "NeoPixelAnimTask",
        3072,
        NULL,
        1,
        NULL,
        0
    );

    // Play startup chime
    soundPowerOn();

    // Connect to Wi-Fi
    connectWiFi();

    // Perform initial email check immediately after connecting
    if (WiFi.status() == WL_CONNECTED) {
        performEmailCheck();
        lastPollTime = millis();
    }
}

/*
====================================================================
MAIN EXECUTION LOOP
====================================================================
*/

void loop() {
    unsigned long currentMillis = millis();

    // Wi-Fi Connection Monitor & Auto-Reconnect
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("\n[!] Wi-Fi connection lost! Attempting reconnect..."));
        connectWiFi();
    }

    // Periodic Email Polling Timer
    if (currentMillis - lastPollTime >= POLL_INTERVAL_MS) {
        lastPollTime = currentMillis;

        if (WiFi.status() == WL_CONNECTED) {
            performEmailCheck();
        }
    }

    // Small yield for background housekeeping
    delay(50);
}
