#include "improv_serial.h"

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "net.h"
#include "settings.h"

namespace {

constexpr uint8_t HEADER[6] = {'I', 'M', 'P', 'R', 'O', 'V'};
constexpr uint8_t PROTOCOL_VERSION = 1;

enum PacketType : uint8_t { TYPE_STATE = 1, TYPE_ERROR = 2, TYPE_RPC = 3, TYPE_RESULT = 4 };
enum State : uint8_t { STATE_READY = 2, STATE_PROVISIONING = 3, STATE_PROVISIONED = 4 };
enum Error : uint8_t {
    ERROR_NONE = 0,
    ERROR_INVALID_RPC = 1,
    ERROR_UNKNOWN_COMMAND = 2,
    ERROR_UNABLE_TO_CONNECT = 3,
};
enum Command : uint8_t {
    CMD_WIFI_SETTINGS = 1,
    CMD_GET_STATE = 2,
    CMD_GET_DEVICE_INFO = 3,
    CMD_GET_NETWORKS = 4,
};

uint8_t buffer[200];
size_t length = 0;
bool provisioning = false;

void sendPacket(uint8_t type, const uint8_t* data, uint8_t dataLength) {
    uint8_t packet[9 + 255 + 1];
    memcpy(packet, HEADER, 6);
    packet[6] = PROTOCOL_VERSION;
    packet[7] = type;
    packet[8] = dataLength;
    memcpy(packet + 9, data, dataLength);
    uint8_t checksum = 0;
    for (size_t i = 0; i < 9u + dataLength; i++) checksum += packet[i];
    packet[9 + dataLength] = checksum;
    Serial.write(packet, 10 + dataLength);
    Serial.write('\n');
    Serial.flush();
}

void sendState(State state) {
    const uint8_t data = state;
    sendPacket(TYPE_STATE, &data, 1);
}

void sendError(Error error) {
    const uint8_t data = error;
    sendPacket(TYPE_ERROR, &data, 1);
}

// RPC result: command, total length, then length prefixed strings.
void sendResult(uint8_t command, const String* strings, size_t count) {
    uint8_t data[250];
    size_t pos = 2;
    for (size_t i = 0; i < count; i++) {
        const size_t n = strings[i].length();
        if (pos + 1 + n > sizeof(data)) break;
        data[pos++] = (uint8_t)n;
        memcpy(data + pos, strings[i].c_str(), n);
        pos += n;
    }
    data[0] = command;
    data[1] = (uint8_t)(pos - 2);
    sendPacket(TYPE_RESULT, data, (uint8_t)pos);
}

String deviceUrl() { return "http://" + net::ip(); }

void handleRpc(const uint8_t* data, uint8_t dataLength) {
    if (dataLength < 2 || data[1] != dataLength - 2) {
        sendError(ERROR_INVALID_RPC);
        return;
    }
    const uint8_t command = data[0];
    const uint8_t* args = data + 2;
    const uint8_t argsLength = data[1];

    switch (command) {
    case CMD_WIFI_SETTINGS: {
        // [ssid length][ssid][password length][password]
        if (argsLength < 2) return sendError(ERROR_INVALID_RPC);
        const uint8_t ssidLength = args[0];
        if (ssidLength > 32 || 1u + ssidLength + 1 > argsLength) return sendError(ERROR_INVALID_RPC);
        const uint8_t passLength = args[1 + ssidLength];
        if (passLength > 64 || 2u + ssidLength + passLength > argsLength) return sendError(ERROR_INVALID_RPC);
        char ssid[33] = {}, pass[65] = {};
        memcpy(ssid, args + 1, ssidLength);
        memcpy(pass, args + 2 + ssidLength, passLength);
        sendError(ERROR_NONE);
        sendState(STATE_PROVISIONING);
        provisioning = true;
        net::connectWith(ssid, pass);
        break;
    }
    case CMD_GET_STATE:
        if (net::connected()) {
            sendState(STATE_PROVISIONED);
            const String url = deviceUrl();
            sendResult(command, &url, 1);
        } else {
            sendState(STATE_READY);
        }
        break;
    case CMD_GET_DEVICE_INFO: {
        const Settings s = settings::get();
        const String info[4] = {cfg::FIRMWARE_NAME, cfg::FIRMWARE_VERSION, "ESP32-C6", s.hostname};
        sendResult(command, info, 4);
        break;
    }
    case CMD_GET_NETWORKS: {
        // Blocking scan: fine here, the animation runs in its own task.
        if (WiFi.getMode() == WIFI_MODE_NULL) WiFi.mode(WIFI_STA);
        const int16_t n = WiFi.scanNetworks();
        for (int i = 0; i < n; i++) {
            const String entry[3] = {WiFi.SSID(i), String(WiFi.RSSI(i)),
                                     WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "YES" : "NO"};
            if (entry[0].isEmpty()) continue;
            sendResult(command, entry, 3);
        }
        WiFi.scanDelete();
        sendResult(command, nullptr, 0);   // empty result marks the end of the list
        break;
    }
    default:
        sendError(ERROR_UNKNOWN_COMMAND);
        break;
    }
}

}  // namespace

namespace improv {

bool handleByte(uint8_t byte) {
    if (length < sizeof(HEADER)) {
        if (byte != HEADER[length]) {
            length = 0;
            return false;
        }
        buffer[length++] = byte;
        return true;
    }

    buffer[length++] = byte;
    if (length == 7 && byte != PROTOCOL_VERSION) {
        length = 0;
        return true;
    }
    if (length < 9) return true;

    const uint8_t dataLength = buffer[8];
    const size_t total = 9u + dataLength + 1;
    if (total > sizeof(buffer)) {
        length = 0;
        return true;
    }
    if (length < total) return true;

    uint8_t checksum = 0;
    for (size_t i = 0; i < total - 1; i++) checksum += buffer[i];
    const bool valid = checksum == buffer[total - 1];
    const uint8_t type = buffer[7];
    length = 0;

    if (!valid) sendError(ERROR_INVALID_RPC);
    else if (type == TYPE_RPC) handleRpc(buffer + 9, dataLength);
    return true;
}

void loop() {
    if (!provisioning) return;
    switch (net::connectResult()) {
    case net::ConnectResult::Ok: {
        provisioning = false;
        sendState(STATE_PROVISIONED);
        const String url = deviceUrl();
        sendResult(CMD_WIFI_SETTINGS, &url, 1);
        break;
    }
    case net::ConnectResult::Failed:
        provisioning = false;
        sendError(ERROR_UNABLE_TO_CONNECT);
        sendState(STATE_READY);
        break;
    default:
        break;
    }
}

}  // namespace improv
