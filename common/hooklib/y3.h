#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "hooklib/config.h"

#pragma pack(push, 1)

// Value held on a card.
struct CardByteData {
    unsigned int Data;
};

// Unused
enum CardType {
    TYPE0 = 0,
    TYPE1,
    TYPE2,
    TYPE3,
    TYPE4,
    TYPE5,
    TYPE6,
    TYPE7 = 7
};

enum CardStatus {
    // Unset entry
    INVALID = 0,
    // Valid card
    VALID = 1,
    // Not a card but rather infrared interference. Only relevant in test mode.
    INFERENCE = 2,
    // This is only used by the printer camera.
    MARKER = 3
};

struct CardInfo {
    // X position of the card.
    float fX; // 0x00|0
    // Y position of the card.
    float fY; // 0x04|4
    // Rotation of the card in degrees >=0.0 && <360.0
    float fAngle; // 0x08|8
    // Unused
    enum CardType eCardType; // 0x0C|12
    // see enum CardStatus
    enum CardStatus eCardStatus; // 0x10|16
    // card's BaseCode. used for a reference to the card being tracked as well as part of the IvCode.
    unsigned int uID; // 0x14|20
    // Unused
    int nNumChars; // 0x18|24
    // Title Code. Is 8589934592 for EKT.
    struct CardByteData ubChar0; // 0x1C|28
    // Must be 0x4000 for the printer camera.
    struct CardByteData ubChar1; // 0x20|32
    // Unused
    struct CardByteData ubChar2; // 0x24|36
    // Must be 0x0 for the printer camera.
    struct CardByteData ubChar3; // 0x28|40
    // Unused
    struct CardByteData ubChar4; // 0x2C|44
    // Unused
    struct CardByteData ubChar5; // 0x30|48
};

#pragma pack(pop)

HRESULT y3_hook_init(const struct y3_config *cfg, HINSTANCE self, const wchar_t* config_path);

void y3_insert_hooks(HMODULE target);
