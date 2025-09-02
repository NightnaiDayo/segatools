#include <stdint.h>
#include <stdbool.h>

#pragma pack(push, 1)

enum Y3WebsocketProtocol {
    Y3_WS_PROTO_ERROR = 0,
    Y3_WS_PROTO_CARD_DATA = 2,
    Y3_WS_PROTO_PING = 1,
    Y3_WS_PROTO_GAME_TITLE = 3,
};

struct CardByteData {
    unsigned int Data;
};

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
    INVALID = 0,
    VALID = 1,
    INFERENCE = 2,
    MARKER = 3
};

struct CardInfo {
    float fX; // 0x00|0
    float fY; // 0x04|4
    float fAngle; // 0x08|8
    enum CardType eCardType; // 0x0C|12
    enum CardStatus eCardStatus; // 0x10|16
    unsigned int uID; // 0x14|20
    int nNumChars; // 0x18|24
    struct CardByteData ubChar0; // 0x1C|28
    struct CardByteData ubChar1; // 0x20|32
    struct CardByteData ubChar2; // 0x24|36
    struct CardByteData ubChar3; // 0x28|40
    struct CardByteData ubChar4; // 0x2C|44
    struct CardByteData ubChar5; // 0x30|48
};

#pragma pack(pop)

struct y3_config {
    bool enable;
    uint8_t ws_enable;
    uint16_t ws_port;
    uint32_t ws_timeout;

    float dll_version;
    float firm_version;
    char firm_name_field[5];
    char firm_name_printer[5];
    char target_code_field[5];
    char target_code_printer[5];
};

void y3_hook_init(const struct y3_config *cfg, HINSTANCE self);

void y3_insert_hooks(HMODULE target);
