/**
 * ============================================================
 *  SmartIoT Core — WiFi Manager
 *  File    : SmartWiFiManager.h
 *  Version : 1.0.0-phase3
 *  Target  : ESP8266 (Wemos D1 Mini) / Arduino Framework
 * ============================================================
 *
 *  Philosophy:
 *    WiFi is a service — not the heart of the firmware.
 *    Inspired by Tasmota. Built from scratch.
 *    Main loop is NEVER blocked under any condition.
 *
 *  Usage:
 *    #define DEVICE_NAME "SmartIoT"
 *    #include "SmartWiFiManager.h"
 *    SmartWiFi.begin();
 *    SmartWiFi.loop();
 *
 * ============================================================
 */

#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <functional>

// ─────────────────────────────────────────────────────────────
//  Device name — softcoded by user in sketch
//  Portal SSID: DEVICE_NAME-CHIPID
//  Example   : SmartIoT-FED282
// ─────────────────────────────────────────────────────────────

#ifndef DEVICE_NAME
  #define DEVICE_NAME "SmartIoT"
#endif

// ─────────────────────────────────────────────────────────────
//  Timing constants — all in milliseconds
// ─────────────────────────────────────────────────────────────

#define WIFI_TIMEOUT_LAST_MS         120000UL  // 120s  last connected at boot
#define WIFI_TIMEOUT_LAST_RUNTIME_MS  60000UL  //  60s  last connected at runtime
#define WIFI_TIMEOUT_PRIMARY_MS       60000UL  //  60s  WiFi1 attempt
#define WIFI_TIMEOUT_BACKUP_MS        60000UL  //  60s  WiFi2 attempt
#define WIFI_RUNTIME_SWITCH_MS       120000UL  //   2m  runtime disconnect wait
#define WIFI_PORTAL_TIMEOUT_MS       180000UL  //   3m  portal auto-close
#define WIFI_SERIAL_INTERVAL_MS        5000UL  //   5s  heartbeat interval
#define WIFI_PORTAL_RESTART_DELAY_MS   5000UL  //   5s  delay before restart
#define WIFI_DOT_INTERVAL_MS          10000UL  //  10s  progress dot interval
#define WIFI_RECONNECT_INTERVAL_MS    30000UL  //  30s  retry same network interval

// ─────────────────────────────────────────────────────────────
//  LittleFS config file
// ─────────────────────────────────────────────────────────────

#define WIFI_CONFIG_FILE   "/wifi.json"   ///< Credentials — written only by portal
#define WIFI_LAST_FILE     "/last.json"   ///< Last SSID — written at runtime
#define WIFI_IP_WAIT_MS      8000UL       ///< Wait for DHCP IP after connect

// ─────────────────────────────────────────────────────────────
//  Serial logging — always on, uptime timestamp every line
// ─────────────────────────────────────────────────────────────

#define WIFI_LOG(msg) \
    do { _printUptime(); Serial.println(F(msg)); } while(0)

#define WIFI_LOG_VAL(msg, val) \
    do { _printUptime(); Serial.print(F(msg)); Serial.println(val); } while(0)

// ─────────────────────────────────────────────────────────────
//  State machine
// ─────────────────────────────────────────────────────────────

enum SmartWiFiState
{
    WIFI_UNINITIALISED  = -1,  ///< Sentinel — before begin()
    WIFI_BOOT           =  0,  ///< Hardware init — clean WiFi stack
    WIFI_LOAD_SETTINGS  =  1,  ///< Mount LittleFS — load credentials
    WIFI_CONNECT_LAST   =  2,  ///< Try last connected network  (Phase 3)
    WIFI_CONNECT_PRIMARY=  3,  ///< Try WiFi1                   (Phase 3)
    WIFI_CONNECT_BACKUP =  4,  ///< Try WiFi2                   (Phase 3)
    WIFI_CONNECTED      =  5,  ///< Steady state — link up      (Phase 3)
    WIFI_DISCONNECTED   =  6,  ///< Runtime disconnect           (Phase 3)
    WIFI_PORTAL         =  7   ///< Captive portal active
};

// ─────────────────────────────────────────────────────────────
//  Credential storage
// ─────────────────────────────────────────────────────────────

struct WiFiCredentials
{
    char ssid[33];      ///< Max 32 chars + null
    char password[65];  ///< Max 64 chars + null
};

// ─────────────────────────────────────────────────────────────
//  SmartWiFiManager
// ─────────────────────────────────────────────────────────────

class SmartWiFiManager
{
public:

    // ── Constructor ──────────────────────────────────────────
    SmartWiFiManager();

    // ── Public API ───────────────────────────────────────────

    /** begin() — call once from setup() */
    void begin();

    /** loop() — call every iteration of main loop() — never blocks */
    void loop();

    /** getState() — returns current state machine state */
    SmartWiFiState getState() const;

    /** hasCredentials() — true if at least WiFi1 is saved */
    bool hasCredentials() const;

    // ── Diagnostics API ──────────────────────────────────────
    //  Tasmota-inspired status reporting
    //  Safe to call from any firmware task at any time
    //  All functions are const — read only, never modify state

    /**
     *  isWiFiConnected()
     *  Returns true when ESP8266 has a valid IP address.
     *  Local LAN communication is available.
     *  Use this before any local network operation.
     */
    bool isWiFiConnected() const;

    /**
     *  isInternetConnected()
     *  Returns true when public internet is reachable.
     *  Use this before Firebase, MQTT, OTA, NTP calls.
     *
     *  NOTE: Always returns false in v1.0
     *  Internet check will be implemented in a future phase.
     *  Architecture is ready — no changes needed when implemented.
     */
    bool isInternetConnected() const;

    /**
     *  getSSID()
     *  Returns connected SSID or empty string if not connected.
     */
    String getSSID() const;

    /**
     *  getIP()
     *  Returns IP address string or "0.0.0.0" if not connected.
     */
    String getIP() const;

    /**
     *  getGateway()
     *  Returns gateway IP string or "0.0.0.0" if not connected.
     */
    String getGateway() const;

    /**
     *  getMAC()
     *  Returns MAC address string — always available.
     *  Format: "5C:CF:7F:FE:D2:82"
     */
    String getMAC() const;

    /**
     *  getRSSI()
     *  Returns signal strength in dBm.
     *  Returns 0 if not connected.
     *  Typical values: -30 (excellent) to -90 (weak)
     */
    int32_t getRSSI() const;

    /**
     *  getUptime()
     *  Returns time connected as "HH:MM:SS" string.
     *  Resets on each new connection.
     *  Returns "00:00:00" if not connected.
     */
    String getUptime() const;

    /**
     *  getReconnectCount()
     *  Returns total number of reconnections since boot.
     *  Useful for monitoring connection stability.
     */
    uint8_t getReconnectCount() const;

    // ── Callbacks ─────────────────────────────────────────────
    //  Register functions to be called on WiFi events.
    //  Use lambda or function pointer.
    //
    //  IMPORTANT: Keep callbacks SHORT and NON-BLOCKING.
    //  Callbacks run inside WiFiManager loop().
    //  Never use delay() or blocking code inside a callback.
    //
    //  Usage:
    //    SmartWiFi.onConnect([]() {
    //        myServer.begin(80);
    //    });
    //    SmartWiFi.onPortalStart([]() {
    //        myServer.stop();   // free port 80 for portal
    //    });

    /** Called when WiFi connects and valid IP obtained */
    void onConnect(std::function<void()> callback);

    /** Called when WiFi connection is lost */
    void onDisconnect(std::function<void()> callback);

    /** Called when captive portal SoftAP starts — free port 80 */
    void onPortalStart(std::function<void()> callback);

    /** Called when captive portal SoftAP stops — reclaim port 80 */
    void onPortalStop(std::function<void()> callback);

    /**
     *  openPortal()
     *  Deletes saved credentials and restarts device.
     *  On next boot captive portal will open automatically.
     *
     *  Use for:
     *    - Button press to reconfigure WiFi
     *    - Firebase/MQTT command to reset WiFi
     *    - Factory reset scenario
     *
     *  WARNING: Device will restart immediately.
     *  Save any pending data before calling.
     */
    void openPortal();

    /**
     *  isPortalActive()
     *  Returns true when captive portal SoftAP is running.
     *  Use this to stop any user web server on port 80
     *  before portal starts to avoid port conflict.
     *
     *  Usage:
     *    SmartWiFi.onPortalStart([]() { myServer.stop(); });
     *    SmartWiFi.onPortalStop([]()  { myServer.begin(80); });
     */
    bool isPortalActive() const;

    /**
     *  getStateString()
     *  Returns current state as human-readable string.
     *  Useful for local web UI and diagnostics page.
     */
    String getStateString() const;

private:

    // ── State machine ────────────────────────────────────────
    SmartWiFiState  _currentState;
    void            setState(SmartWiFiState newState);
    bool            _stateFirstRun;   ///< true on first loop() in new state

    // ── Diagnostics state ────────────────────────────────────
    bool          _wifiConnected;      ///< true = valid IP obtained
    bool          _internetConnected;  ///< placeholder - always false v1.0
    unsigned long _wifiConnectedSince; ///< millis() when connection confirmed

    // ── State handlers ───────────────────────────────────────
    void handleBoot();
    void handleLoadSettings();
    void handleConnectLast();
    void handleConnectPrimary();
    void handleConnectBackup();
    void handleConnected();
    void handleDisconnected();
    void handlePortal();

    // ── Connection helpers ───────────────────────────────────
    void     startConnection(const char* ssid, const char* password);
    bool     checkConnection();
    void     onConnected();
    void     printConnectionInfo();
    bool     isSameSSID(const char* a, const char* b);
    unsigned long _connectStartTime;   ///< When current attempt started
    unsigned long _dotPrintTime;       ///< Last dot printed
    unsigned long _disconnectTime;     ///< When disconnect happened
    unsigned long _connectedTime;      ///< When connection confirmed
    bool          _connectBeginCalled; ///< WiFi.begin() already called
    bool          _waitingForIP;       ///< Connected — waiting for DHCP IP
    bool          _isRuntimeReconnect; ///< true=runtime disconnect false=boot
    uint8_t       _reconnectCount;     ///< Total reconnection counter

    // ── Callbacks ─────────────────────────────────────────────
    std::function<void()> _onConnectCb;
    std::function<void()> _onDisconnectCb;
    std::function<void()> _onPortalStartCb;
    std::function<void()> _onPortalStopCb;

    // ── Portal ───────────────────────────────────────────────
    ESP8266WebServer  _server;           ///< Web server on port 80
    bool              _portalStarted;       ///< SoftAP + server running
    bool              _saveRequested;       ///< Credentials received from form
    bool              _restartPending;      ///< Waiting to restart
    bool              _portalWifiConnected; ///< WiFi connected while portal active
    unsigned long     _restartTime;      ///< When to restart
    unsigned long     _portalStartTime;  ///< When portal started

    void startPortal();
    void stopPortal();
    void handleRoot();
    void handleSave();
    void handleScan();
    void handleNotFound();

    // ── Credentials ──────────────────────────────────────────
    WiFiCredentials _wifi1;
    WiFiCredentials _wifi2;
    char            _lastSSID[33];
    bool            _hasWifi1;
    bool            _hasWifi2;
    bool            _hasLastSSID;

    // ── Filesystem ───────────────────────────────────────────
    bool mountFilesystem();
    bool loadCredentials();
    bool loadLastSSID();
    bool saveCredentials();
    bool saveLastSSID();
    bool verifyFile(const char* path);
    bool validateSSID(const char* ssid);

    // ── Timers ───────────────────────────────────────────────
    unsigned long _lastUpdateTime;
    unsigned long _lastReconnectTime;
    unsigned long _lastInternetCheckTime;
    unsigned long _lastScanTime;
    unsigned long _stateEnteredTime;

    // ── Serial helpers ───────────────────────────────────────
    void _printUptime()            const;
    void _printBanner()            const;
    void _printCredentialStatus()  const;
    void _maskPassword(const char* pass, char* masked, size_t len) const;
    String _getDeviceName()        const;
};

// ─────────────────────────────────────────────────────────────
//  Global singleton
// ─────────────────────────────────────────────────────────────

extern SmartWiFiManager SmartWiFi;
