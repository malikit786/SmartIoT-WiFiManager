/**
 * ============================================================
 *  SmartIoT Core - WiFi Manager
 *  File    : WiFiManager.ino
 *  Version : 1.0.0
 *  Target  : ESP8266 - Wemos D1 Mini
 * ============================================================
 *
 *  Arduino IDE Settings:
 *    Board       : LOLIN(WEMOS) D1 R2 & mini
 *    Flash Size  : 4MB (FS:2MB OTA:~1019KB)
 *    CPU Speed   : 80 MHz
 *
 * ============================================================
 */

#define DEVICE_NAME "SmartIoT"

#include "SmartWiFiManager.h"

// ─────────────────────────────────────────────────────────────
//  Your application server (example)
//  Replace with your actual server
// ─────────────────────────────────────────────────────────────

// ESP8266WebServer myServer(80);

// ─────────────────────────────────────────────────────────────
//  Timing
// ─────────────────────────────────────────────────────────────

static const unsigned long BLINK_INTERVAL_MS  = 500UL;
static const unsigned long STATUS_INTERVAL_MS = 10000UL;
static unsigned long       lastBlinkTime       = 0UL;
static unsigned long       lastStatusTime      = 0UL;
static bool                ledState            = false;

// ─────────────────────────────────────────────────────────────
//  setup()
// ─────────────────────────────────────────────────────────────

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // ── Callbacks ─────────────────────────────────────────────
    // Register before begin() so no events are missed

    SmartWiFi.onConnect([]() {
        Serial.println(F("[App] WiFi connected - start app server"));
        // myServer.begin(80);
    });

    SmartWiFi.onDisconnect([]() {
        Serial.println(F("[App] WiFi lost - stop app server"));
        // myServer.stop();
    });

    SmartWiFi.onPortalStart([]() {
        Serial.println(F("[App] Portal starting - free port 80"));
        // myServer.stop();
    });

    SmartWiFi.onPortalStop([]() {
        Serial.println(F("[App] Portal stopped"));
        if (SmartWiFi.isWiFiConnected()) {
            Serial.println(F("[App] Restarting app server"));
            // myServer.begin(80);
        }
    });

    SmartWiFi.begin();
}

// ─────────────────────────────────────────────────────────────
//  loop()
// ─────────────────────────────────────────────────────────────

void loop()
{
    SmartWiFi.loop();
    // myServer.handleClient();

    unsigned long now = millis();

    // ── Diagnostics — every 10 seconds ───────────────────────
    if (now - lastStatusTime >= STATUS_INTERVAL_MS)
    {
        lastStatusTime = now;

        if (SmartWiFi.isWiFiConnected())
        {
            Serial.println(F("--- SmartWiFi Status ---"));
            Serial.print(F("SSID      : ")); Serial.println(SmartWiFi.getSSID());
            Serial.print(F("IP        : ")); Serial.println(SmartWiFi.getIP());
            Serial.print(F("Gateway   : ")); Serial.println(SmartWiFi.getGateway());
            Serial.print(F("RSSI      : ")); Serial.println(SmartWiFi.getRSSI());
            Serial.print(F("Uptime    : ")); Serial.println(SmartWiFi.getUptime());
            Serial.print(F("Reconnects: ")); Serial.println(SmartWiFi.getReconnectCount());
            Serial.print(F("Portal    : ")); Serial.println(SmartWiFi.isPortalActive() ? "Active" : "Inactive");
            Serial.println(F("------------------------"));
        }
        else
        {
            Serial.print(F("--- SmartWiFi: "));
            Serial.println(SmartWiFi.getStateString());
        }
    }

    // ── On demand portal — example: button on GPIO0 ──────────
    // if (digitalRead(0) == LOW) {
    //     SmartWiFi.openPortal();   // delete credentials + restart
    // }

    // ── Your firmware tasks ───────────────────────────────────
    // sensor.read();
    // battery.monitor();
    // automation.run();

    // ── LED blink ─────────────────────────────────────────────
    if (now - lastBlinkTime >= BLINK_INTERVAL_MS)
    {
        lastBlinkTime = now;
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
    }
}
