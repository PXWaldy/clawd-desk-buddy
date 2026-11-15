// Optional password protection for the web UI and API.
//
// The password itself is never stored: only a random salt and a PBKDF2 hash.
// After a successful login the browser gets a random session token as cookie.
#pragma once

#include <Arduino.h>

namespace auth {

bool enabled();

// Sets a new password. An empty password switches the protection off.
void setPassword(const char* password);

// Checks a password. Attempts are rate limited to slow down guessing; in that
// case the function returns false and sets tooFast.
bool checkPassword(const char* password, bool& tooFast);

// Creates a session and returns its token.
String createSession();
bool validSession(const String& token);
void endSession(const String& token);

}  // namespace auth
