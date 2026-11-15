// Firmware update over the web UI.
//
// Accepts both release files: the plain app image ("-ota.bin") and the merged
// image the web flasher uses ("-factory.bin"). For the merged image only the
// app part is written, bootloader and partition table stay untouched.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ota {

// Starts a new upload and drops an unfinished one.
void begin();

// Feeds the next bytes of the uploaded file. Returns false after an error.
bool write(const uint8_t* data, size_t length);

// Finishes the upload. Returns true if the new firmware is valid and will be
// started on the next boot.
bool finish();

// Text of the last error, empty if there was none.
const char* error();

}  // namespace ota
