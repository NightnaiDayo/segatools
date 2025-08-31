enum Y3WebsocketProtocol {
    Y3_WS_PROTO_ERROR = 0,
    Y3_WS_PROTO_CARD_DATA = 2,
    Y3_WS_PROTO_PING = 1,
    Y3_WS_PROTO_GAME_TITLE = 3,
};

struct CardByteData {
    unsigned int Data;
};

struct CardInfo {
    float fX; // 0x00|0
    float fY; // 0x04|4
    float fAngle; // 0x08|8
    int eCardType; // 0x0C|12
    int eCardStatus; // 0x10|16
    unsigned int uID; // 0x14|20
    int nNumChars; // 0x18|24
    struct CardByteData ubChar0; // 0x1C|28
    struct CardByteData ubChar1; // 0x20|32
    struct CardByteData ubChar2; // 0x24|36
    struct CardByteData ubChar3; // 0x28|40
    struct CardByteData ubChar4; // 0x2C|44
    struct CardByteData ubChar5; // 0x30|48
};

enum Type {
    // Token: 0x04000084 RID: 132
    TYPE0 = 0,
    // Token: 0x04000085 RID: 133
    TYPE1,
    // Token: 0x04000086 RID: 134
    TYPE2,
    // Token: 0x04000087 RID: 135
    TYPE3,
    // Token: 0x04000088 RID: 136
    TYPE4,
    // Token: 0x04000089 RID: 137
    TYPE5,
    // Token: 0x0400008A RID: 138
    TYPE6,
    // Token: 0x0400008B RID: 139
    TYPE7 = 7
};

enum Status {
    INVALID = 0,
    // Token: 0x0400008E RID: 142
    VALID = 1,
    // Token: 0x0400008F RID: 143
    INFERENCE = 2,
    // Token: 0x04000090 RID: 144
    MARKER = 3
};

void y3_hook_init(const struct y3_config *cfg, HINSTANCE self);
