#include "auth.h"

#include <esp_random.h>
#include <mbedtls/pkcs5.h>

#include "config.h"
#include "settings.h"

namespace {

constexpr size_t TOKEN_BYTES = 16;
constexpr size_t MAX_SESSIONS = 6;

// Sessions live in RAM only, a reboot logs everybody out.
char sessions[MAX_SESSIONS][TOKEN_BYTES * 2 + 1] = {};
size_t nextSlot = 0;
uint32_t lastAttemptMs = 0;
bool attemptSeen = false;

bool hashPassword(const char* password, const uint8_t* salt, size_t saltLength, uint8_t* out) {
    return mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256, (const unsigned char*)password,
                                         strlen(password), salt, saltLength,
                                         cfg::PASSWORD_PBKDF2_ROUNDS, 32, out) == 0;
}

// Comparison that takes the same time wherever the first difference is.
bool sameBytes(const uint8_t* a, const uint8_t* b, size_t length) {
    uint8_t diff = 0;
    for (size_t i = 0; i < length; i++) diff |= a[i] ^ b[i];
    return diff == 0;
}

void clearSessions() {
    memset(sessions, 0, sizeof(sessions));
}

}  // namespace

namespace auth {

bool enabled() { return settings::get().passwordEnabled; }

void setPassword(const char* password) {
    clearSessions();
    if (password == nullptr || password[0] == 0) {
        settings::update([](Settings& s) {
            s.passwordEnabled = false;
            memset(s.passwordSalt, 0, sizeof(s.passwordSalt));
            memset(s.passwordHash, 0, sizeof(s.passwordHash));
        });
        settings::flush();
        return;
    }
    uint8_t salt[16], hash[32];
    esp_fill_random(salt, sizeof(salt));
    if (!hashPassword(password, salt, sizeof(salt), hash)) return;
    settings::update([&](Settings& s) {
        memcpy(s.passwordSalt, salt, sizeof(salt));
        memcpy(s.passwordHash, hash, sizeof(hash));
        s.passwordEnabled = true;
    });
    settings::flush();
}

bool checkPassword(const char* password, bool& tooFast) {
    const uint32_t now = millis();
    tooFast = attemptSeen && now - lastAttemptMs < cfg::LOGIN_RETRY_PAUSE_MS;
    if (tooFast) return false;
    attemptSeen = true;
    lastAttemptMs = now;

    const Settings s = settings::get();
    if (!s.passwordEnabled || password == nullptr) return false;
    uint8_t hash[32];
    if (!hashPassword(password, s.passwordSalt, sizeof(s.passwordSalt), hash)) return false;
    return sameBytes(hash, s.passwordHash, sizeof(hash));
}

String createSession() {
    uint8_t raw[TOKEN_BYTES];
    esp_fill_random(raw, sizeof(raw));
    char* token = sessions[nextSlot];
    nextSlot = (nextSlot + 1) % MAX_SESSIONS;
    for (size_t i = 0; i < TOKEN_BYTES; i++) snprintf(token + i * 2, 3, "%02x", raw[i]);
    return token;
}

bool validSession(const String& token) {
    if (token.length() != TOKEN_BYTES * 2) return false;
    bool found = false;
    for (const auto& session : sessions) {
        if (session[0] == 0) continue;
        if (sameBytes((const uint8_t*)session, (const uint8_t*)token.c_str(), TOKEN_BYTES * 2)) {
            found = true;
        }
    }
    return found;
}

void endSession(const String& token) {
    for (auto& session : sessions) {
        if (token.length() == TOKEN_BYTES * 2 && strcmp(session, token.c_str()) == 0) session[0] = 0;
    }
}

}  // namespace auth
