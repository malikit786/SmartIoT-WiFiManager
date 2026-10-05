/**
 * ============================================================
 *  SmartIoT Core -- WiFi Manager
 *  File    : PortalPages.cpp
 *  Version : 1.0.0
 *  Target  : ESP8266 / Arduino Framework
 * ============================================================
 *
 *  Contains ALL HTML for the captive portal.
 *  No WiFi logic. No state machine. Pure presentation.
 *
 *  PROGMEM Strategy:
 *    Static HTML sections stored in PROGMEM (flash).
 *    Dynamic values inserted at runtime via String concatenation.
 *    This keeps RAM free during normal operation.
 *    HTML is only loaded into RAM when a page is requested.
 *
 * ============================================================
 */

#include "PortalPages.h"
#include <ESP8266WiFi.h>

// ─────────────────────────────────────────────────────────────
//  PROGMEM HTML Sections
//  Each section is a static chunk stored in flash.
//  FPSTR() converts them to String at runtime.
//  Split into logical sections so dynamic values
//  can be inserted between them.
// ─────────────────────────────────────────────────────────────

// ── Portal Page: Head + CSS ──────────────────────────────────

static const char _HTML_HEAD[] PROGMEM =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>SmartIoT Setup</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;"
         "margin:0;padding:10px;}"
    ".card{background:#16213e;border-radius:10px;padding:20px;"
          "max-width:400px;margin:10px auto;"
          "box-shadow:0 4px 15px rgba(0,0,0,0.3);}"
    "h1{color:#0f9b8e;text-align:center;margin:0 0 5px 0;font-size:22px;}"
    "h2{color:#0f9b8e;font-size:16px;margin:15px 0 10px 0;"
       "border-bottom:1px solid #0f9b8e;padding-bottom:5px;}"
    ".device{text-align:center;color:#aaa;font-size:13px;margin-bottom:15px;}"
    "label{display:block;font-size:13px;color:#aaa;margin:10px 0 4px 0;}"
    "input[type=text]{"
        "width:100%;padding:10px;border:1px solid #0f9b8e;"
        "border-radius:6px;background:#0d1b2a !important;"
        "color:#eee !important;-webkit-text-fill-color:#eee !important;"
        "font-size:14px;box-sizing:border-box;}"
    "input:-webkit-autofill,"
    "input:-webkit-autofill:hover,"
    "input:-webkit-autofill:focus,"
    "input:-webkit-autofill:active{"
        "-webkit-box-shadow:0 0 0 30px #0d1b2a inset !important;"
        "-webkit-text-fill-color:#eee !important;}"
    ".pw-wrap{position:relative;display:flex;align-items:stretch;}"
    ".pw-wrap input{flex:1;min-width:0;padding:10px 44px 10px 10px;"
                   "border:1px solid #0f9b8e;border-radius:6px;"
                   "background:#0d1b2a !important;color:#eee !important;"
                   "-webkit-text-fill-color:#eee !important;"
                   "font-size:14px;box-sizing:border-box;}"
    ".pw-wrap input:focus{"
        "outline:none;"
        "-webkit-box-shadow:0 0 0 30px #0d1b2a inset !important;"
        "-webkit-text-fill-color:#eee !important;}"
    ".eye-btn{"
        "position:absolute;right:8px;top:0;bottom:0;margin:auto 0;"
        "background:none;border:none;color:#0f9b8e;cursor:pointer;"
        "font-size:16px;padding:0;width:28px;height:28px;"
        "display:flex;align-items:center;justify-content:center;}"
    ".eye-btn:hover{color:#fff;}"
    "button{"
        "width:100%;padding:12px;background:#0f9b8e;color:#fff;"
        "border:none;border-radius:6px;font-size:16px;"
        "cursor:pointer;margin-top:15px;}"
    "button:hover{background:#0d8a7e;}"
    ".scan-btn{"
        "background:#1a1a2e;border:1px solid #0f9b8e;"
        "color:#0f9b8e;padding:8px;border-radius:6px;"
        "cursor:pointer;font-size:13px;width:100%;margin-top:8px;}"
    ".net-row{"
        "padding:10px;border-bottom:1px solid #2a2a4e;"
        "cursor:pointer;display:flex;justify-content:space-between;"
        "align-items:center;border-radius:6px;}"
    ".net-row:hover{background:#0d1b2a;}"
    ".net-left{display:flex;align-items:center;gap:8px;}"
    ".net-ssid{font-size:14px;}"
    ".net-rssi{font-size:11px;color:#aaa;}"
    ".lock{font-size:12px;color:#aaa;}"
    ".bars{display:flex;align-items:flex-end;gap:2px;height:16px;}"
    ".bar{width:4px;background:#2a2a4e;border-radius:1px;}"
    ".bar.on{background:#0f9b8e;}"
    ".b1{height:4px;}.b2{height:7px;}.b3{height:11px;}.b4{height:15px;}"
    "#scan-list{"
        "max-height:200px;overflow-y:auto;margin-top:8px;"
        "border:1px solid #2a2a4e;border-radius:6px;}"
    "#scan-status{font-size:13px;color:#aaa;text-align:center;padding:10px;}"
    "</style></head><body>"
    "<div class='card'>"
    "<h1>SmartIoT Setup</h1>"
    "<div class='device'>Device: ";

// ── Portal Page: Network Scanner Section ─────────────────────

static const char _HTML_SCANNER[] PROGMEM =
    "</div>"
    "<h2>Available Networks</h2>"
    "<div id='scan-list'>"
    "<div id='scan-status'>Scanning...</div>"
    "</div>"
    "<button class='scan-btn' onclick='startScan()'>&#8635; Scan Again</button>";

// ── Portal Page: WiFi1 Fields ────────────────────────────────

static const char _HTML_WIFI1[] PROGMEM =
    "<h2>WiFi 1 - Primary</h2>"
    "<label>SSID</label>"
    "<input type='text' id='w1ssid' name='wifi1_ssid' "
           "placeholder='Network name' autocomplete='off'><br>"
    "<label>Password</label>"
    "<div class='pw-wrap'>"
    "<input type='password' id='w1pass' name='wifi1_pass' "
           "placeholder='Password' autocomplete='new-password'>"
    "<button class='eye-btn' type='button' id='eye1' "
            "onclick='togglePw(\"w1pass\",\"eye1\")'>&#128065;</button>"
    "</div>";

// ── Portal Page: WiFi2 Fields ────────────────────────────────

static const char _HTML_WIFI2[] PROGMEM =
    "<h2>WiFi 2 - Backup</h2>"
    "<label>SSID</label>"
    "<input type='text' id='w2ssid' name='wifi2_ssid' "
           "placeholder='Network name (optional)' "
           "autocomplete='off' autocorrect='off' spellcheck='false'><br>"
    "<label>Password</label>"
    "<div class='pw-wrap'>"
    "<input type='password' id='w2pass' name='wifi2_pass' "
           "placeholder='Password' autocomplete='new-password' "
           "autocorrect='off' autocapitalize='off'>"
    "<button class='eye-btn' type='button' id='eye2' "
            "onclick='togglePw(\"w2pass\",\"eye2\")'>&#128065;</button>"
    "</div>"
    "<button onclick='saveConfig()'>Save &amp; Connect</button>"
    "</div>";

// ── Portal Page: JavaScript ──────────────────────────────────

static const char _HTML_JS[] PROGMEM =
    "<script>"

    // Fix autofill background color
    "function fixStyle(f){"
        "f.style.setProperty('background-color','#0d1b2a','important');"
        "f.style.setProperty('-webkit-text-fill-color','#eee','important');"
        "f.style.setProperty('color','#eee','important');"
        "f.style.setProperty('box-shadow','0 0 0 30px #0d1b2a inset','important');"
        "f.style.setProperty('-webkit-box-shadow','0 0 0 30px #0d1b2a inset','important');"
    "}"

    // Toggle password show/hide
    "function togglePw(id,btnId){"
        "var f=document.getElementById(id);"
        "var b=document.getElementById(btnId);"
        "var val=f.value;"
        "if(f.type==='password'){"
            "f.type='text';"
            "b.innerHTML='&#128064;';"
            "b.style.color='#fff';"
        "}else{"
            "f.type='password';"
            "b.innerHTML='&#128065;';"
            "b.style.color='#0f9b8e';"
        "}"
        "f.value=val;"
        "setTimeout(function(){fixStyle(f);},10);"
        "setTimeout(function(){fixStyle(f);},100);"
    "}"

    // Track last focused SSID field
    "var lastFocused='w1ssid';"
    "document.addEventListener('DOMContentLoaded',function(){"
        "document.getElementById('w1ssid')"
            ".addEventListener('focus',function(){lastFocused='w1ssid';});"
        "document.getElementById('w2ssid')"
            ".addEventListener('focus',function(){lastFocused='w2ssid';});"

        // Fix autofill on all inputs
        "var inputs=document.querySelectorAll('input');"
        "for(var i=0;i<inputs.length;i++){"
            "inputs[i].addEventListener('focus',function(){fixStyle(this);});"
            "inputs[i].addEventListener('input',function(){fixStyle(this);});"
            "inputs[i].addEventListener('animationstart',function(){"
                "fixStyle(this);"
            "});"
        "}"
        "setTimeout(function(){"
            "var ins=document.querySelectorAll('input');"
            "for(var i=0;i<ins.length;i++) fixStyle(ins[i]);"
        "},200);"

        // Auto scan on page load
        "startScan();"
    "});"

    // Smart network selection
    "function selectNet(ssid){"
        "var w1=document.getElementById('w1ssid');"
        "var w2=document.getElementById('w2ssid');"
        "if(lastFocused==='w2ssid'){"
            "w2.value=ssid;"
            "document.getElementById('w2pass').focus();"
        "}else if(w1.value.trim()===''){"
            "w1.value=ssid;"
            "document.getElementById('w1pass').focus();"
        "}else{"
            "w2.value=ssid;"
            "document.getElementById('w2pass').focus();"
        "}"
    "}"

    // Save credentials via XHR POST
    "function saveConfig(){"
        "var w1=document.getElementById('w1ssid').value.trim();"
        "if(!w1){alert('WiFi 1 SSID cannot be empty');return;}"
        "var data='wifi1_ssid='+encodeURIComponent(w1)"
            "+'&wifi1_pass='+encodeURIComponent(document.getElementById('w1pass').value)"
            "+'&wifi2_ssid='+encodeURIComponent(document.getElementById('w2ssid').value)"
            "+'&wifi2_pass='+encodeURIComponent(document.getElementById('w2pass').value);"
        "var x=new XMLHttpRequest();"
        "x.open('POST','/save',true);"
        "x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');"
        "x.onload=function(){document.body.innerHTML=x.responseText;};"
        "x.send(data);"
    "}"

    // Network scan polling
    "function startScan(){"
        "document.getElementById('scan-list').innerHTML="
            "'<div id=\"scan-status\">Scanning...</div>';"
        "fetch('/scan').then(r=>r.text()).then(updateScan);"
    "}"
    "function updateScan(data){"
        "if(data==='scanning'){"
            "setTimeout(function(){"
                "fetch('/scan').then(r=>r.text()).then(updateScan);"
            "},1000);"
            "return;"
        "}"
        "if(data==='none'){"
            "document.getElementById('scan-list').innerHTML="
                "'<div id=\"scan-status\">No networks found</div>';"
            "return;"
        "}"
        "document.getElementById('scan-list').innerHTML=data;"
    "}"

    "</script></body></html>";

// ── Success Page ─────────────────────────────────────────────

static const char _HTML_SUCCESS_HEAD[] PROGMEM =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Saved</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;"
         "margin:0;padding:20px;text-align:center;}"
    ".card{background:#16213e;border-radius:10px;padding:30px;"
          "max-width:400px;margin:20px auto;}"
    "h2{color:#0f9b8e;}"
    ".tick{font-size:60px;color:#0f9b8e;}"
    ".ssid{color:#fff;font-size:18px;margin:15px 0;}"
    ".msg{color:#aaa;font-size:14px;}"
    "#count{color:#0f9b8e;font-size:24px;font-weight:bold;}"
    "</style></head><body>"
    "<div class='card'>"
    "<div class='tick'>&#10003;</div>"
    "<h2>Credentials Saved Successfully</h2>"
    "<div class='ssid'>WiFi: ";

static const char _HTML_SUCCESS_TAIL[] PROGMEM =
    "</div>"
    "<div class='msg'>Device will restart in</div>"
    "<div id='count'>5</div>"
    "<div class='msg'>seconds</div>"
    "</div>"
    "<script>"
    "var c=5;"
    "var t=setInterval(function(){"
        "c--;document.getElementById('count').innerHTML=c;"
        "if(c<=0)clearInterval(t);"
    "},1000);"
    "</script></body></html>";

// ─────────────────────────────────────────────────────────────
//  buildPortalPage()
// ─────────────────────────────────────────────────────────────

String PortalPages::buildPortalPage(const String& deviceName,
                                    const String& scanResults)
{
    String html;
    html.reserve(4096);   // pre-allocate to avoid fragmentation

    html  = FPSTR(_HTML_HEAD);
    html += deviceName;
    html += FPSTR(_HTML_SCANNER);
    html += FPSTR(_HTML_WIFI1);
    html += FPSTR(_HTML_WIFI2);
    html += FPSTR(_HTML_JS);

    return html;
}

// ─────────────────────────────────────────────────────────────
//  buildScanResults()
//  Returns HTML rows for found networks — strongest first
// ─────────────────────────────────────────────────────────────

String PortalPages::buildScanResults(int n)
{
    if (n <= 0)
        return F("<div id='scan-status'>No networks found</div>");

    String html;
    html.reserve(512);

    // Sort by RSSI — strongest first using selection sort
    bool used[20] = {false};
    int  count    = (n > 20) ? 20 : n;

    for (int i = 0; i < count; i++)
    {
        int     best     = -1;
        int32_t bestRSSI = -999;

        for (int j = 0; j < count; j++)
        {
            if (!used[j] && WiFi.RSSI(j) > bestRSSI)
            {
                bestRSSI = WiFi.RSSI(j);
                best     = j;
            }
        }

        if (best < 0) break;
        used[best] = true;

        String  ssid    = WiFi.SSID(best);
        int32_t rssi    = WiFi.RSSI(best);
        bool    secured = (WiFi.encryptionType(best) != ENC_TYPE_NONE);

        // Signal bars — CSS based
        int bars = 1;
        if      (rssi >= -60) bars = 4;
        else if (rssi >= -70) bars = 3;
        else if (rssi >= -80) bars = 2;

        // Build bar HTML
        String barsHtml = F("<div class='bars'>");
        for (int b = 1; b <= 4; b++)
        {
            barsHtml += F("<div class='bar b");
            barsHtml += String(b);
            if (b <= bars) barsHtml += F(" on");
            barsHtml += F("'></div>");
        }
        barsHtml += F("</div>");

        // Lock icon
        String lockHtml = secured
            ? F("<span class='lock'>&#128274;</span>")
            : F("");

        // Row
        html += F("<div class='net-row' onclick='selectNet(\"");
        html += ssid;
        html += F("\")'><div class='net-left'>");
        html += barsHtml;
        html += lockHtml;
        html += F("<span class='net-ssid'>");
        html += ssid;
        html += F("</span></div>");
        html += F("<span class='net-rssi'>");
        html += String(rssi);
        html += F(" dBm</span></div>");
    }

    return html;
}

// ─────────────────────────────────────────────────────────────
//  buildSuccessPage()
// ─────────────────────────────────────────────────────────────

String PortalPages::buildSuccessPage(const String& ssid,
                                     const String& ip)
{
    (void)ip;   // reserved for future use

    String html;
    html.reserve(512);

    html  = FPSTR(_HTML_SUCCESS_HEAD);
    html += ssid;
    html += FPSTR(_HTML_SUCCESS_TAIL);

    return html;
}
