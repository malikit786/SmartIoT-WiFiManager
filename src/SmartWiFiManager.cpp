/**
 * ============================================================
 *  SmartIoT Core - WiFi Manager
 *  File    : WiFiManager.cpp
 *  Version : 1.0.0-phase3
 *  Target  : ESP8266 (Wemos D1 Mini) / Arduino Framework
 * ============================================================
 */

#include "SmartWiFiManager.h"
#include "PortalPages.h"
#include <ArduinoJson.h>

// ─────────────────────────────────────────────────────────────
//  Global singleton
// ─────────────────────────────────────────────────────────────

SmartWiFiManager SmartWiFi;

// ─────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────

SmartWiFiManager::SmartWiFiManager()
    : _currentState(WIFI_UNINITIALISED)
    , _stateFirstRun(false)
    , _wifiConnected(false)
    , _internetConnected(false)
    , _wifiConnectedSince(0)
    , _server(80)
    , _portalStarted(false)
    , _saveRequested(false)
    , _restartPending(false)
    , _portalWifiConnected(false)
    , _restartTime(0)
    , _portalStartTime(0)
    , _hasWifi1(false)
    , _hasWifi2(false)
    , _hasLastSSID(false)
    , _lastUpdateTime(0)
    , _lastReconnectTime(0)
    , _lastInternetCheckTime(0)
    , _lastScanTime(0)
    , _stateEnteredTime(0)
    , _connectStartTime(0)
    , _dotPrintTime(0)
    , _disconnectTime(0)
    , _connectedTime(0)
    , _connectBeginCalled(false)
    , _waitingForIP(false)
    , _isRuntimeReconnect(false)
    , _reconnectCount(0)
{
    memset(&_wifi1,   0, sizeof(_wifi1));
    memset(&_wifi2,   0, sizeof(_wifi2));
    memset(_lastSSID, 0, sizeof(_lastSSID));
}

// ─────────────────────────────────────────────────────────────
//  begin()
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::begin()
{
    Serial.begin(115200);
    delay(300);

    _lastUpdateTime        = millis();
    _lastReconnectTime     = millis();
    _lastInternetCheckTime = millis();
    _lastScanTime          = millis();
    _stateEnteredTime      = millis();

    _printBanner();
    setState(WIFI_BOOT);
}

// ─────────────────────────────────────────────────────────────
//  loop()
//  Never blocks - always returns immediately
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::loop()
{
    switch (_currentState)
    {
        case WIFI_BOOT:            handleBoot();          break;
        case WIFI_LOAD_SETTINGS:   handleLoadSettings();  break;
        case WIFI_PORTAL:          handlePortal();        break;

        case WIFI_CONNECT_LAST:      handleConnectLast();    break;
        case WIFI_CONNECT_PRIMARY:   handleConnectPrimary(); break;
        case WIFI_CONNECT_BACKUP:    handleConnectBackup();  break;
        case WIFI_CONNECTED:         handleConnected();      break;
        case WIFI_DISCONNECTED:      handleDisconnected();   break;

        case WIFI_UNINITIALISED:
            if (millis() - _lastUpdateTime >= WIFI_SERIAL_INTERVAL_MS)
            {
                _lastUpdateTime = millis();
                WIFI_LOG("[WiFi] WARNING: loop() called before begin()");
            }
            break;
    }

    yield();
}

// ─────────────────────────────────────────────────────────────
//  getState() / hasCredentials()
// ─────────────────────────────────────────────────────────────

SmartWiFiState SmartWiFiManager::getState() const { return _currentState; }
bool SmartWiFiManager::hasCredentials()     const { return _hasWifi1; }

// ─────────────────────────────────────────────────────────────
//  setState()
//  Only authorised path for all state transitions
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::setState(SmartWiFiState newState)
{
    if (_currentState == newState) return;

    _currentState     = newState;
    _stateEnteredTime = millis();
    _stateFirstRun    = true;   // signal handlers to run entry logic
}

// ─────────────────────────────────────────────────────────────
//  handleBoot()
//  Clean WiFi stack - runs once
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::handleBoot()
{
    WIFI_LOG("[WiFi] ---- Boot ----");
    WIFI_LOG("[WiFi] Initialising WiFi stack...");

    _isRuntimeReconnect = false;   // boot - use full timeouts

    WiFi.mode(WIFI_STA);
    WIFI_LOG("[WiFi] Mode: Station only");

    WiFi.persistent(false);
    WIFI_LOG("[WiFi] SDK auto-save: Disabled");

    WiFi.disconnect(true);
    WIFI_LOG("[WiFi] SDK credentials: Cleared");

    WIFI_LOG("[WiFi] WiFi stack ready");

    setState(WIFI_LOAD_SETTINGS);
}

// ─────────────────────────────────────────────────────────────
//  handleLoadSettings()
//  Mount LittleFS - load credentials - decide next state
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::handleLoadSettings()
{
    WIFI_LOG("[WiFi] ---- Load Settings ----");

    if (!mountFilesystem())
    {
        WIFI_LOG("[WiFi] Formatting filesystem...");
        LittleFS.format();
        if (!LittleFS.begin())
        {
            WIFI_LOG("[WiFi] Filesystem error - starting portal");
            setState(WIFI_PORTAL);
            return;
        }
        WIFI_LOG("[WiFi] Filesystem formatted OK");
    }

    if (!loadCredentials())
    {
        WIFI_LOG("[WiFi] No credentials found");
        WIFI_LOG("[WiFi] Starting captive portal for setup...");
        setState(WIFI_PORTAL);
        return;
    }

    // Load last SSID from separate file - safe even if corrupt
    loadLastSSID();

    _printCredentialStatus();

    if (_hasLastSSID)
    {
        WIFI_LOG_VAL("[WiFi] Will try last network: ", _lastSSID);
        setState(WIFI_CONNECT_LAST);
    }
    else if (_hasWifi1)
    {
        WIFI_LOG("[WiFi] No last network - trying WiFi1");
        setState(WIFI_CONNECT_PRIMARY);
    }
    else
    {
        WIFI_LOG("[WiFi] No usable credentials - starting portal");
        setState(WIFI_PORTAL);
    }
}

// ─────────────────────────────────────────────────────────────
//  handleConnectLast()
//  Try last connected network - 120 second timeout
//  This gives the router time to boot after a power cut
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::handleConnectLast()
{
    // ── Entry - first time in this state ────────────────────
    if (_stateFirstRun)
    {
        _stateFirstRun = false;

        // Find which credential matches the last SSID
        // Password comes from WiFi1 or WiFi2 - whichever matches
        const char* password = "";
        if (_hasWifi1 && isSameSSID(_wifi1.ssid, _lastSSID))
            password = _wifi1.password;
        else if (_hasWifi2 && isSameSSID(_wifi2.ssid, _lastSSID))
            password = _wifi2.password;

        startConnection(_lastSSID, password);
        return;
    }

    // ── Check if connected ───────────────────────────────────
    if (checkConnection()) return;   // connected - done

    // ── Progress dot every 10 seconds ───────────────────────
    if (millis() - _dotPrintTime >= WIFI_DOT_INTERVAL_MS)
    {
        _dotPrintTime = millis();
        WIFI_LOG("[WiFi] ...");
    }

    // ── Timeout - 120 seconds ───────────────────────────────
    // Boot timeout = 120s, Runtime timeout = 60s
    unsigned long timeout = _isRuntimeReconnect
                          ? WIFI_TIMEOUT_LAST_RUNTIME_MS
                          : WIFI_TIMEOUT_LAST_MS;

    if (millis() - _connectStartTime >= timeout)
    {
        WIFI_LOG_VAL("[WiFi] Timeout: ", _lastSSID);

        // Avoid trying same network twice
        if (_hasWifi1 && !isSameSSID(_wifi1.ssid, _lastSSID))
        {
            WIFI_LOG("[WiFi] Trying WiFi1...");
            setState(WIFI_CONNECT_PRIMARY);
        }
        else if (_hasWifi2 && !isSameSSID(_wifi2.ssid, _lastSSID))
        {
            WIFI_LOG("[WiFi] Trying WiFi2...");
            setState(WIFI_CONNECT_BACKUP);
        }
        else if (_hasWifi1)
        {
            // Last SSID is same as WiFi1 - still try it fresh
            setState(WIFI_CONNECT_PRIMARY);
        }
        else
        {
            WIFI_LOG("[WiFi] All networks failed - starting portal");
            setState(WIFI_PORTAL);
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  handleConnectPrimary()
//  Try WiFi1 - 60 second timeout
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::handleConnectPrimary()
{
    if (_stateFirstRun)
    {
        _stateFirstRun = false;
        startConnection(_wifi1.ssid, _wifi1.password);
        return;
    }

    if (checkConnection()) return;

    if (millis() - _dotPrintTime >= WIFI_DOT_INTERVAL_MS)
    {
        _dotPrintTime = millis();
        WIFI_LOG("[WiFi] ...");
    }

    if (millis() - _connectStartTime >= WIFI_TIMEOUT_PRIMARY_MS)
    {
        WIFI_LOG_VAL("[WiFi] Timeout: ", _wifi1.ssid);

        if (_hasWifi2)
        {
            WIFI_LOG("[WiFi] Trying WiFi2...");
            setState(WIFI_CONNECT_BACKUP);
        }
        else
        {
            WIFI_LOG("[WiFi] All networks failed - starting portal");
            setState(WIFI_PORTAL);
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  handleConnectBackup()
//  Try WiFi2 - 60 second timeout
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::handleConnectBackup()
{
    if (_stateFirstRun)
    {
        _stateFirstRun = false;
        startConnection(_wifi2.ssid, _wifi2.password);
        return;
    }

    if (checkConnection()) return;

    if (millis() - _dotPrintTime >= WIFI_DOT_INTERVAL_MS)
    {
        _dotPrintTime = millis();
        WIFI_LOG("[WiFi] ...");
    }

    if (millis() - _connectStartTime >= WIFI_TIMEOUT_BACKUP_MS)
    {
        WIFI_LOG_VAL("[WiFi] Timeout: ", _wifi2.ssid);
        WIFI_LOG("[WiFi] All networks failed - starting portal");
        setState(WIFI_PORTAL);
    }
}

// ─────────────────────────────────────────────────────────────
//  handleConnected()
//  Steady state - WiFi link is up
//  Monitor connection - detect if it drops
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::handleConnected()
{
    // ── Entry - runs once when state entered ─────────────────
    if (_stateFirstRun)
    {
        _stateFirstRun = false;
        // Connection info already printed in onConnected()
        // where IP is guaranteed valid
        return;
    }

    // ── Monitor connection health ────────────────────────────
    if (WiFi.status() != WL_CONNECTED)
    {
        WIFI_LOG("[WiFi] Connection lost");
        WIFI_LOG_VAL("[WiFi] Was connected to: ", WiFi.SSID().c_str());
        _disconnectTime = millis();
        setState(WIFI_DISCONNECTED);
    }
}

// ─────────────────────────────────────────────────────────────
//  handleDisconnected()
//  Runtime disconnect handler
//
//  Strategy:
//    Wait 5 minutes on same network before trying another
//    This avoids unnecessary switching on temporary drops
//    Firmware keeps running - only cloud comm pauses
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::handleDisconnected()
{
    if (_stateFirstRun)
    {
        _stateFirstRun      = false;
        _wifiConnected      = false;
        _wifiConnectedSince = 0;

        // Fire disconnect callback
        if (_onDisconnectCb) _onDisconnectCb();

        WIFI_LOG("[WiFi] Firmware continues running");
        WIFI_LOG("[WiFi] Waiting before switching network...");
    }

    // ── Progress dot every 10 seconds ───────────────────────
    if (millis() - _dotPrintTime >= WIFI_DOT_INTERVAL_MS)
    {
        _dotPrintTime = millis();
        WIFI_LOG("[WiFi] ...");
    }

    // ── 5 minute wait - then restart connection sequence ────
    if (millis() - _disconnectTime >= WIFI_RUNTIME_SWITCH_MS)
    {
        WIFI_LOG("[WiFi] Restarting connection sequence...");
        _reconnectCount++;
        _isRuntimeReconnect = true;   // runtime - use shorter timeouts

        if (_hasLastSSID)
            setState(WIFI_CONNECT_LAST);
        else if (_hasWifi1)
            setState(WIFI_CONNECT_PRIMARY);
        else
            setState(WIFI_PORTAL);
    }
}

// ─────────────────────────────────────────────────────────────
//  startConnection()
//  Begin a WiFi connection attempt - non-blocking
//  WiFi.begin() called once - status polled in loop()
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::startConnection(const char* ssid, const char* password)
{
    _printUptime();
    Serial.print(F("[WiFi] Connecting to: "));
    Serial.println(ssid);

    // disconnect(true) clears SDK internal cache
    // This prevents WL_CONNECTED being returned from stale cached state
    // Without this, runtime reconnect gets fake WL_CONNECTED immediately
    // with no real DHCP and RSSI=31
    WiFi.disconnect(true);

    WiFi.begin(ssid, password);

    _connectStartTime    = millis();
    _dotPrintTime        = millis();
    _connectBeginCalled  = true;
    _waitingForIP        = false;
}

// ─────────────────────────────────────────────────────────────
//  checkConnection()
//  Poll WiFi.status() - wait for DHCP IP - then transition
//
//  Two stage process:
//  Stage 1: Wait for WL_CONNECTED
//  Stage 2: Wait for valid IP (DHCP complete)
//
//  If DHCP fails - disconnect and retry fresh
//  Never proceed with (IP unset)
// ─────────────────────────────────────────────────────────────

bool SmartWiFiManager::checkConnection()
{
    if (WiFi.status() != WL_CONNECTED) return false;

    // Stage 1 complete - WiFi associated
    // Now wait for DHCP to assign a valid IP
    if (!_waitingForIP)
    {
        _waitingForIP  = true;
        _connectedTime = millis();
        return false;
    }

    // Stage 2 - check if IP is valid
    IPAddress ip = WiFi.localIP();
    bool ipValid = (ip[0] != 0);   // 0.0.0.0 means DHCP not done yet

    if (ipValid)
    {
        // IP confirmed - connection fully established
        onConnected();
        return true;
    }

    // Still waiting for DHCP
    if (millis() - _connectedTime < WIFI_IP_WAIT_MS)
    {
        return false;   // keep waiting
    }

    // DHCP timeout - IP never arrived
    // Disconnect and force startConnection() again via _stateFirstRun
    // CRITICAL: do NOT reset _connectStartTime here
    // The state-level timeout must continue counting toward expiry
    // so that WiFi2 eventually gets tried
    WIFI_LOG("[WiFi] DHCP failed - disconnecting and retrying...");
    WiFi.disconnect(true);   // clear SDK cache on retry too

    _waitingForIP       = false;
    _connectBeginCalled = false;
    _stateFirstRun      = true;   // force startConnection() next loop
    // _connectStartTime intentionally NOT reset here

    return false;
}

// ─────────────────────────────────────────────────────────────
//  onConnected()
//  Called when WiFi connected AND IP confirmed
//
//  Key fix:
//    NEVER touch /wifi.json here - credentials must stay safe
//    Only write /last.json - small separate file
//    Even if /last.json corrupts - credentials are safe
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::onConnected()
{
    // Save last connected SSID in memory
    String ssid = WiFi.SSID();
    strlcpy(_lastSSID, ssid.c_str(), sizeof(_lastSSID));
    _hasLastSSID = true;

    // Set diagnostics state
    _wifiConnected      = true;
    _wifiConnectedSince = millis();

    // Fire connect callback
    if (_onConnectCb) _onConnectCb();

    // Print here - IP is guaranteed valid at this exact moment
    printConnectionInfo();

    // Save last SSID to its own small file
    saveLastSSID();

    setState(WIFI_CONNECTED);
}

// ─────────────────────────────────────────────────────────────
//  printConnectionInfo()
//  Print full connection details to Serial
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::printConnectionInfo()
{
    WIFI_LOG_VAL("[WiFi] Connected: ",    WiFi.SSID().c_str());
    WIFI_LOG_VAL("[WiFi] IP Address: ",   WiFi.localIP().toString().c_str());
    WIFI_LOG_VAL("[WiFi] Gateway: ",      WiFi.gatewayIP().toString().c_str());
    WIFI_LOG_VAL("[WiFi] RSSI: ",         WiFi.RSSI());
    WIFI_LOG_VAL("[WiFi] Reconnects: ",   _reconnectCount);
}

// ─────────────────────────────────────────────────────────────
//  isSameSSID()
//  Case-sensitive SSID comparison
// ─────────────────────────────────────────────────────────────

bool SmartWiFiManager::isSameSSID(const char* a, const char* b)
{
    return (strcmp(a, b) == 0);
}

// ─────────────────────────────────────────────────────────────
//  handlePortal()
//  Non-blocking portal handler - called every loop()
//
//  Entry (first run):
//    - Start SoftAP
//    - Start web server
//    - Register routes
//
//  Every run:
//    - Handle incoming HTTP requests
//    - Check portal timeout
//    - Check restart pending
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::handlePortal()
{
    // ── Entry logic - runs once when state is entered ────────
    if (_stateFirstRun)
    {
        _stateFirstRun = false;
        startPortal();
    }

    // ── Restart pending - wait 5 seconds then restart ────────
    if (_restartPending)
    {
        if (millis() - _restartTime >= WIFI_PORTAL_RESTART_DELAY_MS)
        {
            WIFI_LOG("[WiFi] Restarting now...");
            ESP.restart();
        }
        _server.handleClient();
        return;
    }

    // ── Bug 6 Fix: Monitor WiFi while portal is active ───────
    // Log connection status but DO NOT change state (Bug 7)
    // Portal stays open regardless of WiFi status
    if (WiFi.status() == WL_CONNECTED)
    {
        // Only log once when connection detected
        if (!_portalWifiConnected)
        {
            _portalWifiConnected = true;
            _printUptime();
            Serial.print(F("[WiFi] Network connected while portal active: "));
            Serial.println(WiFi.SSID().c_str());
            _printUptime();
            Serial.print(F("[WiFi] IP Address: "));
            Serial.println(WiFi.localIP().toString().c_str());
            WIFI_LOG("[WiFi] Portal remains open for configuration");
        }
    }
    else
    {
        // Reset flag if disconnected
        if (_portalWifiConnected)
        {
            _portalWifiConnected = false;
            WIFI_LOG("[WiFi] Network disconnected - portal still active");
        }
    }

    // ── Handle incoming HTTP requests - non-blocking ─────────
    _server.handleClient();

    // ── Portal timeout - 3 minutes ───────────────────────────
    if (millis() - _portalStartTime >= WIFI_PORTAL_TIMEOUT_MS)
    {
        WIFI_LOG("[WiFi] Portal timeout - no configuration received");
        stopPortal();

        if (_hasWifi1)
        {
            WIFI_LOG("[WiFi] Trying saved credentials...");
            setState(WIFI_CONNECT_PRIMARY);
        }
        else
        {
            WIFI_LOG("[WiFi] No credentials - restarting portal");
            _stateFirstRun   = true;
            _portalStartTime = millis();
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  startPortal()
//  Start SoftAP and web server
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::startPortal()
{
    WIFI_LOG("[WiFi] ---- Portal ----");

    String apName = _getDeviceName();

    // Start SoftAP - open network, no password
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apName.c_str());

    _printUptime();
    Serial.print(F("[WiFi] SoftAP started: "));
    Serial.println(apName);

    _printUptime();
    Serial.print(F("[WiFi] Portal IP: "));
    Serial.println(WiFi.softAPIP().toString());

    // Register web server routes
    _server.on("/",       [this]() { handleRoot(); });
    _server.on("/save",   [this]() { handleSave(); });
    _server.on("/scan",   [this]() { handleScan(); });
    _server.onNotFound(   [this]() { handleNotFound(); });

    _server.begin();
    WIFI_LOG("[WiFi] Web server started");
    WIFI_LOG("[WiFi] Waiting for configuration...");

    // Fire portal start callback — user should stop their server on port 80
    if (_onPortalStartCb) _onPortalStartCb();

    _portalStarted   = true;
    _portalStartTime = millis();
}

// ─────────────────────────────────────────────────────────────
//  stopPortal()
//  Stop SoftAP and web server cleanly
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::stopPortal()
{
    if (!_portalStarted) return;

    // Fire portal stop callback before shutting down
    // User can reclaim port 80 here
    if (_onPortalStopCb) _onPortalStopCb();

    _server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    _portalStarted = false;

    WIFI_LOG("[WiFi] Portal stopped");
}

// ─────────────────────────────────────────────────────────────
//  handleRoot()
//  GET / - serve configuration page
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::handleRoot()
{
    WIFI_LOG("[WiFi] Client connected to portal");
    WiFi.scanNetworks(true);   // async scan - non-blocking
    _server.send(200, "text/html",
        PortalPages::buildPortalPage(_getDeviceName(), ""));
}

// ─────────────────────────────────────────────────────────────
//  handleScan()
//  GET /scan - return scan results as HTML rows
//  Called by page JavaScript after scan completes
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::handleScan()
{
    int n = WiFi.scanComplete();

    if (n == WIFI_SCAN_RUNNING)
    {
        // Scan still in progress - tell page to wait
        _server.send(200, "text/plain", "scanning");
        return;
    }

    if (n <= 0)
    {
        // No networks found or scan not started yet
        WiFi.scanNetworks(true);
        _server.send(200, "text/plain", "none");
        return;
    }

    WIFI_LOG_VAL("[WiFi] Scan complete - networks found: ", n);
    String result = PortalPages::buildScanResults(n);
    WiFi.scanDelete();
    _server.send(200, "text/html", result);
}

// ─────────────────────────────────────────────────────────────
//  handleSave()
//  POST /save - receive and save credentials from form
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::handleSave()
{
    WIFI_LOG("[WiFi] Credentials received from portal");

    // Read form fields
    String w1ssid = _server.arg("wifi1_ssid");
    String w1pass = _server.arg("wifi1_pass");
    String w2ssid = _server.arg("wifi2_ssid");
    String w2pass = _server.arg("wifi2_pass");

    // Validate - at least WiFi1 SSID must be present
    if (w1ssid.length() == 0)
    {
        WIFI_LOG("[WiFi] Save rejected - WiFi1 SSID is empty");
        _server.send(400, "text/html",
            "<h2>Error: WiFi 1 SSID cannot be empty</h2>"
            "<a href='/'>Go Back</a>");
        return;
    }

    // Store WiFi1
    strlcpy(_wifi1.ssid,     w1ssid.c_str(), sizeof(_wifi1.ssid));
    strlcpy(_wifi1.password, w1pass.c_str(), sizeof(_wifi1.password));
    _hasWifi1 = true;

    _printUptime();
    Serial.print(F("[WiFi] WiFi1: "));
    Serial.println(_wifi1.ssid);

    // Store WiFi2 if provided
    if (w2ssid.length() > 0)
    {
        strlcpy(_wifi2.ssid,     w2ssid.c_str(), sizeof(_wifi2.ssid));
        strlcpy(_wifi2.password, w2pass.c_str(), sizeof(_wifi2.password));
        _hasWifi2 = true;

        _printUptime();
        Serial.print(F("[WiFi] WiFi2: "));
        Serial.println(_wifi2.ssid);
    }

    // Save to LittleFS
    WIFI_LOG("[WiFi] Saving to filesystem...");
    if (saveCredentials())
    {
        WIFI_LOG("[WiFi] Saved OK");
    }
    else
    {
        WIFI_LOG("[WiFi] Save failed - will try connecting anyway");
    }

    String successPage = PortalPages::buildSuccessPage(
        String(_wifi1.ssid),
        WiFi.softAPIP().toString()
    );
    _server.send(200, "text/html", successPage);

    // Schedule restart
    _restartPending = true;
    _restartTime    = millis();

    WIFI_LOG("[WiFi] Restarting in 5 seconds...");
}

// ─────────────────────────────────────────────────────────────
//  handleNotFound()
//  Captive portal redirect - any unknown URL goes to /
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::handleNotFound()
{
    // Redirect all unknown requests to portal page
    // This makes iOS and Android open the portal automatically
    _server.sendHeader("Location", "http://192.168.4.1/", true);
    _server.send(302, "text/plain", "");
}




// ─────────────────────────────────────────────────────────────
//  mountFilesystem()
// ─────────────────────────────────────────────────────────────

bool SmartWiFiManager::mountFilesystem()
{
    WIFI_LOG("[WiFi] Mounting filesystem...");
    if (!LittleFS.begin())
    {
        WIFI_LOG("[WiFi] Filesystem mount failed");
        return false;
    }
    WIFI_LOG("[WiFi] Filesystem mounted OK");
    return true;
}

// ─────────────────────────────────────────────────────────────
//  loadCredentials()
//  Read /wifi.json from LittleFS
// ─────────────────────────────────────────────────────────────

bool SmartWiFiManager::loadCredentials()
{
    WIFI_LOG("[WiFi] Reading config file...");

    if (!LittleFS.exists(WIFI_CONFIG_FILE))
    {
        WIFI_LOG("[WiFi] Config file not found");
        return false;
    }

    File file = LittleFS.open(WIFI_CONFIG_FILE, "r");
    if (!file)
    {
        WIFI_LOG("[WiFi] Could not open config file");
        return false;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err)
    {
        WIFI_LOG("[WiFi] Config file corrupted");
        return false;
    }

    const char* w1ssid = doc["wifi1_ssid"] | "";
    const char* w1pass = doc["wifi1_pass"] | "";
    const char* w2ssid = doc["wifi2_ssid"] | "";
    const char* w2pass = doc["wifi2_pass"] | "";

    if (validateSSID(w1ssid))
    {
        strlcpy(_wifi1.ssid,     w1ssid, sizeof(_wifi1.ssid));
        strlcpy(_wifi1.password, w1pass, sizeof(_wifi1.password));
        _hasWifi1 = true;
    }

    if (validateSSID(w2ssid))
    {
        strlcpy(_wifi2.ssid,     w2ssid, sizeof(_wifi2.ssid));
        strlcpy(_wifi2.password, w2pass, sizeof(_wifi2.password));
        _hasWifi2 = true;
    }

    // last_ssid is now in /last.json - loaded separately by loadLastSSID()
    return _hasWifi1;
}

// ─────────────────────────────────────────────────────────────
//  saveCredentials()
//  Write /wifi.json to LittleFS
//
//  Fix: Delete old file first - then write fresh
//  Reason: Opening existing file with "w" on LittleFS can
//  leave old bytes at the end if new content is shorter.
//  This corrupts the JSON. Delete first guarantees clean write.
// ─────────────────────────────────────────────────────────────

bool SmartWiFiManager::saveCredentials()
{
    // Step 1: Delete old file if exists
    if (LittleFS.exists(WIFI_CONFIG_FILE))
    {
        LittleFS.remove(WIFI_CONFIG_FILE);
    }

    // Step 2: Write fresh file
    File file = LittleFS.open(WIFI_CONFIG_FILE, "w");
    if (!file)
    {
        WIFI_LOG("[WiFi] Could not open config file for writing");
        return false;
    }

    StaticJsonDocument<1024> doc;
    doc["wifi1_ssid"] = _wifi1.ssid;
    doc["wifi1_pass"] = _wifi1.password;
    doc["wifi2_ssid"] = _wifi2.ssid;
    doc["wifi2_pass"] = _wifi2.password;
    // Note: last_ssid is NOT stored here - it has its own /last.json file

    size_t written = serializeJson(doc, file);
    file.close();

    // Diagnostic log
    _printUptime();
    Serial.print(F("[WiFi] Bytes written: "));
    Serial.println(written);

    // Step 3: Verify file size
    if (!verifyFile(WIFI_CONFIG_FILE))
    {
        // Log actual file size for diagnosis
        if (LittleFS.exists(WIFI_CONFIG_FILE))
        {
            File f = LittleFS.open(WIFI_CONFIG_FILE, "r");
            if (f)
            {
                _printUptime();
                Serial.print(F("[WiFi] File size on disk: "));
                Serial.println(f.size());
                f.close();
            }
        }
        else
        {
            WIFI_LOG("[WiFi] File does not exist after write");
        }
        WIFI_LOG("[WiFi] Write verify failed - removing corrupt file");
        LittleFS.remove(WIFI_CONFIG_FILE);
        return false;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────
//  loadLastSSID()
//  Read /last.json - separate from credentials
//  If corrupt - no problem - credentials still safe
// ─────────────────────────────────────────────────────────────

bool SmartWiFiManager::loadLastSSID()
{
    if (!LittleFS.exists(WIFI_LAST_FILE)) return false;

    File file = LittleFS.open(WIFI_LAST_FILE, "r");
    if (!file) return false;

    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) 
    {
        WIFI_LOG("[WiFi] Last SSID file corrupt - ignored");
        LittleFS.remove(WIFI_LAST_FILE);
        return false;
    }

    const char* last = doc["last_ssid"] | "";
    if (validateSSID(last))
    {
        strlcpy(_lastSSID, last, sizeof(_lastSSID));
        _hasLastSSID = true;
        return true;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────
//  saveLastSSID()
//  Write /last.json - small dedicated file
//  delete-first approach - safe write
//  If this fails or corrupts - /wifi.json stays intact
// ─────────────────────────────────────────────────────────────

bool SmartWiFiManager::saveLastSSID()
{
    // Delete old file first
    if (LittleFS.exists(WIFI_LAST_FILE))
    {
        LittleFS.remove(WIFI_LAST_FILE);
    }

    File file = LittleFS.open(WIFI_LAST_FILE, "w");
    if (!file) return false;

    StaticJsonDocument<128> doc;
    doc["last_ssid"] = _lastSSID;

    size_t written = serializeJson(doc, file);
    file.close();

    return (written > 0);
}

// ─────────────────────────────────────────────────────────────
//  verifyFile()
//  Verify file exists and has content
//
//  Simple size check - not JSON parse
//  Reason: JSON parse needs 1024 bytes RAM on stack
//  During SoftAP + WebServer operation RAM is tight
//  A file with bytes > 0 is sufficient confirmation
// ─────────────────────────────────────────────────────────────

bool SmartWiFiManager::verifyFile(const char* path)
{
    if (!LittleFS.exists(path)) return false;

    File file = LittleFS.open(path, "r");
    if (!file) return false;

    size_t size = file.size();
    file.close();

    return (size > 10);   // must have meaningful content
}

// ─────────────────────────────────────────────────────────────
//  Diagnostics API Implementation
// ─────────────────────────────────────────────────────────────

bool SmartWiFiManager::isWiFiConnected() const
{
    return _wifiConnected;
}

bool SmartWiFiManager::isInternetConnected() const
{
    // Placeholder - always false in v1.0
    // Internet connectivity check will be implemented in future phase
    return _internetConnected;
}

String SmartWiFiManager::getSSID() const
{
    if (!_wifiConnected) return String("");
    return WiFi.SSID();
}

String SmartWiFiManager::getIP() const
{
    if (!_wifiConnected) return String("0.0.0.0");
    return WiFi.localIP().toString();
}

String SmartWiFiManager::getGateway() const
{
    if (!_wifiConnected) return String("0.0.0.0");
    return WiFi.gatewayIP().toString();
}

String SmartWiFiManager::getMAC() const
{
    return WiFi.macAddress();
}

int32_t SmartWiFiManager::getRSSI() const
{
    if (!_wifiConnected) return 0;
    return WiFi.RSSI();
}

String SmartWiFiManager::getUptime() const
{
    if (!_wifiConnected || _wifiConnectedSince == 0)
        return String("00:00:00");

    unsigned long s = (millis() - _wifiConnectedSince) / 1000;
    unsigned long m = s / 60;
    unsigned long h = m / 60;
    s %= 60;
    m %= 60;

    // Format HH:MM:SS
    char buf[9];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, s);
    return String(buf);
}

uint8_t SmartWiFiManager::getReconnectCount() const
{
    return _reconnectCount;
}

bool SmartWiFiManager::isPortalActive() const
{
    return _portalStarted;
}

// ─────────────────────────────────────────────────────────────
//  openPortal()
//  Delete credentials + restart → portal on next boot
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::openPortal()
{
    WIFI_LOG("[WiFi] openPortal() called - clearing credentials...");

    // Mount filesystem if needed
    if (!LittleFS.begin())
    {
        WIFI_LOG("[WiFi] Filesystem mount failed in openPortal()");
    }
    else
    {
        // Delete both credential files
        if (LittleFS.exists(WIFI_CONFIG_FILE))
        {
            LittleFS.remove(WIFI_CONFIG_FILE);
            WIFI_LOG("[WiFi] /wifi.json deleted");
        }

        if (LittleFS.exists(WIFI_LAST_FILE))
        {
            LittleFS.remove(WIFI_LAST_FILE);
            WIFI_LOG("[WiFi] /last.json deleted");
        }
    }

    WIFI_LOG("[WiFi] Restarting - portal will open on boot...");
    delay(200);
    ESP.restart();
}

// ─────────────────────────────────────────────────────────────
//  Callback Setters
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::onConnect(std::function<void()> cb)
{
    _onConnectCb = cb;
}

void SmartWiFiManager::onDisconnect(std::function<void()> cb)
{
    _onDisconnectCb = cb;
}

void SmartWiFiManager::onPortalStart(std::function<void()> cb)
{
    _onPortalStartCb = cb;
}

void SmartWiFiManager::onPortalStop(std::function<void()> cb)
{
    _onPortalStopCb = cb;
}

String SmartWiFiManager::getStateString() const
{
    switch (_currentState)
    {
        case WIFI_UNINITIALISED:   return F("Uninitialised");
        case WIFI_BOOT:            return F("Boot");
        case WIFI_LOAD_SETTINGS:   return F("Loading Settings");
        case WIFI_CONNECT_LAST:    return F("Connecting (Last)");
        case WIFI_CONNECT_PRIMARY: return F("Connecting (Primary)");
        case WIFI_CONNECT_BACKUP:  return F("Connecting (Backup)");
        case WIFI_CONNECTED:       return F("Connected");
        case WIFI_DISCONNECTED:    return F("Disconnected");
        case WIFI_PORTAL:          return F("Portal Active");
        default:                   return F("Unknown");
    }
}

// ─────────────────────────────────────────────────────────────
//  validateSSID()
// ─────────────────────────────────────────────────────────────

bool SmartWiFiManager::validateSSID(const char* ssid)
{
    if (!ssid) return false;
    size_t len = strlen(ssid);
    return (len > 0 && len <= 32);
}

// ─────────────────────────────────────────────────────────────
//  _getDeviceName()
//  Returns DEVICE_NAME-CHIPID  e.g. SmartIoT-FED282
// ─────────────────────────────────────────────────────────────

String SmartWiFiManager::_getDeviceName() const
{
    String name = F(DEVICE_NAME);
    name += "-";
    // Chip ID in uppercase HEX - only the ID part is uppercase
    String chipId = String(ESP.getChipId(), HEX);
    chipId.toUpperCase();
    name += chipId;
    return name;
}

// ─────────────────────────────────────────────────────────────
//  _printBanner()
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::_printBanner() const
{
    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("  SmartIoT Core - WiFi Manager v1.0"));
    Serial.print(F("  Device : "));
    Serial.print(F(DEVICE_NAME));
    Serial.print(F("-"));
    Serial.println(ESP.getChipId(), HEX);
    Serial.print(F("  MAC    : "));
    Serial.println(WiFi.macAddress());
    Serial.println(F("========================================"));
    Serial.println();
}

// ─────────────────────────────────────────────────────────────
//  _printCredentialStatus()
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::_printCredentialStatus() const
{
    char masked[9];

    if (_hasWifi1)
    {
        _maskPassword(_wifi1.password, masked, sizeof(masked));
        _printUptime();
        Serial.print(F("[WiFi] WiFi1 : "));
        Serial.print(_wifi1.ssid);
        Serial.print(F(" / "));
        Serial.println(masked);
    }
    else { WIFI_LOG("[WiFi] WiFi1 : not set"); }

    if (_hasWifi2)
    {
        _maskPassword(_wifi2.password, masked, sizeof(masked));
        _printUptime();
        Serial.print(F("[WiFi] WiFi2 : "));
        Serial.print(_wifi2.ssid);
        Serial.print(F(" / "));
        Serial.println(masked);
    }
    else { WIFI_LOG("[WiFi] WiFi2 : not set"); }

    if (_hasLastSSID)
    {
        _printUptime();
        Serial.print(F("[WiFi] Last  : "));
        Serial.println(_lastSSID);
    }
}

// ─────────────────────────────────────────────────────────────
//  _printUptime()
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::_printUptime() const
{
    unsigned long s = millis() / 1000;
    unsigned long m = s / 60;
    unsigned long h = m / 60;
    s %= 60; m %= 60;

    if (h < 10) Serial.print(F("0"));
    Serial.print(h);
    Serial.print(F(":"));
    if (m < 10) Serial.print(F("0"));
    Serial.print(m);
    Serial.print(F(":"));
    if (s < 10) Serial.print(F("0"));
    Serial.print(s);
    Serial.print(F(" "));
}

// ─────────────────────────────────────────────────────────────
//  _maskPassword()
// ─────────────────────────────────────────────────────────────

void SmartWiFiManager::_maskPassword(const char* password,
                                     char* masked, size_t maskedLen) const
{
    (void)password;
    strlcpy(masked, "********", maskedLen);
}
