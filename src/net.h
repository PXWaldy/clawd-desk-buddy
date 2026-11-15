// Network: WiFi station, setup access point with captive portal, mDNS, NTP.
//
// All WiFi work happens in net::loop() (main loop task). Other tasks only
// leave requests, which keeps the WiFi stack calls in one place.
#pragma once

#include <Arduino.h>

namespace net {

enum class ConnectResult : uint8_t { None, Trying, Ok, Failed };

struct ScanEntry {
    char ssid[33];
    int8_t rssi;
    bool secure;
};

void begin();
void loop(uint32_t nowMs);

// --- requests, safe to call from any task ---------------------------------
void requestSetup();                                   // open the setup AP
void connectWith(const char* ssid, const char* pass);  // try new credentials
void forgetWifi();                                     // erase credentials, open setup
void hostnameChanged();                                // re-announce mDNS
void requestScan();

// --- state -----------------------------------------------------------------
bool connected();
bool setupActive();          // setup AP is open
ConnectResult connectResult();   // outcome of the last connectWith()
String ip();
String ssid();
int rssi();
bool scanning();
size_t scanResults(ScanEntry* out, size_t max);

}  // namespace net
