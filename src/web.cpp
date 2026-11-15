#include "web.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include "auth.h"
#include "behavior.h"
#include "config.h"
#include "hint.h"
#include "net.h"
#include "ota.h"
#include "settings.h"
#include "sleep.h"
#include "web_assets.h"

namespace {

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

volatile bool broadcastNeeded = false;
volatile bool reboot = false;
bool updateOk = false;
uint32_t lastBehaviorVersion = 0;
uint32_t lastBroadcastMs = 0;

// --- helpers ---------------------------------------------------------------

String colorToHex(uint32_t rgb) {
    char text[8];
    snprintf(text, sizeof(text), "#%06x", (unsigned)(rgb & 0xFFFFFF));
    return text;
}

bool hexToColor(const char* text, uint32_t& rgb) {
    if (text == nullptr || text[0] != '#' || strlen(text) != 7) return false;
    char* end = nullptr;
    const unsigned long value = strtoul(text + 1, &end, 16);
    if (*end != 0) return false;
    rgb = (uint32_t)value;
    return true;
}

String minutesToClock(uint16_t minutes) {
    char text[6];
    snprintf(text, sizeof(text), "%02u:%02u", (unsigned)(minutes / 60), (unsigned)(minutes % 60));
    return text;
}

// Parses "HH:MM". Returns -1 if the text is not a valid time.
int clockToMinutes(const char* text) {
    if (text == nullptr || strlen(text) != 5 || text[2] != ':') return -1;
    for (int i : {0, 1, 3, 4}) {
        if (text[i] < '0' || text[i] > '9') return -1;
    }
    const int hours = (text[0] - '0') * 10 + (text[1] - '0');
    const int minutes = (text[3] - '0') * 10 + (text[4] - '0');
    if (hours > 23 || minutes > 59) return -1;
    return hours * 60 + minutes;
}

// A POSIX TZ rule like "CET-1CEST,M3.5.0,M10.5.0/3": printable ASCII only.
bool validTimezoneRule(const char* text) {
    const size_t length = text ? strlen(text) : 0;
    if (length < 3 || length > 47) return false;
    for (size_t i = 0; i < length; i++) {
        if (text[i] <= ' ' || text[i] > '~') return false;
    }
    return true;
}

// --- sessions --------------------------------------------------------------

constexpr char COOKIE_NAME[] = "clawd_session";

String sessionToken(AsyncWebServerRequest* request) {
    if (!request->hasHeader("Cookie")) return String();
    const String cookies = request->header("Cookie");
    const String key = String(COOKIE_NAME) + "=";
    int start = cookies.indexOf(key);
    if (start < 0) return String();
    start += key.length();
    int end = cookies.indexOf(';', start);
    if (end < 0) end = cookies.length();
    return cookies.substring(start, end);
}

bool authorized(AsyncWebServerRequest* request) {
    return !auth::enabled() || auth::validSession(sessionToken(request));
}

void sendWithSession(AsyncWebServerRequest* request, const String& token) {
    AsyncWebServerResponse* response =
        request->beginResponse(200, "application/json", "{\"ok\":true}");
    // HttpOnly: scripts cannot read the token. SameSite: other sites cannot use it.
    response->addHeader("Set-Cookie", String(COOKIE_NAME) + "=" + token +
                                          "; Path=/; HttpOnly; SameSite=Strict" +
                                          (token.isEmpty() ? "; Max-Age=0" : ""));
    request->send(response);
}

int expressionFromId(const char* id) {
    if (id == nullptr) return -1;
    for (uint8_t i = 0; i < EXPRESSION_COUNT; i++) {
        if (strcmp(id, expressionId((Expression)i)) == 0) return i;
    }
    return -1;
}

// --- state -----------------------------------------------------------------

String stateJson() {
    const Settings s = settings::get();
    JsonDocument doc;
    doc["name"] = cfg::FIRMWARE_NAME;
    doc["version"] = cfg::FIRMWARE_VERSION;
    doc["display_on"] = s.displayOn;
    doc["brightness"] = s.brightness;
    doc["expression"] = expressionId(behavior::baseExpression());
    doc["random"] = s.randomMode;
    doc["interval"] = s.randomIntervalS;
    doc["interval_min"] = cfg::RANDOM_INTERVAL_MIN_S;
    doc["interval_max"] = cfg::RANDOM_INTERVAL_MAX_S;
    doc["asleep"] = !behavior::screenLit();
    doc["sleep_enabled"] = s.sleepEnabled;
    doc["sleep_from"] = minutesToClock(s.sleepFromMin);
    doc["sleep_to"] = minutesToClock(s.sleepToMin);
    doc["wake_seconds"] = s.wakeSeconds;
    doc["time"] = sleep_schedule::localTime();
    doc["timezone"] = s.timezoneName;
    doc["password_enabled"] = s.passwordEnabled;
    doc["bg"] = colorToHex(s.colorBackground);
    doc["eye"] = colorToHex(s.colorEye);

    JsonArray list = doc["expressions"].to<JsonArray>();
    for (uint8_t i = 0; i < EXPRESSION_COUNT; i++) {
        JsonObject item = list.add<JsonObject>();
        item["id"] = expressionId((Expression)i);
        item["name"] = expressionName((Expression)i);
        item["enabled"] = (s.enabledMask & (1u << i)) != 0;
    }

    JsonObject defaults = doc["defaults"].to<JsonObject>();
    defaults["bg"] = colorToHex(cfg::COLOR_BACKGROUND);
    defaults["eye"] = colorToHex(cfg::COLOR_EYE);
    defaults["claude_orange"] = colorToHex(cfg::COLOR_CLAUDE_ORANGE);

    JsonObject accents = doc["accents"].to<JsonObject>();
    accents["blush"] = colorToHex(cfg::COLOR_BLUSH);
    accents["heart"] = colorToHex(cfg::COLOR_HEART);
    accents["sparkle"] = colorToHex(cfg::COLOR_SPARKLE);
    accents["angry"] = colorToHex(cfg::COLOR_ANGRY);

    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["connected"] = net::connected();
    wifi["setup"] = net::setupActive();
    wifi["ssid"] = net::ssid();
    wifi["ip"] = net::ip();
    wifi["rssi"] = net::rssi();
    wifi["hostname"] = s.hostname;
    wifi["saved_ssid"] = s.wifiSsid;

    String out;
    serializeJson(doc, out);
    return out;
}

// Applies a partial state update. Returns an error text, or nullptr if fine.
const char* applyPatch(JsonVariantConst patch) {
    using behavior::EventType;
    if (!patch.is<JsonObjectConst>()) return "expected a JSON object";

    if (patch["display_on"].is<bool>()) {
        const bool on = patch["display_on"];
        settings::update([on](Settings& s) { s.displayOn = on; });
        behavior::post(EventType::SetDisplayOn, on);
    }
    if (patch["brightness"].is<int>()) {
        const int value = constrain(patch["brightness"].as<int>(), 0, 100);
        settings::update([value](Settings& s) { s.brightness = (uint8_t)value; });
        behavior::post(EventType::SetBrightness, value);
    }
    if (patch["expression"].is<const char*>()) {
        const int index = expressionFromId(patch["expression"]);
        if (index < 0) return "unknown expression";
        if (!(settings::get().enabledMask & (1u << index))) return "expression is disabled";
        settings::update([](Settings& s) { s.randomMode = false; });
        behavior::post(EventType::SetExpression, index);
    }
    if (patch["random"].is<bool>()) {
        const bool on = patch["random"];
        settings::update([on](Settings& s) { s.randomMode = on; });
        behavior::post(EventType::SetRandom, on);
    }
    if (patch["interval"].is<int>()) {
        const int value = constrain(patch["interval"].as<int>(), (int)cfg::RANDOM_INTERVAL_MIN_S,
                                    (int)cfg::RANDOM_INTERVAL_MAX_S);
        settings::update([value](Settings& s) { s.randomIntervalS = (uint16_t)value; });
        behavior::post(EventType::SetRandomInterval, value);
    }
    if (patch["enabled"].is<JsonArrayConst>()) {
        uint16_t mask = 0;
        for (JsonVariantConst id : patch["enabled"].as<JsonArrayConst>()) {
            const int index = expressionFromId(id.as<const char*>());
            if (index >= 0) mask |= 1u << index;
        }
        if (mask == 0) return "at least one expression must stay enabled";
        settings::update([mask](Settings& s) { s.enabledMask = mask; });
        behavior::post(EventType::SetEnabledMask, mask);
    }
    if (patch["sleep_enabled"].is<bool>()) {
        const bool on = patch["sleep_enabled"];
        settings::update([on](Settings& s) { s.sleepEnabled = on; });
    }
    if (patch["sleep_from"].is<const char*>()) {
        const int minutes = clockToMinutes(patch["sleep_from"]);
        if (minutes < 0) return "invalid time";
        settings::update([minutes](Settings& s) { s.sleepFromMin = (uint16_t)minutes; });
    }
    if (patch["sleep_to"].is<const char*>()) {
        const int minutes = clockToMinutes(patch["sleep_to"]);
        if (minutes < 0) return "invalid time";
        settings::update([minutes](Settings& s) { s.sleepToMin = (uint16_t)minutes; });
    }
    if (patch["wake_seconds"].is<int>()) {
        const int value = constrain(patch["wake_seconds"].as<int>(), (int)cfg::WAKE_SECONDS_MIN,
                                    (int)cfg::WAKE_SECONDS_MAX);
        settings::update([value](Settings& s) { s.wakeSeconds = (uint16_t)value; });
        behavior::post(EventType::SetWakeSeconds, value);
    }
    if (patch["timezone"].is<JsonObjectConst>()) {
        const char* name = patch["timezone"]["name"] | "";
        const char* rule = patch["timezone"]["posix"] | "";
        if (strlen(name) < 1 || strlen(name) > 39 || !validTimezoneRule(rule)) {
            return "invalid timezone";
        }
        settings::update([name, rule](Settings& s) {
            strlcpy(s.timezoneName, name, sizeof(s.timezoneName));
            strlcpy(s.timezonePosix, rule, sizeof(s.timezonePosix));
        });
    }
    uint32_t color;
    if (patch["bg"].is<const char*>()) {
        if (!hexToColor(patch["bg"], color)) return "invalid color";
        settings::update([color](Settings& s) { s.colorBackground = color; });
        behavior::post(EventType::SetBackground, (int32_t)color);
    }
    if (patch["eye"].is<const char*>()) {
        if (!hexToColor(patch["eye"], color)) return "invalid color";
        settings::update([color](Settings& s) { s.colorEye = color; });
        behavior::post(EventType::SetEyeColor, (int32_t)color);
    }
    if (patch["hostname"].is<const char*>()) {
        String name = patch["hostname"].as<String>();
        name.toLowerCase();
        name.trim();
        if (!settings::validHostname(name.c_str())) {
            return "hostname may only contain a-z, 0-9 and -";
        }
        settings::update([&name](Settings& s) {
            strlcpy(s.hostname, name.c_str(), sizeof(s.hostname));
        });
        net::hostnameChanged();
    }
    broadcastNeeded = true;
    return nullptr;
}

void sendResult(AsyncWebServerRequest* request, const char* error) {
    if (error == nullptr) {
        request->send(200, "application/json", "{\"ok\":true}");
        return;
    }
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = error;
    String out;
    serializeJson(doc, out);
    request->send(400, "application/json", out);
}

// --- handlers --------------------------------------------------------------

void handleIndex(AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response =
        request->beginResponse(200, "text/html", WEB_INDEX_GZ, WEB_INDEX_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
}

void handleScan(AsyncWebServerRequest* request) {
    if (request->hasParam("start")) net::requestScan();
    net::ScanEntry entries[20];
    const size_t n = net::scanResults(entries, 20);
    JsonDocument doc;
    doc["scanning"] = net::scanning();
    JsonArray list = doc["networks"].to<JsonArray>();
    for (size_t i = 0; i < n; i++) {
        JsonObject item = list.add<JsonObject>();
        item["ssid"] = entries[i].ssid;
        item["rssi"] = entries[i].rssi;
        item["secure"] = entries[i].secure;
    }
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
}

void handleWifiStatus(AsyncWebServerRequest* request) {
    static const char* const names[] = {"none", "trying", "ok", "failed"};
    const Settings s = settings::get();
    JsonDocument doc;
    doc["result"] = names[(uint8_t)net::connectResult()];
    doc["connected"] = net::connected();
    doc["ip"] = net::ip();
    doc["hostname"] = s.hostname;
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
}

void handleNotFound(AsyncWebServerRequest* request) {
    // Captive portal: while the setup AP is open, every unknown address
    // (the connectivity checks of phones and laptops) leads to the web UI.
    if (net::setupActive() && request->client()->localIP() == WiFi.softAPIP()) {
        request->redirect("http://" + WiFi.softAPIP().toString() + "/");
        return;
    }
    request->send(404, "text/plain", "Not found");
}

void handleWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type,
                   void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        client->text(stateJson());
        return;
    }
    if (type != WS_EVT_DATA) return;
    const AwsFrameInfo* info = (const AwsFrameInfo*)arg;
    // Only small, complete text messages are expected.
    if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) return;
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) return;
    applyPatch(doc.as<JsonVariantConst>());
}

}  // namespace

namespace web {

void begin() {
    // Password protection: without a valid session only the page itself and
    // the login endpoints are reachable.
    server.addMiddleware([](AsyncWebServerRequest* request, ArMiddlewareNext next) {
        const String& url = request->url();
        const bool isProtected = url.startsWith("/api/") || url == "/ws";
        const bool isOpen = url == "/api/login" || url == "/api/auth";
        if (isProtected && !isOpen && !authorized(request)) {
            request->send(401, "application/json", "{\"ok\":false,\"error\":\"login required\"}");
            return;
        }
        next();
    });

    ws.onEvent(handleWsEvent);
    server.addHandler(&ws);

    server.on("/api/auth", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["enabled"] = auth::enabled();
        doc["authorized"] = authorized(request);
        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    auto* loginHandler = new AsyncCallbackJsonWebHandler(
        "/api/login", [](AsyncWebServerRequest* request, JsonVariant& json) {
            bool tooFast = false;
            if (auth::checkPassword(json["password"] | "", tooFast)) {
                sendWithSession(request, auth::createSession());
            } else if (tooFast) {
                request->send(429, "application/json",
                              "{\"ok\":false,\"error\":\"Please wait a moment and try again.\"}");
            } else {
                request->send(401, "application/json",
                              "{\"ok\":false,\"error\":\"Wrong password.\"}");
            }
        });
    loginHandler->setMethod(HTTP_POST);
    server.addHandler(loginHandler);

    server.on("/api/logout", HTTP_POST, [](AsyncWebServerRequest* request) {
        auth::endSession(sessionToken(request));
        sendWithSession(request, String());
    });

    // Sets, changes or removes (empty) the password. All other sessions end.
    auto* passwordHandler = new AsyncCallbackJsonWebHandler(
        "/api/password", [](AsyncWebServerRequest* request, JsonVariant& json) {
            const char* password = json["password"] | "";
            const size_t length = strlen(password);
            if (length != 0 && (length < cfg::PASSWORD_MIN_LENGTH || length > 64)) {
                sendResult(request, "The password needs 4 to 64 characters.");
                return;
            }
            auth::setPassword(password);
            ws.closeAll();
            broadcastNeeded = true;
            sendWithSession(request, length ? auth::createSession() : String());
        });
    passwordHandler->setMethod(HTTP_POST);
    server.addHandler(passwordHandler);

    server.on("/", HTTP_GET, handleIndex);
    server.on("/index.html", HTTP_GET, handleIndex);

    server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "application/json", stateJson());
    });

    auto* stateHandler = new AsyncCallbackJsonWebHandler(
        "/api/state", [](AsyncWebServerRequest* request, JsonVariant& json) {
            sendResult(request, applyPatch(json));
        });
    stateHandler->setMethod(HTTP_POST);
    server.addHandler(stateHandler);

    server.on("/api/colors/reset", HTTP_POST, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["bg"] = colorToHex(cfg::COLOR_BACKGROUND);
        doc["eye"] = colorToHex(cfg::COLOR_EYE);
        sendResult(request, applyPatch(doc.as<JsonVariantConst>()));
    });

    server.on("/api/wifi/scan", HTTP_GET, handleScan);
    server.on("/api/wifi/status", HTTP_GET, handleWifiStatus);

    auto* wifiHandler = new AsyncCallbackJsonWebHandler(
        "/api/wifi", [](AsyncWebServerRequest* request, JsonVariant& json) {
            const char* ssid = json["ssid"] | "";
            const char* pass = json["pass"] | "";
            if (strlen(ssid) == 0 || strlen(ssid) > 32 || strlen(pass) > 64) {
                sendResult(request, "invalid WiFi name or password");
                return;
            }
            net::connectWith(ssid, pass);
            sendResult(request, nullptr);
        });
    wifiHandler->setMethod(HTTP_POST);
    server.addHandler(wifiHandler);

    server.on("/api/wifi/reset", HTTP_POST, [](AsyncWebServerRequest* request) {
        sendResult(request, nullptr);
        net::forgetWifi();
    });

    server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* request) {
        sendResult(request, nullptr);
        reboot = true;
    });

    // Firmware update: the file arrives in pieces through the upload callback,
    // the request callback then reports the result.
    server.on(
        "/api/update", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            if (!authorized(request)) return;   // already answered by the middleware
            if (updateOk) {
                sendResult(request, nullptr);
                reboot = true;
            } else {
                sendResult(request, ota::error()[0] ? ota::error() : "No firmware file received.");
                hint::clear();
            }
        },
        [](AsyncWebServerRequest* request, const String&, size_t index, uint8_t* data,
           size_t len, bool final) {
            // The upload callback runs before the request is checked, so the
            // session has to be verified here as well.
            if (!authorized(request)) return;
            if (index == 0) {
                updateOk = false;
                ota::begin();
                hint::show("Updating firmware", "", "Do not unplug", 0);
            }
            ota::write(data, len);
            if (final) {
                updateOk = ota::finish();
                if (updateOk) hint::show("Update done", "", "Restarting...", 0);
            }
        });

    server.onNotFound(handleNotFound);
    server.begin();
}

void loop(uint32_t now) {
    ws.cleanupClients();

    const uint32_t version = behavior::version();
    if (version != lastBehaviorVersion) {
        lastBehaviorVersion = version;
        broadcastNeeded = true;
    }
    // Wait a moment so the animation task has applied pending changes.
    if (broadcastNeeded && now - lastBroadcastMs >= 100) {
        broadcastNeeded = false;
        lastBroadcastMs = now;
        if (ws.count() > 0) ws.textAll(stateJson());
    }
}

bool rebootRequested() { return reboot; }

}  // namespace web
