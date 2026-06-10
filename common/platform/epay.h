#pragma once

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

#pragma pack(push,1)
struct epay_config {
    bool enable;
    bool hook;
};

/* The functions in these structs are how clients like amdaemon interface
 * with thinca. We can simply replace these functions with our own stubs
 * to bypass errors and such. Currently this DOES NOT allow for epay, and
 * trying to do so will most likely just lead to misery. My goal isn't to
 * reimplement epay, just to give amdaemon SOMETHING so we can boot properly.
 */ 
struct thinca_impl {
    uint64_t* unk0;
    void (*unk8)(struct thinca_impl *);
    uint64_t (*initialize)(struct thinca_impl *, uint64_t);
    uint64_t (*dispose)(struct thinca_impl *);
    uint64_t (*setResource)(struct thinca_impl *, char *);
    uint64_t (*setThincaPaymentLog)(struct thinca_impl *, uint64_t, char *, uint64_t, const char *);
    uint64_t (*setIcasClientLog)(struct thinca_impl *, uint64_t, char *);
    uint64_t (*setIcasClientConfig)(struct thinca_impl *, char *, uint64_t);
    uint64_t* unk40;
    uint64_t* unk48;
    uint64_t (*setClientCertificate)(struct thinca_impl *, char *, uint64_t);
    uint64_t (*setTerminalSerial)(struct thinca_impl *, char *);
    uint64_t (*setGoodsCode)(struct thinca_impl *, char *);
    uint64_t unk68;
    uint64_t (*setThincaEventInterface)(struct thinca_impl *, void*); // probably a struct
    uint64_t unkGap78[7];
    uint64_t (*checkDeal)(struct thinca_impl *, void *); // probably a struct
    uint64_t unkGapB8[41];
    uint64_t (*cancelRequest)(struct thinca_impl *);
    uint64_t (*selectButton)(struct thinca_impl *);
    uint64_t unkGap210[2];
    uint64_t (*unk220)(struct thinca_impl *, uint64_t);
    uint64_t (*unk228)(struct thinca_impl *, uint64_t);
};

/* I believe the actual struct is 0x310 bytes, so for now I'm just
 * implementing what I need and hoping the rest don't cause issues 
 * later. AMDaemon seems to only care about impl1 and deal_thing,
 * at least from what I can tell
 */ 
struct thinca_main { 
    struct thinca_impl* impl1;
    struct thinca_impl* impl2;
    HANDLE* mutex1;
    HANDLE* mutex2;
    HANDLE* mutex3;
    uint64_t* unk28;
    uint64_t* unk30;
    uint64_t* unk38;
    uint64_t* unk40;
    uint64_t* deal_thing;
    uint64_t filler[88];
};

#pragma pack(pop)
HRESULT epay_hook_init(const struct epay_config *cfg);
