#include <Arduino.h>
#include <WiFi.h>
#include <ESP_Mail_Client.h>

/*
====================================================
WIFI CONFIGURATION
====================================================
*/

#define WIFI_SSID       "YOUR_WIFI_NAME"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

/*
====================================================
GMAIL IMAP CONFIGURATION
====================================================
IMPORTANT:
Use Google App Password
NOT your normal Gmail password
====================================================
*/

#define IMAP_HOST       "imap.gmail.com"
#define IMAP_PORT       993

#define AUTHOR_EMAIL    "yourgmail@gmail.com"
#define AUTHOR_PASSWORD "YOUR_APP_PASSWORD"

/*
====================================================
GPIO CONFIGURATION
====================================================
*/

#define LED_PIN             26
#define BUZZER_PIN          25
#define BUZZER_CHANNEL      0

/*
====================================================
GLOBAL VARIABLES
====================================================
*/

IMAPSession imap;

int lastUnreadCount = -1;

unsigned long lastCheck = 0;
const unsigned long checkInterval = 60000; // 60 seconds

/*
====================================================
TONE FUNCTIONS
====================================================
*/

void stopTone()
{
    ledcWriteTone(BUZZER_CHANNEL, 0);
}

void playTone(int freq, int duration)
{
    ledcWriteTone(BUZZER_CHANNEL, freq);

    digitalWrite(LED_PIN, HIGH);

    delay(duration);

    stopTone();

    digitalWrite(LED_PIN, LOW);

    delay(40);
}

/*
====================================================
MELODIES
====================================================
*/

void playPowerOnMelody()
{
    playTone(523, 120);   // C5
    playTone(659, 120);   // E5
    playTone(784, 150);   // G5
    playTone(1046, 220);  // C6
}

void playWifiConnectedMelody()
{
    playTone(784, 100);   // G5
    playTone(1046, 180);  // C6
}

void playNewEmailMelody()
{
    playTone(1046, 120);  // C6
    playTone(1318, 120);  // E6
    playTone(1567, 180);  // G6
}

/*
====================================================
LED STATUS FUNCTIONS
====================================================
*/

void wifiConnectingAnimation()
{
    digitalWrite(LED_PIN, HIGH);
    delay(150);

    digitalWrite(LED_PIN, LOW);
    delay(150);
}

void wifiConnectedIndicator()
{
    digitalWrite(LED_PIN, HIGH);
}

/*
====================================================
CONNECT TO WIFI
====================================================
*/

void connectWiFi()
{
    Serial.println();
    Serial.println("Connecting to WiFi...");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        wifiConnectingAnimation();
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi Connected!");

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    wifiConnectedIndicator();

    playWifiConnectedMelody();
}

/*
====================================================
CHECK EMAIL
====================================================
*/

void checkEmail()
{
    Serial.println();
    Serial.println("Checking email...");

    Session_Config config;

    config.server.host_name = IMAP_HOST;
    config.server.port = IMAP_PORT;

    config.login.email = AUTHOR_EMAIL;
    config.login.password = AUTHOR_PASSWORD;

    config.login.user_domain = "";

    IMAP_Data imap_data;

    imap.debug(0);

    if (!imap.connect(&config, &imap_data))
    {
        Serial.println("IMAP Connection Failed");

        digitalWrite(LED_PIN, LOW);

        return;
    }

    if (!imap.selectFolder("INBOX"))
    {
        Serial.println("Failed to open INBOX");

        imap.closeSession();

        return;
    }

    int unreadCount = imap.selectedFolder().msgCountUnread();

    Serial.print("Unread Emails: ");
    Serial.println(unreadCount);

    /*
    ====================================================
    FIRST RUN
    ====================================================
    */

    if (lastUnreadCount == -1)
    {
        lastUnreadCount = unreadCount;
    }

    /*
    ====================================================
    NEW EMAIL DETECTION
    ====================================================
    */

    if (unreadCount > lastUnreadCount)
    {
        Serial.println("NEW EMAIL DETECTED!");

        playNewEmailMelody();
    }

    lastUnreadCount = unreadCount;

    imap.closeSession();

    wifiConnectedIndicator();
}

/*
====================================================
SETUP
====================================================
*/

void setup()
{
    Serial.begin(115200);

    pinMode(LED_PIN, OUTPUT);

    digitalWrite(LED_PIN, LOW);

    /*
    ====================================================
    BUZZER PWM SETUP
    ====================================================
    */

    ledcSetup(BUZZER_CHANNEL, 2000, 8);
    ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);

    /*
    ====================================================
    POWER ON MELODY
    ====================================================
    */

    playPowerOnMelody();

    /*
    ====================================================
    CONNECT WIFI
    ====================================================
    */

    connectWiFi();

    /*
    ====================================================
    INITIAL EMAIL CHECK
    ====================================================
    */

    checkEmail();
}

/*
====================================================
MAIN LOOP
====================================================
*/

void loop()
{
    if (millis() - lastCheck >= checkInterval)
    {
        lastCheck = millis();

        /*
        ================================================
        WIFI RECONNECT
        ================================================
        */

        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("WiFi Lost! Reconnecting...");

            digitalWrite(LED_PIN, LOW);

            connectWiFi();
        }

        /*
        ================================================
        CHECK EMAIL
        ================================================
        */

        checkEmail();
    }
}