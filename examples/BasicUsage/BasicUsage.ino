/**
 * ============================================================
 *  SmartIoT Core - WiFi Manager
 *  Example : BasicUsage
 *  File    : BasicUsage.ino
 *  Version : 1.0.0
 *  Target  : ESP8266 - Wemos D1 Mini
 * ============================================================
 *
 *  Arduino IDE Settings:
 *    Board       : LOLIN(WEMOS) D1 R2 & mini
 *    Flash Size  : 4MB (FS:2MB OTA:~1019KB)
 *    CPU Speed   : 80 MHz
 *    Upload Speed: 921600
 *
 *  How it works:
 *    1. First boot  -> Captive portal opens (SmartIoT-XXXXXX)
 *    2. Connect phone to that network
 *    3. Open 192.168.4.1 -> Enter WiFi credentials
 *    4. Device saves credentials and connects automatically
 *    5. All future boots -> connects to saved network
 *
 *  On demand portal:
 *    Call SmartWiFi.openPortal() from anywhere
 *    e.g. button press, Firebase command, web command
 *
 * ============================================================
 */

// Device name — portal will broadcast: SmartIoT-XXXXXX
// Change this for each product
#define DEVICE_NAME "SmartIoT"

#include <SmartWiFiManager.h>

// ─────────────────────────────────────────────────────────────
//  Optional: Your application web server on port 80
//  Use callbacks below to coordinate with portal
// ─────────────────────────────────────────────────────────────

// #include <ESP8266WebServer.h>
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
    digitalWrite(LED_BUILTIN, HIGH);   // OFF — active LOW on ESP8266

    // Register callbacks BEFORE begin() — no events missed
    SmartWiFi.onConnect([]() {
        Serial.println(F("[App] WiFi connected — start your services here"));
        // myServer.begin(80);
    });

    SmartWiFi.onDisconnect([]() {
        Serial.println(F("[App] WiFi lost — stop your services here"));
        // myServer.stop();
    });

    SmartWiFi.onPortalStart([]() {
        Serial.println(F("[App] Portal starting — free port 80"));
        // myServer.stop();
    });

    SmartWiFi.onPortalStop([]() {
        Serial.println(F("[App] Portal stopped"));
        if (SmartWiFi.isWiFiConnected()) {
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

    // Print status every 10 seconds
    if (now - lastStatusTime >= STATUS_INTERVAL_MS)
    {
        lastStatusTime = now;

        if (SmartWiFi.isWiFiConnected())
        {
            Serial.println(F("--- SmartWiFi Status ---"));
            Serial.print(F("SSID      : ")); Serial.println(SmartWiFi.getSSID());
            Serial.print(F("IP        : ")); Serial.println(SmartWiFi.getIP());
            Serial.print(F("Gateway   : ")); Serial.println(SmartWiFi.getGateway());
            Serial.print(F("MAC       : ")); Serial.println(SmartWiFi.getMAC());
            Serial.print(F("RSSI      : ")); Serial.println(SmartWiFi.getRSSI());
            Serial.print(F("Uptime    : ")); Serial.println(SmartWiFi.getUptime());
            Serial.print(F("Reconnects: ")); Serial.println(SmartWiFi.getReconnectCount());
            Serial.print(F("Portal    : ")); Serial.println(SmartWiFi.isPortalActive() ? "Active" : "Inactive");
            Serial.println(F("------------------------"));
        }
        else
        {
            Serial.print(F("[WiFi] State: "));
            Serial.println(SmartWiFi.getStateString());
        }
    }

    // On demand portal — Flash button (GPIO0) example
    // if (digitalRead(0) == LOW) {
    //     delay(50);
    //     if (digitalRead(0) == LOW) {
    //         SmartWiFi.openPortal();
    //     }
    // }

    // Your firmware tasks — always run regardless of WiFi
    // sensor.read();
    // battery.monitor();
    // automation.run();

    // LED blink — proves loop is never blocked by WiFiManager
    if (now - lastBlinkTime >= BLINK_INTERVAL_MS)
    {
        lastBlinkTime = now;
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
    }
}
