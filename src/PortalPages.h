/**
 * ============================================================
 *  SmartIoT Core -- WiFi Manager
 *  File    : PortalPages.h
 *  Version : 1.0.0
 *  Target  : ESP8266 / Arduino Framework
 * ============================================================
 *
 *  Purpose:
 *    HTML presentation layer for the captive portal.
 *    Completely separated from WiFi business logic.
 *
 *    WiFiManager.cpp never contains raw HTML.
 *    All HTML lives here.
 *
 *  Usage:
 *    #include "PortalPages.h"
 *    server.send(200, "text/html",
 *        PortalPages::buildPortalPage(deviceName, scanResults));
 *
 * ============================================================
 */

#pragma once

#include <Arduino.h>

namespace PortalPages
{
    /**
     *  buildPortalPage()
     *
     *  Returns the full WiFi configuration HTML page.
     *
     *  @param deviceName  e.g. "SmartIoT-FED282"
     *  @param scanResults HTML rows from buildScanResults()
     *                     Pass empty String if scan not ready yet
     */
    String buildPortalPage(const String& deviceName,
                           const String& scanResults);

    /**
     *  buildScanResults()
     *
     *  Returns HTML rows for the network list section.
     *  Called by /scan route — injected into page via JavaScript.
     *
     *  WiFi scan must be complete before calling.
     *  Caller is responsible for calling WiFi.scanDelete() after.
     *
     *  @param networkCount  Result of WiFi.scanComplete()
     */
    String buildScanResults(int networkCount);

    /**
     *  buildSuccessPage()
     *
     *  Returns the success page shown after credentials are saved.
     *  Includes 5-second countdown and restart message.
     *
     *  @param ssid  The saved WiFi1 SSID
     *  @param ip    Current IP (portal IP before restart)
     */
    String buildSuccessPage(const String& ssid,
                            const String& ip);
}
