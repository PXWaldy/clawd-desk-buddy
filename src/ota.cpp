#include "ota.h"

#include <Arduino.h>
#include <Update.h>

namespace {

constexpr size_t HEADER_BYTES = 36;            // enough to identify an app image
constexpr size_t MERGED_APP_OFFSET = 0x10000;  // where the app sits in the merged image
constexpr uint8_t IMAGE_MAGIC = 0xE9;
constexpr uint16_t CHIP_ID_ESP32C6 = 0x000D;
constexpr uint32_t APP_DESCRIPTOR_MAGIC = 0xABCD5432;

enum class Stage : uint8_t {
    Detect,   // reading the first bytes to see which kind of file this is
    Skip,     // merged image: skipping bootloader and partition table
    Header,   // merged image: reading the header of the app part
    Write,    // writing the app to flash
    Failed,
};

Stage stage = Stage::Failed;
size_t position = 0;                 // bytes of the upload seen so far
uint8_t header[HEADER_BYTES];
size_t headerLength = 0;
const char* errorText = "";

void fail(const char* text) {
    if (stage == Stage::Write) Update.abort();
    stage = Stage::Failed;
    errorText = text;
}

bool isImage(const uint8_t* h) { return h[0] == IMAGE_MAGIC; }

bool isAppForThisChip(const uint8_t* h) {
    const uint16_t chip = h[12] | (h[13] << 8);
    const uint32_t magic = h[32] | (h[33] << 8) | (h[34] << 16) | ((uint32_t)h[35] << 24);
    return isImage(h) && chip == CHIP_ID_ESP32C6 && magic == APP_DESCRIPTOR_MAGIC;
}

// Starts the flash update with the app header that was collected.
void startWriting() {
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        fail("Not enough space for the update.");
        return;
    }
    if (Update.write(header, HEADER_BYTES) != HEADER_BYTES) {
        fail("Writing to flash failed.");
        return;
    }
    stage = Stage::Write;
}

}  // namespace

namespace ota {

void begin() {
    if (stage == Stage::Write) Update.abort();
    stage = Stage::Detect;
    position = 0;
    headerLength = 0;
    errorText = "";
}

bool write(const uint8_t* data, size_t length) {
    while (length > 0 && stage != Stage::Failed) {
        size_t used = 0;
        switch (stage) {
        case Stage::Detect:
        case Stage::Header:
            used = min(length, HEADER_BYTES - headerLength);
            memcpy(header + headerLength, data, used);
            headerLength += used;
            if (headerLength < HEADER_BYTES) break;
            if (isAppForThisChip(header)) {
                startWriting();
            } else if (stage == Stage::Detect && isImage(header)) {
                stage = Stage::Skip;     // looks like the merged image
                headerLength = 0;
            } else {
                fail("This file is not a Clawd Desk Buddy firmware.");
            }
            break;
        case Stage::Skip:
            used = min(length, MERGED_APP_OFFSET - position);
            if (position + used == MERGED_APP_OFFSET) stage = Stage::Header;
            break;
        case Stage::Write:
            used = length;
            if (Update.write(const_cast<uint8_t*>(data), used) != used) {
                fail("Writing to flash failed.");
            }
            break;
        case Stage::Failed:
            break;
        }
        data += used;
        length -= used;
        position += used;
    }
    return stage != Stage::Failed;
}

bool finish() {
    if (stage == Stage::Failed) return false;
    if (stage != Stage::Write) {
        fail("This file is not a Clawd Desk Buddy firmware.");
        return false;
    }
    stage = Stage::Failed;   // done, a new upload has to call begin()
    if (!Update.end(true)) {
        errorText = "The firmware file is damaged.";
        return false;
    }
    return true;
}

const char* error() { return errorText; }

}  // namespace ota
