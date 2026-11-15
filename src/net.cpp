#include "net.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "config.h"
#include "hint.h"
#include "settings.h"

namespace {

enum class StaState : uint8_t { Off, Connecting, Connected };

StaState staState = StaState::Off;
uint32_t stateSinceMs = 0;
uint8_t attempt = 0;
uint8_t maxAttempts = cfg::WIFI_CONNECT_ATTEMPTS;
uint32_t lostSinceMs = 0;

// Credentials of the running connection attempt
char activeSsid[33] = "";
char activePass[65] = "";
bool activeIsNew = false;        // not saved yet, save them on success

// Setup access point
bool apActive = false;
uint32_t apIdleSinceMs = 0;
uint32_t apCloseAtMs = 0;        // 0 = no scheduled close
DNSServer dns;

// Requests from other tasks
SemaphoreHandle_t lock = nullptr;
volatile bool wantSetup = false;
volatile bool wantConnect = false;
volatile bool wantForget = false;
volatile bool wantMdns = false;
volatile bool wantScan = false;
char pendingSsid[33] = "";
char pendingPass[65] = "";
volatile net::ConnectResult result = net::ConnectResult::None;

// Scan cache
constexpr size_t MAX_SCAN = 20;
net::ScanEntry scanCache[MAX_SCAN];
size_t scanCount = 0;
volatile bool scanRunning = false;

void startMdns() {
    const Settings s = settings::get();
    MDNS.end();
    if (MDNS.begin(s.hostname)) {
        MDNS.addService("http", "tcp", 80);
    }
}

void startAp(uint32_t now) {
    if (!apActive) {
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(cfg::SETUP_AP_SSID);     // open network, only while setup runs
        dns.setErrorReplyCode(DNSReplyCode::NoError);
        dns.start(53, "*", WiFi.softAPIP());  // captive portal: every name leads here
        apActive = true;
        Serial.printf("Setup AP open: %s (%s)\n", cfg::SETUP_AP_SSID,
                      WiFi.softAPIP().toString().c_str());
    }
    apIdleSinceMs = now;
    apCloseAtMs = 0;
    hint::show("WiFi setup", "Join the WiFi network", cfg::SETUP_AP_SSID, 15000);
}

void stopAp() {
    if (!apActive) return;
    dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apActive = false;
    apCloseAtMs = 0;
    Serial.println("Setup AP closed");
}

void beginAttempt(uint32_t now) {
    // A running attempt must be stopped first, otherwise the WiFi stack
    // refuses the new configuration.
    if (WiFi.status() != WL_CONNECTED && attempt > 1) {
        WiFi.disconnect();
        delay(200);
    }
    WiFi.begin(activeSsid, activePass);
    staState = StaState::Connecting;
    stateSinceMs = now;
}

void startConnect(const char* ssid, const char* pass, bool isNew, uint32_t now) {
    strlcpy(activeSsid, ssid, sizeof(activeSsid));
    strlcpy(activePass, pass, sizeof(activePass));
    activeIsNew = isNew;
    attempt = 1;
    // New credentials get one quick attempt, so the user sees the result soon.
    maxAttempts = isNew ? 1 : cfg::WIFI_CONNECT_ATTEMPTS;
    lostSinceMs = 0;

    const Settings s = settings::get();
    WiFi.mode(apActive ? WIFI_AP_STA : WIFI_STA);
    WiFi.setHostname(s.hostname);
    Serial.printf("WiFi: connecting to \"%s\"\n", ssid);
    beginAttempt(now);
}

void connectSaved(uint32_t now) {
    const Settings s = settings::get();
    if (s.wifiSsid[0] == 0) {
        staState = StaState::Off;
        stateSinceMs = now;
        return;
    }
    startConnect(s.wifiSsid, s.wifiPass, false, now);
}

void onConnected(uint32_t now) {
    staState = StaState::Connected;
    stateSinceMs = now;
    lostSinceMs = 0;

    if (activeIsNew) {
        settings::update([](Settings& s) {
            strlcpy(s.wifiSsid, activeSsid, sizeof(s.wifiSsid));
            strlcpy(s.wifiPass, activePass, sizeof(s.wifiPass));
        });
        settings::flush();
        activeIsNew = false;
    }
    result = net::ConnectResult::Ok;

    startMdns();
    const Settings s = settings::get();
    configTzTime(s.timezonePosix, cfg::NTP_SERVER);

    const String address = WiFi.localIP().toString();
    char name[40];
    snprintf(name, sizeof(name), "%s.local", s.hostname);
    Serial.printf("WiFi: connected, http://%s (http://%s)\n", address.c_str(), name);
    hint::show("Connected", address.c_str(), name, cfg::IP_HINT_MS);

    // Leave the setup AP open a little longer so the portal can show the result.
    if (apActive) apCloseAtMs = now + cfg::SETUP_AP_LINGER_MS;
}

void onConnectFailed(uint32_t now) {
    Serial.printf("WiFi: could not connect to \"%s\"\n", activeSsid);
    WiFi.disconnect();
    staState = StaState::Off;
    stateSinceMs = now;
    if (activeIsNew) {
        result = net::ConnectResult::Failed;
        activeIsNew = false;
        if (!apActive) connectSaved(now);   // go back to the saved network
        else apIdleSinceMs = now;
        return;
    }
    // The saved network is not reachable: fall back to the setup AP.
    if (!apActive) startAp(now);
}

void handleRequests(uint32_t now) {
    if (wantForget) {
        wantForget = false;
        settings::update([](Settings& s) {
            s.wifiSsid[0] = 0;
            s.wifiPass[0] = 0;
        });
        settings::flush();
        WiFi.disconnect(true, true);
        staState = StaState::Off;
        stateSinceMs = now;
        startAp(now);
    }
    if (wantSetup) {
        wantSetup = false;
        startAp(now);
    }
    if (wantConnect) {
        wantConnect = false;
        char ssid[33], pass[65];
        xSemaphoreTake(lock, portMAX_DELAY);
        strlcpy(ssid, pendingSsid, sizeof(ssid));
        strlcpy(pass, pendingPass, sizeof(pass));
        xSemaphoreGive(lock);
        startConnect(ssid, pass, true, now);
    }
    if (wantMdns) {
        wantMdns = false;
        const Settings s = settings::get();
        WiFi.setHostname(s.hostname);
        if (staState == StaState::Connected) startMdns();
    }
    if (wantScan && !scanRunning) {
        wantScan = false;
        if (WiFi.getMode() == WIFI_MODE_NULL) WiFi.mode(WIFI_STA);
        if (WiFi.scanNetworks(true) != WIFI_SCAN_FAILED) scanRunning = true;
    }
}

void collectScan() {
    if (!scanRunning) return;
    const int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    scanCount = 0;
    for (int i = 0; i < n && scanCount < MAX_SCAN; i++) {
        const String name = WiFi.SSID(i);
        if (name.isEmpty()) continue;
        bool duplicate = false;
        for (size_t k = 0; k < scanCount; k++) {
            if (name == scanCache[k].ssid) duplicate = true;
        }
        if (duplicate) continue;   // results are sorted by signal, keep the strongest
        strlcpy(scanCache[scanCount].ssid, name.c_str(), sizeof(scanCache[0].ssid));
        scanCache[scanCount].rssi = (int8_t)WiFi.RSSI(i);
        scanCache[scanCount].secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        scanCount++;
    }
    xSemaphoreGive(lock);
    WiFi.scanDelete();
    scanRunning = false;
}

}  // namespace

namespace net {

void begin() {
    lock = xSemaphoreCreateMutex();
    WiFi.persistent(false);          // credentials live in our own settings
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            Serial.printf("WiFi: disconnected, reason %d\n", info.wifi_sta_disconnected.reason);
        }
    });
    WiFi.setAutoReconnect(true);

    const uint32_t now = millis();
    const Settings s = settings::get();
    if (s.wifiSsid[0] != 0) connectSaved(now);
    else startAp(now);               // first boot: nothing configured yet
}

void loop(uint32_t now) {
    handleRequests(now);
    collectScan();

    if (apActive) {
        dns.processNextRequest();
        if (WiFi.softAPgetStationNum() > 0) apIdleSinceMs = now;
        const bool idleTooLong = now - apIdleSinceMs >= cfg::SETUP_AP_TIMEOUT_MS;
        const bool closeNow = apCloseAtMs != 0 && (int32_t)(now - apCloseAtMs) >= 0;
        if (idleTooLong || closeNow) {
            stopAp();
            if (staState == StaState::Off) connectSaved(now);
        }
    }

    switch (staState) {
    case StaState::Connecting:
        if (WiFi.status() == WL_CONNECTED) {
            onConnected(now);
        } else if (now - stateSinceMs >= cfg::WIFI_CONNECT_TIMEOUT_MS) {
            if (attempt < maxAttempts) {
                attempt++;
                beginAttempt(now);
            } else {
                onConnectFailed(now);
            }
        }
        break;
    case StaState::Connected:
        if (WiFi.status() == WL_CONNECTED) {
            lostSinceMs = 0;
        } else if (lostSinceMs == 0) {
            lostSinceMs = now ? now : 1;
        } else if (now - lostSinceMs >= cfg::WIFI_LOST_GRACE_MS) {
            connectSaved(now);       // the automatic reconnect did not help
        }
        break;
    case StaState::Off:
        // Offline with saved credentials and no setup running: try again later.
        if (!apActive && now - stateSinceMs >= cfg::WIFI_RETRY_PAUSE_MS) connectSaved(now);
        break;
    }
}

void requestSetup() { wantSetup = true; }

void connectWith(const char* ssid, const char* pass) {
    xSemaphoreTake(lock, portMAX_DELAY);
    strlcpy(pendingSsid, ssid, sizeof(pendingSsid));
    strlcpy(pendingPass, pass, sizeof(pendingPass));
    xSemaphoreGive(lock);
    result = ConnectResult::Trying;
    wantConnect = true;
}

void forgetWifi() { wantForget = true; }
void hostnameChanged() { wantMdns = true; }
void requestScan() { wantScan = true; }

bool connected() { return staState == StaState::Connected && WiFi.status() == WL_CONNECTED; }
bool setupActive() { return apActive; }
ConnectResult connectResult() { return result; }
String ip() { return connected() ? WiFi.localIP().toString() : String(); }
String ssid() { return connected() ? WiFi.SSID() : String(); }
int rssi() { return connected() ? WiFi.RSSI() : 0; }
bool scanning() { return scanRunning || wantScan; }

size_t scanResults(ScanEntry* out, size_t max) {
    xSemaphoreTake(lock, portMAX_DELAY);
    const size_t n = scanCount < max ? scanCount : max;
    memcpy(out, scanCache, n * sizeof(ScanEntry));
    xSemaphoreGive(lock);
    return n;
}

}  // namespace net
