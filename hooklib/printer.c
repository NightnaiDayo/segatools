/*
    Sinfonia CHC-C3XX Printer emulator

    Credits:

    c310emu (rakisaionji)
    segatools-kancolle (OLEG)
    c310emu (doremi)
    chc (emihiok)
*/

#include "hooklib/printer.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

#include "hook/table.h"
#include "hook/procaddr.h"
#include "hooklib/dll.h"
#include "hooklib/uart.h"
#include "util/dprintf.h"
#include "util/dump.h"

// #define IMAGE_SIZE 0x24FC00
#define IMAGE_SIZE 0x1F0800
#define HOLO_SIZE 0xC5400

static const int8_t idNumber = 0x01;
static uint64_t serialNo = 0x333231412D4135;

static uint16_t WIDTH = 0;
static uint16_t HEIGHT = 0;

static bool rotate180 = false;
static uint8_t cardRFID[0xC] = {0};
static uint8_t cardDataBuffer[0x20] = {0};
static bool awaitingCardExit = false;
static wchar_t printer_out_path[MAX_PATH];

/* Firmware data */

static uint8_t mainFirmware[0x40] = {0};
static uint8_t dspFirmware[0x40] = {0};
static uint8_t paramFirmware[0x40] = {0};

/* printerInfo variables */

static int32_t PAPERINFO[10];
static int32_t CURVE[3][3];
static uint8_t POLISH[2];
static int32_t MTF[9];

/* Printer status */

static uint8_t STATUS = 0;

/* C3XXFWDLusb API hooks */

int WINAPI fwdlusb_open(uint16_t *rResult);
void WINAPI fwdlusb_close();
int WINAPI fwdlusb_listupPrinter(uint8_t *rIdArray);
int WINAPI fwdlusb_listupPrinterSN(uint64_t *rSerialArray);
int WINAPI fwdlusb_selectPrinter(uint8_t printerId, uint16_t *rResult);
int WINAPI fwdlusb_selectPrinterSN(uint64_t printerSN, uint16_t *rResult);
int WINAPI fwdlusb_getPrinterInfo(uint16_t tagNumber, uint8_t *rBuffer, uint32_t *rLen);
int WINAPI fwdlusb_status(uint16_t *rResult);
int WINAPI fwdlusb_statusAll(uint8_t *idArray, uint16_t *rResultArray);
int WINAPI fwdlusb_resetPrinter(uint16_t *rResult);
int WINAPI fwdlusb_updateFirmware(uint8_t update, LPCSTR filename, uint16_t *rResult);
int WINAPI fwdlusb_getFirmwareInfo(uint8_t update, LPCSTR filename, uint8_t *rBuffer, uint32_t *rLen, uint16_t *rResult);
int WINAPI fwdlusb_MakeThread(uint16_t maxCount);
int WINAPI fwdlusb_ReleaseThread(uint16_t *rResult);
int WINAPI fwdlusb_AttachThreadCount(uint16_t *rCount, uint16_t *rMaxCount);
int WINAPI fwdlusb_getErrorLog(uint16_t index, uint8_t *rData, uint16_t *rResult);

/* C3XXusb API hooks */

int WINAPI chcusb_MakeThread(uint16_t maxCount);
int WINAPI chcusb_open(uint16_t *rResult);
void WINAPI chcusb_close();
int WINAPI chcusb_ReleaseThread(uint16_t *rResult);
int WINAPI chcusb_listupPrinter(uint8_t *rIdArray);
int WINAPI chcusb_listupPrinterSN(uint64_t *rSerialArray);
int WINAPI chcusb_selectPrinter(uint8_t printerId, uint16_t *rResult);
int WINAPI chcusb_selectPrinterSN(uint64_t printerSN, uint16_t *rResult);
int WINAPI chcusb_getPrinterInfo(uint16_t tagNumber, uint8_t *rBuffer, uint32_t *rLen);
int WINAPI chcusb_imageformat(uint16_t format, uint16_t ncomp, uint16_t depth, uint16_t width, uint16_t height, uint8_t * image, uint16_t* rResult);
int WINAPI chcusb_imageformat_330(
        uint16_t format,
        uint16_t ncomp,
        uint16_t depth,
        uint16_t width,
        uint16_t height,
        uint16_t *rResult);
int __thiscall chcusb_setmtf(int32_t *mtf);
int WINAPI chcusb_makeGamma(uint16_t k, uint8_t *intoneR, uint8_t *intoneG, uint8_t *intoneB);
int WINAPI chcusb_setIcctable(
    LPCSTR icc1,
    LPCSTR icc2,
    uint16_t intents,
    uint8_t *intoneR,
    uint8_t *intoneG,
    uint8_t *intoneB,
    uint8_t *outtoneR,
    uint8_t *outtoneG,
    uint8_t *outtoneB,
    uint16_t *rResult);
int WINAPI chcusb_copies(uint16_t copies, uint16_t *rResult);
int WINAPI chcusb_status(uint16_t *rResult);
int WINAPI chcusb_statusAll(uint8_t *idArray, uint16_t *rResultArray);
int WINAPI chcusb_startpage(uint16_t postCardState, uint16_t *pageId, uint16_t *rResult);
int WINAPI chcusb_startpage_300(uint16_t postCardState, uint16_t *rResult);
int WINAPI chcusb_endpage(uint16_t *rResult);
int WINAPI chcusb_write(uint8_t *data, uint32_t *writeSize, uint16_t *rResult);
int WINAPI chcusb_writeLaminate(uint8_t *data, uint32_t *writeSize, uint16_t *rResult);
int WINAPI chcusb_writeHolo(uint8_t *data, uint32_t *writeSize, uint16_t *rResult);
int WINAPI chcusb_setPrinterInfo(uint16_t tagNumber, uint8_t *rBuffer, uint32_t *rLen, uint16_t *rResult);
int WINAPI chcusb_getGamma(LPCSTR filename, uint8_t *r, uint8_t *g, uint8_t *b, uint16_t *rResult);
int WINAPI chcusb_getMtf(LPCSTR filename, int32_t *mtf, uint16_t *rResult);
int WINAPI chcusb_cancelCopies(uint16_t pageId, uint16_t *rResult);
int WINAPI chcusb_setPrinterToneCurve(uint16_t type, uint16_t number, uint16_t *data, uint16_t *rResult);
int WINAPI chcusb_getPrinterToneCurve(uint16_t type, uint16_t number, uint16_t *data, uint16_t *rResult);
int WINAPI chcusb_blinkLED(uint16_t *rResult);
int WINAPI chcusb_resetPrinter(uint16_t *rResult);
int WINAPI chcusb_AttachThreadCount(uint16_t *rCount, uint16_t *rMaxCount);
int WINAPI chcusb_getPrintIDStatus(uint16_t pageId, uint8_t *rBuffer, uint16_t *rResult);
int WINAPI chcusb_setPrintStandby(uint16_t position, uint16_t *rResult);
int WINAPI chcusb_setPrintStandby_300(uint16_t *rResult);
int WINAPI chcusb_testCardFeed(uint16_t mode, uint16_t times, uint16_t *rResult);
int WINAPI chcusb_exitCard(uint16_t *rResult);
int WINAPI chcusb_getCardRfidTID(uint8_t *rCardTID, uint16_t *rResult);
int WINAPI chcusb_commCardRfidReader(uint8_t *sendData, uint8_t *rRecvData, uint32_t sendSize, uint32_t *rRecvSize, uint16_t *rResult);
int WINAPI chcusb_updateCardRfidReader(uint8_t *data, uint32_t size, uint16_t *rResult);
int WINAPI chcusb_getErrorLog(uint16_t index, uint8_t *rData, uint16_t *rResult);
int WINAPI chcusb_getErrorStatus(uint16_t *rBuffer);
int WINAPI chcusb_setCutList(uint8_t *rData, uint16_t *rResult);
int WINAPI chcusb_setLaminatePattern(uint16_t index, uint8_t *rData, uint16_t *rResult);
int WINAPI chcusb_color_adjustment(LPCSTR filename, int32_t a2, int32_t a3, int16_t a4, int16_t a5, int64_t a6, int64_t a7, uint16_t *rResult);
int WINAPI chcusb_color_adjustmentEx(int32_t a1, int32_t a2, int32_t a3, int16_t a4, int16_t a5, int64_t a6, int64_t a7, uint16_t *rResult);
int WINAPI chcusb_getEEPROM(uint8_t index, uint8_t *rData, uint16_t *rResult);
int WINAPI chcusb_setParameter(uint8_t a1, uint32_t a2, uint16_t *rResult);
int WINAPI chcusb_getParameter(uint8_t a1, uint8_t *a2, uint16_t *rResult);
int WINAPI chcusb_universal_command(int32_t a1, uint8_t a2, int32_t a3, uint8_t *a4, uint16_t *rResult);

/* PrintDLL API hooks */

void CFW_close(const void *handle);
int CFW_getFirmwareInfo(const void *handle, uint8_t update, LPCSTR filename, uint8_t *rBuffer, uint32_t *rLen, uint16_t *rResult);
int CFW_getPrinterInfo(const void *handle, uint16_t tagNumber, uint8_t *rBuffer, uint32_t *rLen);
int CFW_init(LPCSTR dllpath);
int CFW_listupPrinter(const void *handle, uint8_t *rIdArray);
int CFW_listupPrinterSN(const void *handle, uint64_t *rSerialArray);
int CFW_open(const void *handle, uint16_t *rResult);
int CFW_resetPrinter(const void *handle, uint16_t *rResult);
int CFW_selectPrinter(const void *handle, uint8_t printerId, uint16_t *rResult);
int CFW_selectPrinterSN(const void *handle, uint64_t printerSN, uint16_t *rResult);
int CFW_status(const void *handle, uint16_t *rResult);
int CFW_statusAll(const void *handle, uint8_t *idArray, uint16_t *rResultArray);
void CFW_term(const void *handle);
int CFW_updateFirmware(const void *handle, uint8_t update, LPCSTR filename, uint16_t *rResult);
int CHCUSB_AttachThreadCount(const void *handle, uint16_t *rCount, uint16_t *rMaxCount);
int CHCUSB_MakeThread(const void *handle, uint16_t maxCount);
int CHCUSB_ReleaseThread(const void *handle, uint16_t *rResult);
int CHCUSB_blinkLED(const void *handle, uint16_t *rResult);
int CHCUSB_cancelCopies(const void *handle, uint16_t pageId, uint16_t *rResult);
void CHCUSB_close(const void *handle);
int CHCUSB_commCardRfidReader(const void *handle, uint8_t *sendData, uint8_t *rRecvData, uint32_t sendSize, uint32_t *rRecvSize, uint16_t *rResult);
int CHCUSB_copies(const void *handle, uint16_t copies, uint16_t *rResult);
int CHCUSB_endpage(const void *handle, uint16_t *rResult);
int CHCUSB_exitCard(const void *handle, uint16_t *rResult);
int CHCUSB_getCardRfidTID(const void *handle, uint8_t *rCardTID, uint16_t *rResult);
int CHCUSB_getErrorLog(const void *handle, uint16_t index, uint8_t *rData, uint16_t *rResult);
int CHCUSB_getErrorStatus(const void *handle, uint16_t *rBuffer);
int CHCUSB_getGamma(const void *handle, LPCSTR filename, uint8_t *r, uint8_t *g, uint8_t *b, uint16_t *rResult);
int CHCUSB_getMtf(const void *handle, LPCSTR filename, int32_t *mtf, uint16_t *rResult);
int CHCUSB_getPrintIDStatus(const void *handle, uint16_t pageId, uint8_t *rBuffer, uint16_t *rResult);
int CHCUSB_getPrinterInfo(const void *handle, uint16_t tagNumber, uint8_t *rBuffer, uint32_t *rLen);
int CHCUSB_getPrinterToneCurve(const void *handle, uint16_t type, uint16_t number, uint16_t *data, uint16_t *rResult);
int CHCUSB_imageformat(const void *handle, uint16_t format, uint16_t ncomp, uint16_t depth, uint16_t width, uint16_t height, uint8_t *inputImage, uint16_t *rResult);
int CHCUSB_init(LPCSTR dllpath);
int CHCUSB_listupPrinter(const void *handle, uint8_t *rIdArray);
int CHCUSB_listupPrinterSN(const void *handle, uint64_t *rSerialArray);
int CHCUSB_makeGamma(const void *handle, uint16_t k, uint8_t *intoneR, uint8_t *intoneG, uint8_t *intoneB);
int CHCUSB_open(const void *handle, uint16_t *rResult);
int CHCUSB_resetPrinter(const void *handle, uint16_t *rResult);
int CHCUSB_selectPrinter(const void *handle, uint8_t printerId, uint16_t *rResult);
int CHCUSB_selectPrinterSN(const void *handle, uint64_t printerSN, uint16_t *rResult);
int CHCUSB_setIcctable(const void *handle, uint16_t intents, uint8_t *intoneR, uint8_t *intoneG, uint8_t *intoneB, uint8_t *outtoneR, uint8_t *outtoneG, uint8_t *outtoneB, uint16_t *rResult);
int CHCUSB_setIcctableProfile(const void *handle, LPCSTR icc1, LPCSTR icc2, uint16_t intents, uint16_t *rResult);
int CHCUSB_setPrintStandby(const void *handle, uint16_t position, uint16_t *rResult);
int CHCUSB_setPrinterInfo(const void *handle, uint16_t tagNumber, uint8_t *rBuffer, uint32_t *rLen, uint16_t *rResult);
int CHCUSB_setPrinterToneCurve(const void *handle, uint16_t type, uint16_t number, uint16_t *data, uint16_t *rResult);
int CHCUSB_setmtf(const void *handle, int32_t *mtf);
int CHCUSB_startpage(const void *handle, uint16_t postCardState, uint16_t *pageId, uint16_t *rResult);
int CHCUSB_status(const void *handle, uint16_t *rResult);
int CHCUSB_statusAll(const void *handle, uint8_t *idArray, uint16_t *rResultArray);
int CHCUSB_testCardFeed(const void *handle, uint16_t mode, uint16_t times, uint16_t *rResult);
void CHCUSB_term(const void *handle);
int CHCUSB_updateCardRfidReader(const void *handle, uint8_t *data, uint32_t size, uint16_t *rResult);
int CHCUSB_write(const void *handle, uint8_t *data, uint32_t offset, uint32_t *writeSize, uint16_t *rResult);
int CHCUSB_writeHolo(const void *handle, uint8_t *data, uint32_t offset, uint32_t *writeSize, uint16_t *rResult);
int CHCUSB_writeLaminate(const void *handle, uint8_t *data, uint32_t offset, uint32_t *writeSize, uint16_t *rResult);

/* Bitmap writing utils */

DWORD WriteDataToBitmapFile(
    LPCWSTR lpFilePath, DWORD dwBitCount,
    DWORD dwWidth, DWORD dwHeight,
    PBYTE pbInput, DWORD cbInput,
    PBYTE pbMetadata, DWORD cbMetadata,
    bool pFlip);
DWORD WriteArrayToFile(LPCSTR lpOutputFilePath, LPVOID lpDataTemp, DWORD nDataSize, BOOL isAppend);

#define MAX_CARDS 36

// big thanks to Coburn and Mitsuhide for helping me figure out this thing

// request format:
// 0xe0 - sync
// 0x?? - command
// 0x?? - payload length
// ...  - payload
// 0x?? - checksum (sum of everything except the sync byte)
//
// response format:
// 0xe0 - sync
// 0x?? - command
// 0x?? - status code
// 0x?? - payload length
// ...  - payload
// 0x?? - checksum

enum {
    DECK_CMD_RESET = 0x41,
    DECK_CMD_GET_BOOT_FW_VERSION = 0x84,
    DECK_CMD_GET_BOARD_INFO = 0x85,
    DECK_CMD_INIT_UNK1 = 0x81,
    DECK_CMD_GET_APP_FW_VERSION = 0x42,
    DECK_CMD_READ_DATA = 0x02,
    DECK_CMD_WRITE = 0x03,
    DECK_CMD_INIT_UNK2 = 0x04,
    DECK_CMD_INIT_UNK3 = 0x05,
    DECK_CMD_READ = 0x06
};

enum {
    DECK_READ_START = 0x81,
    DECK_READ_DATA = 0x82,
    DECK_READ_END = 0x83
};

struct deck_hdr {
    uint8_t sync;
    uint8_t cmd;
    uint8_t nbytes;
};

struct deck_resp_hdr {
    uint8_t sync;
    uint8_t cmd;
    uint8_t status;
    uint8_t nbytes;
};

struct deck_resp_get_boot_fw_version {
    struct deck_resp_hdr hdr;
    uint8_t boot_fw_version;
};

struct deck_resp_get_app_fw_version {
    struct deck_resp_hdr hdr;
    uint8_t app_fw_version;
};

struct deck_resp_get_board_info {
    struct deck_resp_hdr hdr;
    uint8_t board[9];
};

struct deck_resp_read_data {
    struct deck_resp_hdr hdr;
    uint8_t card_data[32];
};

struct deck_resp_read {
    struct deck_resp_hdr hdr;
    uint8_t card_data[44];
};

union deck_req_any {
    struct deck_hdr hdr;
    uint8_t bytes[520];
};

static HRESULT deck_handle_irp(struct irp *irp);
static HRESULT deck_handle_irp_locked(struct irp *irp);
static HRESULT deck_req_dispatch(const union deck_req_any *req);
static HRESULT deck_req_get_boot_fw_version(void);
static HRESULT deck_req_get_app_fw_version(void);
static HRESULT deck_req_get_board_info(void);
static HRESULT deck_req_read(void);
static HRESULT deck_req_read_data(void);
static HRESULT deck_req_write(const uint8_t *req_bytes);
static HRESULT deck_req_nop(uint8_t cmd);
static void deck_read_one(void);

static HRESULT deck_frame_accept(const struct iobuf *dest);
static void deck_frame_sync(struct iobuf *src);
static HRESULT deck_frame_decode(struct iobuf *dest, struct iobuf *src);
static HRESULT deck_frame_encode(struct iobuf *dest, const void *ptr, size_t nbytes);
static HRESULT deck_frame_encode_byte(struct iobuf *dest, uint8_t byte);

static CRITICAL_SECTION deck_lock;
static struct uart deck_uart;
static uint8_t deck_written_bytes[1024];
static uint8_t deck_readable_bytes[1024];
static uint8_t current_card_idx = 0;
static bool read_pending = false;

/* C3XXFWDLusb hook tbl. The ordinals are required, as some games, for example
   Sekito, will import this library by ordinal and not by name. */

static const struct hook_symbol C3XXFWDLusb_hooks[] = {
    {
        .name       = "fwdlusb_open",
        .ordinal    = 0x0001,
        .patch      = fwdlusb_open,
        .link       = NULL
    }, {
        .name       = "fwdlusb_close",
        .ordinal    = 0x0002,
        .patch      = fwdlusb_close,
        .link       = NULL
    }, {
        .name       = "fwdlusb_listupPrinter",
        .ordinal    = 0x0003,
        .patch      = fwdlusb_listupPrinter,
        .link       = NULL
    }, {
        .name       = "fwdlusb_listupPrinterSN",
        .ordinal    = 0x0004,
        .patch      = fwdlusb_listupPrinterSN,
        .link       = NULL
    }, {
        .name       = "fwdlusb_selectPrinter",
        .ordinal    = 0x0005,
        .patch      = fwdlusb_selectPrinter,
        .link       = NULL
    }, {
        .name       = "fwdlusb_selectPrinterSN",
        .ordinal    = 0x0006,
        .patch      = fwdlusb_selectPrinterSN,
        .link       = NULL
    }, {
        .name       = "fwdlusb_getPrinterInfo",
        .ordinal    = 0x0007,
        .patch      = fwdlusb_getPrinterInfo,
        .link       = NULL
    }, {
        .name       = "fwdlusb_status",
        .ordinal    = 0x0008,
        .patch      = fwdlusb_status,
        .link       = NULL
    }, {
        .name       = "fwdlusb_statusAll",
        .ordinal    = 0x0009,
        .patch      = fwdlusb_statusAll,
        .link       = NULL
    }, {
        .name       = "fwdlusb_resetPrinter",
        .ordinal    = 0x000a,
        .patch      = fwdlusb_resetPrinter,
        .link       = NULL
    }, {
        .name       = "fwdlusb_updateFirmware",
        .ordinal    = 0x000b,
        .patch      = fwdlusb_updateFirmware,
        .link       = NULL
    }, {
        .name       = "fwdlusb_getFirmwareInfo",
        .ordinal    = 0x000c,
        .patch      = fwdlusb_getFirmwareInfo,
        .link       = NULL
    }, {
        .name       = "fwdlusb_MakeThread",
        .ordinal    = 0x0065,
        .patch      = fwdlusb_MakeThread,
        .link       = NULL
    }, {
        .name       = "fwdlusb_ReleaseThread",
        .ordinal    = 0x0066,
        .patch      = fwdlusb_ReleaseThread,
        .link       = NULL
    }, {
        .name       = "fwdlusb_AttachThreadCount",
        .ordinal    = 0x0067,
        .patch      = fwdlusb_AttachThreadCount,
        .link       = NULL
    }, {
        .name       = "fwdlusb_getErrorLog",
        .ordinal    = 0x0068,
        .patch      = fwdlusb_getErrorLog,
        .link       = NULL
    },
};

/* C300usb hook tbl */

static const struct hook_symbol C300usb_hooks[] = {
    {
        .name       = "chcusb_MakeThread",
        .ordinal    = 0x0001,
        .patch      = chcusb_MakeThread,
        .link       = NULL
    }, {
        .name       = "chcusb_open",
        .ordinal    = 0x0002,
        .patch      = chcusb_open,
        .link       = NULL
    }, {
        .name       = "chcusb_close",
        .ordinal    = 0x0003,
        .patch      = chcusb_close,
        .link       = NULL
    }, {
        .name       = "chcusb_ReleaseThread",
        .ordinal    = 0x0004,
        .patch      = chcusb_ReleaseThread,
        .link       = NULL
    }, {
        .name       = "chcusb_listupPrinter",
        .ordinal    = 0x0005,
        .patch      = chcusb_listupPrinter,
        .link       = NULL
    }, {
        .name       = "chcusb_listupPrinterSN",
        .ordinal    = 0x0006,
        .patch      = chcusb_listupPrinterSN,
        .link       = NULL
    }, {
        .name       = "chcusb_selectPrinter",
        .ordinal    = 0x0007,
        .patch      = chcusb_selectPrinter,
        .link       = NULL
    }, {
        .name       = "chcusb_selectPrinterSN",
        .ordinal    = 0x0008,
        .patch      = chcusb_selectPrinterSN,
        .link       = NULL
    }, {
        .name       = "chcusb_getPrinterInfo",
        .ordinal    = 0x0009,
        .patch      = chcusb_getPrinterInfo,
        .link       = NULL
    }, {
        .name       = "chcusb_imageformat",
        .ordinal    = 0x000a,
        .patch      = chcusb_imageformat,
        .link       = NULL
    }, {
        .name       = "chcusb_setmtf",
        .ordinal    = 0x000b,
        .patch      = chcusb_setmtf,
        .link       = NULL
    }, {
        .name       = "chcusb_makeGamma",
        .ordinal    = 0x000c,
        .patch      = chcusb_makeGamma,
        .link       = NULL
    }, {
        .name       = "chcusb_setIcctable",
        .ordinal    = 0x000d,
        .patch      = chcusb_setIcctable,
        .link       = NULL
    }, {
        .name       = "chcusb_copies",
        .ordinal    = 0x000e,
        .patch      = chcusb_copies,
        .link       = NULL
    }, {
        .name       = "chcusb_status",
        .ordinal    = 0x000f,
        .patch      = chcusb_status,
        .link       = NULL
    }, {
        .name       = "chcusb_statusAll",
        .ordinal    = 0x0010,
        .patch      = chcusb_statusAll,
        .link       = NULL
    }, {
        .name       = "chcusb_startpage",
        .ordinal    = 0x0011,
        .patch      = chcusb_startpage_300,
        .link       = NULL
    }, {
        .name       = "chcusb_endpage",
        .ordinal    = 0x0012,
        .patch      = chcusb_endpage,
        .link       = NULL
    }, {
        .name       = "chcusb_write",
        .ordinal    = 0x0013,
        .patch      = chcusb_write,
        .link       = NULL
    }, {
        .name       = "chcusb_writeLaminate",
        .ordinal    = 0x0014,
        .patch      = chcusb_writeLaminate,
        .link       = NULL
    }, {
        .name       = "chcusb_setPrinterInfo",
        .ordinal    = 0x0015,
        .patch      = chcusb_setPrinterInfo,
        .link       = NULL
    }, {
        .name       = "chcusb_getGamma",
        .ordinal    = 0x0016,
        .patch      = chcusb_getGamma,
        .link       = NULL
    }, {
        .name       = "chcusb_getMtf",
        .ordinal    = 0x0017,
        .patch      = chcusb_getMtf,
        .link       = NULL
    }, {
        .name       = "chcusb_cancelCopies",
        .ordinal    = 0x0018,
        .patch      = chcusb_cancelCopies,
        .link       = NULL
    }, {
        .name       = "chcusb_setPrinterToneCurve",
        .ordinal    = 0x0019,
        .patch      = chcusb_setPrinterToneCurve,
        .link       = NULL
    }, {
        .name       = "chcusb_getPrinterToneCurve",
        .ordinal    = 0x001a,
        .patch      = chcusb_getPrinterToneCurve,
        .link       = NULL
    }, {
        .name       = "chcusb_blinkLED",
        .ordinal    = 0x001b,
        .patch      = chcusb_blinkLED,
        .link       = NULL
    }, {
        .name       = "chcusb_resetPrinter",
        .ordinal    = 0x001c,
        .patch      = chcusb_resetPrinter,
        .link       = NULL
    }, {
        .name       = "chcusb_AttachThreadCount",
        .ordinal    = 0x001d,
        .patch      = chcusb_AttachThreadCount,
        .link       = NULL
    }, {
        .name       = "chcusb_getPrintIDStatus",
        .ordinal    = 0x001e,
        .patch      = chcusb_getPrintIDStatus,
        .link       = NULL
    }, {
        .name       = "chcusb_setPrintStandby",
        .ordinal    = 0x001f,
        .patch      = chcusb_setPrintStandby_300,
        .link       = NULL
    }, {
        .name       = "chcusb_testCardFeed",
        .ordinal    = 0x0020,
        .patch      = chcusb_testCardFeed,
        .link       = NULL
    }, {
        .name       = "chcusb_setParameter",
        .ordinal    = 0x0021,
        .patch      = chcusb_setParameter,
        .link       = NULL
    }, {
        .name       = "chcusb_getParameter",
        .ordinal    = 0x0022,
        .patch      = chcusb_getParameter,
        .link       = NULL
    }, {
        .name       = "chcusb_getErrorStatus",
        .ordinal    = 0x0023,
        .patch      = chcusb_getErrorStatus,
        .link       = NULL
    }, {
        .name       = "chcusb_setCutList",
        .ordinal    = 0x0028,
        .patch      = chcusb_setCutList,
        .link       = NULL
    }, {
        .name       = "chcusb_setLaminatePattern",
        .ordinal    = 0x0029,
        .patch      = chcusb_setLaminatePattern,
        .link       = NULL
    }, {
        .name       = "chcusb_color_adjustment",
        .ordinal    = 0x002a,
        .patch      = chcusb_color_adjustment,
        .link       = NULL
    }, {
        .name       = "chcusb_color_adjustmentEx",
        .ordinal    = 0x002b,
        .patch      = chcusb_color_adjustmentEx,
        .link       = NULL
    }, {
        .name       = "chcusb_getEEPROM",
        .ordinal    = 0x003a,
        .patch      = chcusb_getEEPROM,
        .link       = NULL
    }, {
        .name       = "chcusb_universal_command",
        .ordinal    = 0x0049,
        .patch      = chcusb_universal_command,
        .link       = NULL
    },
};

/* C310A-Busb/C320Ausb/C330Ausb hook tbl. The ordinals are required, as some
   games, for example Sekito, will import this library by ordinal and not by
   name. */

static const struct hook_symbol C3XXusb_hooks[] = {
    {
        .name       = "chcusb_MakeThread",
        .ordinal    = 0x0001,
        .patch      = chcusb_MakeThread,
        .link       = NULL
    }, {
        .name       = "chcusb_open",
        .ordinal    = 0x0002,
        .patch      = chcusb_open,
        .link       = NULL
    }, {
        .name       = "chcusb_close",
        .ordinal    = 0x0003,
        .patch      = chcusb_close,
        .link       = NULL
    }, {
        .name       = "chcusb_ReleaseThread",
        .ordinal    = 0x0004,
        .patch      = chcusb_ReleaseThread,
        .link       = NULL
    }, {
        .name       = "chcusb_listupPrinter",
        .ordinal    = 0x0005,
        .patch      = chcusb_listupPrinter,
        .link       = NULL
    }, {
        .name       = "chcusb_listupPrinterSN",
        .ordinal    = 0x0006,
        .patch      = chcusb_listupPrinterSN,
        .link       = NULL
    }, {
        .name       = "chcusb_selectPrinter",
        .ordinal    = 0x0007,
        .patch      = chcusb_selectPrinter,
        .link       = NULL
    }, {
        .name       = "chcusb_selectPrinterSN",
        .ordinal    = 0x0008,
        .patch      = chcusb_selectPrinterSN,
        .link       = NULL
    }, {
        .name       = "chcusb_getPrinterInfo",
        .ordinal    = 0x0009,
        .patch      = chcusb_getPrinterInfo,
        .link       = NULL
    }, {
        .name       = "chcusb_imageformat",
        .ordinal    = 0x000a,
        .patch      = chcusb_imageformat,
        .link       = NULL
    }, {
        .name       = "chcusb_setmtf",
        .ordinal    = 0x000b,
        .patch      = chcusb_setmtf,
        .link       = NULL
    }, {
        .name       = "chcusb_makeGamma",
        .ordinal    = 0x000c,
        .patch      = chcusb_makeGamma,
        .link       = NULL
    }, {
        .name       = "chcusb_setIcctable",
        .ordinal    = 0x000d,
        .patch      = chcusb_setIcctable,
        .link       = NULL
    }, {
        .name       = "chcusb_copies",
        .ordinal    = 0x000e,
        .patch      = chcusb_copies,
        .link       = NULL
    }, {
        .name       = "chcusb_status",
        .ordinal    = 0x000f,
        .patch      = chcusb_status,
        .link       = NULL
    }, {
        .name       = "chcusb_statusAll",
        .ordinal    = 0x0010,
        .patch      = chcusb_statusAll,
        .link       = NULL
    }, {
        .name       = "chcusb_startpage",
        .ordinal    = 0x0011,
        .patch      = chcusb_startpage,
        .link       = NULL
    }, {
        .name       = "chcusb_endpage",
        .ordinal    = 0x0012,
        .patch      = chcusb_endpage,
        .link       = NULL
    }, {
        .name       = "chcusb_write",
        .ordinal    = 0x0013,
        .patch      = chcusb_write,
        .link       = NULL
    }, {
        .name       = "chcusb_writeLaminate",
        .ordinal    = 0x0014,
        .patch      = chcusb_writeLaminate,
        .link       = NULL
    }, {
        .name       = "chcusb_writeHolo",
        .ordinal    = 0x0015,
        .patch      = chcusb_writeHolo,
        .link       = NULL
    }, {
        .name       = "chcusb_setPrinterInfo",
        .ordinal    = 0x0016,
        .patch      = chcusb_setPrinterInfo,
        .link       = NULL
    }, {
        .name       = "chcusb_getGamma",
        .ordinal    = 0x0017,
        .patch      = chcusb_getGamma,
        .link       = NULL
    }, {
        .name       = "chcusb_getMtf",
        .ordinal    = 0x0018,
        .patch      = chcusb_getMtf,
        .link       = NULL
    }, {
        .name       = "chcusb_cancelCopies",
        .ordinal    = 0x0019,
        .patch      = chcusb_cancelCopies,
        .link       = NULL
    }, {
        .name       = "chcusb_setPrinterToneCurve",
        .ordinal    = 0x001a,
        .patch      = chcusb_setPrinterToneCurve,
        .link       = NULL
    }, {
        .name       = "chcusb_getPrinterToneCurve",
        .ordinal    = 0x001b,
        .patch      = chcusb_getPrinterToneCurve,
        .link       = NULL
    }, {
        .name       = "chcusb_blinkLED",
        .ordinal    = 0x001c,
        .patch      = chcusb_blinkLED,
        .link       = NULL
    }, {
        .name       = "chcusb_resetPrinter",
        .ordinal    = 0x001d,
        .patch      = chcusb_resetPrinter,
        .link       = NULL
    }, {
        .name       = "chcusb_AttachThreadCount",
        .ordinal    = 0x001e,
        .patch      = chcusb_AttachThreadCount,
        .link       = NULL
    }, {
        .name       = "chcusb_getPrintIDStatus",
        .ordinal    = 0x001f,
        .patch      = chcusb_getPrintIDStatus,
        .link       = NULL
    }, {
        .name       = "chcusb_setPrintStandby",
        .ordinal    = 0x0020,
        .patch      = chcusb_setPrintStandby,
        .link       = NULL
    }, {
        .name       = "chcusb_testCardFeed",
        .ordinal    = 0x0021,
        .patch      = chcusb_testCardFeed,
        .link       = NULL
    }, {
        .name       = "chcusb_exitCard",
        .ordinal    = 0x0022,
        .patch      = chcusb_exitCard,
        .link       = NULL
    }, {
        .name       = "chcusb_getCardRfidTID",
        .ordinal    = 0x0023,
        .patch      = chcusb_getCardRfidTID,
        .link       = NULL
    }, {
        .name       = "chcusb_commCardRfidReader",
        .ordinal    = 0x0024,
        .patch      = chcusb_commCardRfidReader,
        .link       = NULL
    }, {
        .name       = "chcusb_updateCardRfidReader",
        .ordinal    = 0x0025,
        .patch      = chcusb_updateCardRfidReader,
        .link       = NULL
    }, {
        .name       = "chcusb_getErrorLog",
        .ordinal    = 0x0026,
        .patch      = chcusb_getErrorLog,
        .link       = NULL
    }, {
        .name       = "chcusb_getErrorStatus",
        .ordinal    = 0x0027,
        .patch      = chcusb_getErrorStatus,
        .link       = NULL
    }, {
        .name       = "chcusb_setCutList",
        .ordinal    = 0x0028,
        .patch      = chcusb_setCutList,
        .link       = NULL
    }, {
        .name       = "chcusb_setLaminatePattern",
        .ordinal    = 0x0029,
        .patch      = chcusb_setLaminatePattern,
        .link       = NULL
    }, {
        .name       = "chcusb_color_adjustment",
        .ordinal    = 0x002a,
        .patch      = chcusb_color_adjustment,
        .link       = NULL
    }, {
        .name       = "chcusb_color_adjustmentEx",
        .ordinal    = 0x002b,
        .patch      = chcusb_color_adjustmentEx,
        .link       = NULL
    }, {
        .name       = "chcusb_getEEPROM",
        .ordinal    = 0x003a,
        .patch      = chcusb_getEEPROM,
        .link       = NULL
    }, {
        .name       = "chcusb_setParameter",
        .ordinal    = 0x0040,
        .patch      = chcusb_setParameter,
        .link       = NULL
    }, {
        .name       = "chcusb_getParameter",
        .ordinal    = 0x0041,
        .patch      = chcusb_getParameter,
        .link       = NULL
    }, {
        .name       = "chcusb_universal_command",
        .ordinal    = 0x0049,
        .patch      = chcusb_universal_command,
        .link       = NULL
    },
};

/* PrintDLL hook tbl */

static struct hook_symbol printdll_hooks[] = {
    {
        .name       = "CFW_close",
        .ordinal    = 0x0001,
        .patch      = CFW_close,
        .link       = NULL
    }, {
        .name       = "CFW_getFirmwareInfo",
        .ordinal    = 0x0002,
        .patch      = CFW_getFirmwareInfo,
        .link       = NULL
    }, {
        .name       = "CFW_getPrinterInfo",
        .ordinal    = 0x0003,
        .patch      = CFW_getPrinterInfo,
        .link       = NULL
    }, {
        .name       = "CFW_init",
        .ordinal    = 0x0004,
        .patch      = CFW_init,
        .link       = NULL
    }, {
        .name       = "CFW_listupPrinter",
        .ordinal    = 0x0005,
        .patch      = CFW_listupPrinter,
        .link       = NULL
    }, {
        .name       = "CFW_listupPrinterSN",
        .ordinal    = 0x0006,
        .patch      = CFW_listupPrinterSN,
        .link       = NULL
    }, {
        .name       = "CFW_open",
        .ordinal    = 0x0007,
        .patch      = CFW_open,
        .link       = NULL
    }, {
        .name       = "CFW_resetPrinter",
        .ordinal    = 0x0008,
        .patch      = CFW_resetPrinter,
        .link       = NULL
    }, {
        .name       = "CFW_selectPrinter",
        .ordinal    = 0x0009,
        .patch      = CFW_selectPrinter,
        .link       = NULL
    }, {
        .name       = "CFW_selectPrinterSN",
        .ordinal    = 0x000a,
        .patch      = CFW_selectPrinterSN,
        .link       = NULL
    }, {
        .name       = "CFW_status",
        .ordinal    = 0x000b,
        .patch      = CFW_status,
        .link       = NULL
    }, {
        .name       = "CFW_statusAll",
        .ordinal    = 0x000c,
        .patch      = CFW_statusAll,
        .link       = NULL
    }, {
        .name       = "CFW_term",
        .ordinal    = 0x000d,
        .patch      = CFW_term,
        .link       = NULL
    }, {
        .name       = "CFW_updateFirmware",
        .ordinal    = 0x000e,
        .patch      = CFW_updateFirmware,
        .link       = NULL
    }, {
        .name       = "CHCUSB_AttachThreadCount",
        .ordinal    = 0x000f,
        .patch      = CHCUSB_AttachThreadCount,
        .link       = NULL
    }, {
        .name       = "CHCUSB_MakeThread",
        .ordinal    = 0x0010,
        .patch      = CHCUSB_MakeThread,
        .link       = NULL
    }, {
        .name       = "CHCUSB_ReleaseThread",
        .ordinal    = 0x0011,
        .patch      = CHCUSB_ReleaseThread,
        .link       = NULL
    }, {
        .name       = "CHCUSB_blinkLED",
        .ordinal    = 0x0012,
        .patch      = CHCUSB_blinkLED,
        .link       = NULL
    }, {
        .name       = "CHCUSB_cancelCopies",
        .ordinal    = 0x0013,
        .patch      = CHCUSB_cancelCopies,
        .link       = NULL
    }, {
        .name       = "CHCUSB_close",
        .ordinal    = 0x0014,
        .patch      = CHCUSB_close,
        .link       = NULL
    }, {
        .name       = "CHCUSB_commCardRfidReader",
        .ordinal    = 0x0015,
        .patch      = CHCUSB_commCardRfidReader,
        .link       = NULL
    }, {
        .name       = "CHCUSB_copies",
        .ordinal    = 0x0016,
        .patch      = CHCUSB_copies,
        .link       = NULL
    }, {
        .name       = "CHCUSB_endpage",
        .ordinal    = 0x0017,
        .patch      = CHCUSB_endpage,
        .link       = NULL
    }, {
        .name       = "CHCUSB_exitCard",
        .ordinal    = 0x0018,
        .patch      = CHCUSB_exitCard,
        .link       = NULL
    }, {
        .name       = "CHCUSB_getCardRfidTID",
        .ordinal    = 0x0019,
        .patch      = CHCUSB_getCardRfidTID,
        .link       = NULL
    }, {
        .name       = "CHCUSB_getErrorLog",
        .ordinal    = 0x001a,
        .patch      = CHCUSB_getErrorLog,
        .link       = NULL
    }, {
        .name       = "CHCUSB_getErrorStatus",
        .ordinal    = 0x001b,
        .patch      = CHCUSB_getErrorStatus,
        .link       = NULL
    }, {
        .name       = "CHCUSB_getGamma",
        .ordinal    = 0x001c,
        .patch      = CHCUSB_getGamma,
        .link       = NULL
    }, {
        .name       = "CHCUSB_getMtf",
        .ordinal    = 0x001d,
        .patch      = CHCUSB_getMtf,
        .link       = NULL
    }, {
        .name       = "CHCUSB_getPrintIDStatus",
        .ordinal    = 0x001e,
        .patch      = CHCUSB_getPrintIDStatus,
        .link       = NULL
    }, {
        .name       = "CHCUSB_getPrinterInfo",
        .ordinal    = 0x001f,
        .patch      = CHCUSB_getPrinterInfo,
        .link       = NULL
    }, {
        .name       = "CHCUSB_getPrinterToneCurve",
        .ordinal    = 0x0020,
        .patch      = CHCUSB_getPrinterToneCurve,
        .link       = NULL
    }, {
        .name       = "CHCUSB_imageformat",
        .ordinal    = 0x0021,
        .patch      = CHCUSB_imageformat,
        .link       = NULL
    }, {
        .name       = "CHCUSB_init",
        .ordinal    = 0x0022,
        .patch      = CHCUSB_init,
        .link       = NULL
    }, {
        .name       = "CHCUSB_listupPrinter",
        .ordinal    = 0x0023,
        .patch      = CHCUSB_listupPrinter,
        .link       = NULL
    }, {
        .name       = "CHCUSB_listupPrinterSN",
        .ordinal    = 0x0024,
        .patch      = CHCUSB_listupPrinterSN,
        .link       = NULL
    }, {
        .name       = "CHCUSB_makeGamma",
        .ordinal    = 0x0025,
        .patch      = CHCUSB_makeGamma,
        .link       = NULL
    }, {
        .name       = "CHCUSB_open",
        .ordinal    = 0x0026,
        .patch      = CHCUSB_open,
        .link       = NULL
    }, {
        .name       = "CHCUSB_resetPrinter",
        .ordinal    = 0x0027,
        .patch      = CHCUSB_resetPrinter,
        .link       = NULL
    }, {
        .name       = "CHCUSB_selectPrinter",
        .ordinal    = 0x0028,
        .patch      = CHCUSB_selectPrinter,
        .link       = NULL
    }, {
        .name       = "CHCUSB_selectPrinterSN",
        .ordinal    = 0x0029,
        .patch      = CHCUSB_selectPrinterSN,
        .link       = NULL
    }, {
        .name       = "CHCUSB_setIcctable",
        .ordinal    = 0x002a,
        .patch      = CHCUSB_setIcctable,
        .link       = NULL
    }, {
        .name       = "CHCUSB_setIcctableProfile",
        .ordinal    = 0x002b,
        .patch      = CHCUSB_setIcctableProfile,
        .link       = NULL
    }, {
        .name       = "CHCUSB_setPrintStandby",
        .ordinal    = 0x002c,
        .patch      = CHCUSB_setPrintStandby,
        .link       = NULL
    }, {
        .name       = "CHCUSB_setPrinterInfo",
        .ordinal    = 0x002d,
        .patch      = CHCUSB_setPrinterInfo,
        .link       = NULL
    }, {
        .name       = "CHCUSB_setPrinterToneCurve",
        .ordinal    = 0x0023,
        .patch      = CHCUSB_setPrinterToneCurve,
        .link       = NULL
    }, {
        .name       = "CHCUSB_setmtf",
        .ordinal    = 0x002f,
        .patch      = CHCUSB_setmtf,
        .link       = NULL
    }, {
        .name       = "CHCUSB_startpage",
        .ordinal    = 0x0030,
        .patch      = CHCUSB_startpage,
        .link       = NULL
    }, {
        .name       = "CHCUSB_status",
        .ordinal    = 0x0031,
        .patch      = CHCUSB_status,
        .link       = NULL
    }, {
        .name       = "CHCUSB_statusAll",
        .ordinal    = 0x0032,
        .patch      = CHCUSB_statusAll,
        .link       = NULL
    }, {
        .name       = "CHCUSB_term",
        .ordinal    = 0x0033,
        .patch      = CHCUSB_term,
        .link       = NULL
    }, {
        .name       = "CHCUSB_testCardFeed",
        .ordinal    = 0x0034,
        .patch      = CHCUSB_testCardFeed,
        .link       = NULL
    }, {
        .name       = "CHCUSB_updateCardRfidReader",
        .ordinal    = 0x0035,
        .patch      = CHCUSB_updateCardRfidReader,
        .link       = NULL
    }, {
        .name       = "CHCUSB_write",
        .ordinal    = 0x0036,
        .patch      = CHCUSB_write,
        .link       = NULL
    }, {
        .name       = "CHCUSB_writeHolo",
        .ordinal    = 0x0037,
        .patch      = CHCUSB_writeHolo,
        .link       = NULL
    }, {
        .name       = "CHCUSB_writeLaminate",
        .ordinal    = 0x0038,
        .patch      = CHCUSB_writeLaminate,
        .link       = NULL
    },
};

static struct printer_config printer_config;

void printer_hook_init(const struct printer_config *cfg, int rfid_port_no, HINSTANCE self) {
    HANDLE fwFile = NULL;
    DWORD bytesRead = 0;

    assert(cfg != NULL);

    if (!cfg->enable) {
        return;
    }

    /* Set serial and rotate cfg */

    memcpy(&serialNo, cfg->serial_no, sizeof(serialNo));
    _byteswap_uint64(serialNo);

    rotate180 = cfg->rotate_180;

    memcpy(&printer_config, cfg, sizeof(*cfg));
    printer_hook_insert_hooks(NULL);

    /*
    if (self != NULL) {
        // dll_hook_push(self, L"PrintDLL.dll"); // TODO: This doesn't work. Unity moment
        dll_hook_push(self, L"C300usb.dll");
        dll_hook_push(self, L"C300FWDLusb.dll");
        dll_hook_push(self, L"C310Ausb.dll");
        dll_hook_push(self, L"C310Busb.dll");
        dll_hook_push(self, L"C310FWDLusb.dll");
        dll_hook_push(self, L"C310BFWDLusb.dll");
        dll_hook_push(self, L"C320Ausb.dll");
        dll_hook_push(self, L"C320AFWDLusb.dll");
        dll_hook_push(self, L"C330Ausb.dll");
        dll_hook_push(self, L"C330AFWDLusb.dll");
    }
    */

    // Load firmware if has previously been written to
    fwFile = CreateFileW(cfg->main_fw_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fwFile != NULL) {
        ReadFile(fwFile, mainFirmware, sizeof(mainFirmware), &bytesRead, NULL);
        CloseHandle(fwFile);
    }

    fwFile = CreateFileW(cfg->dsp_fw_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fwFile != NULL) {
        ReadFile(fwFile, dspFirmware, sizeof(dspFirmware), &bytesRead, NULL);
        CloseHandle(fwFile);
    }

    fwFile = CreateFileW(cfg->param_fw_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fwFile != NULL) {
        ReadFile(fwFile, paramFirmware, sizeof(dspFirmware), &bytesRead, NULL);
        CloseHandle(fwFile);
    }

    CreateDirectoryW(cfg->printer_out_path, NULL);
    memcpy(printer_out_path, cfg->printer_out_path, MAX_PATH);

    if (rfid_port_no != 0) {
        InitializeCriticalSection(&deck_lock);

        uart_init(&deck_uart, rfid_port_no);
        deck_uart.written.bytes = deck_written_bytes;
        deck_uart.written.nbytes = sizeof(deck_written_bytes);
        deck_uart.readable.bytes = deck_readable_bytes;
        deck_uart.readable.nbytes = sizeof(deck_readable_bytes);

        dprintf("Printer: RFID: hook enabled.\n");

        if (FAILED(iohook_push_handler(deck_handle_irp))) {
            dprintf("Printer: RFID: failed to hook IRP handler.\n");
        }
    }

    dprintf("Printer: hook enabled.\n");
}

void printer_hook_insert_hooks(HMODULE target) {
    hook_table_apply(target, "C310Ausb.dll", C3XXusb_hooks, _countof(C3XXusb_hooks));
    hook_table_apply(target, "C310Busb.dll", C3XXusb_hooks, _countof(C3XXusb_hooks));
    hook_table_apply(target, "C310FWDLusb.dll", C3XXFWDLusb_hooks, _countof(C3XXFWDLusb_hooks));
    hook_table_apply(target, "C310BFWDLusb.dll", C3XXFWDLusb_hooks, _countof(C3XXFWDLusb_hooks));
    hook_table_apply(target, "C320Ausb.dll", C3XXusb_hooks, _countof(C3XXusb_hooks));
    hook_table_apply(target, "C320AFWDLusb.dll", C3XXFWDLusb_hooks, _countof(C3XXFWDLusb_hooks));
    hook_table_apply(target, "C330Ausb.dll", C3XXusb_hooks, _countof(C3XXusb_hooks));
    hook_table_apply(target, "C330AFWDLusb.dll", C3XXFWDLusb_hooks, _countof(C3XXFWDLusb_hooks));

    /* Unity workaround */
    proc_addr_table_push(target, "PrintDLL.dll", printdll_hooks, _countof(printdll_hooks));
    proc_addr_table_push(target, "C300usb.dll", C300usb_hooks, _countof(C300usb_hooks));
    proc_addr_table_push(target, "C300FWDLusb.dll", C3XXFWDLusb_hooks, _countof(C3XXFWDLusb_hooks));
}

static void generate_rfid(void) {
    for (int i = 0; i < sizeof(cardRFID); i++)
        cardRFID[i] = rand();

    dprintf("Printer: New card RFID:\n");
    dump(cardRFID, sizeof(cardRFID));
}

static HRESULT deck_handle_irp(struct irp *irp) {
    HRESULT hr;

    assert(irp != NULL);

    if (!uart_match_irp(&deck_uart, irp)) {
        return iohook_invoke_next(irp);
    }

    EnterCriticalSection(&deck_lock);
    hr = deck_handle_irp_locked(irp);
    LeaveCriticalSection(&deck_lock);

    return hr;
}

static HRESULT deck_handle_irp_locked(struct irp *irp) {
    uint8_t req[1024];
    struct iobuf req_iobuf;
    HRESULT hr;

    if (irp->op == IRP_OP_OPEN) {
        dprintf("Printer: RFID: Starting backend\n");
    }

    hr = uart_handle_irp(&deck_uart, irp);

    if (SUCCEEDED(hr) && irp->op == IRP_OP_READ && read_pending == true) {
        deck_read_one();
        return S_OK;
    }

    if (FAILED(hr) || irp->op != IRP_OP_WRITE) {
        return hr;
    }

    for (;;) {
        // if (deck_uart.written.pos != 0) {
        //     dprintf("Printer: RFID: TX Buffer:\n");
        //     dump_iobuf(&deck_uart.written);
        // }

        req_iobuf.bytes = req;
        req_iobuf.nbytes = sizeof(req);
        req_iobuf.pos = 0;

        hr = deck_frame_decode(&req_iobuf, &deck_uart.written);

        if (hr != S_OK) {
            if (FAILED(hr)) {
                dprintf("Printer: RFID: Deframe error: %x, %d %d\n", (int)hr, (int)req_iobuf.nbytes, (int)req_iobuf.pos);
            }

            return hr;
        }

        // dprintf("Printer: RFID: Deframe Buffer:\n");
        // dump_iobuf(&req_iobuf);

        hr = deck_req_dispatch((const union deck_req_any *)&req);

        if (FAILED(hr)) {
            dprintf("Printer: RFID: Processing error: %x\n", (int)hr);
        }

        // dprintf("Printer: RFID: Written bytes:\n");
        // dump_iobuf(&deck_uart.readable);
    }
}

static HRESULT deck_req_dispatch(const union deck_req_any *req) {
    switch (req->hdr.cmd) {
        case DECK_CMD_RESET:
        case DECK_CMD_INIT_UNK1:
        case DECK_CMD_INIT_UNK2:
        case DECK_CMD_INIT_UNK3:
            return deck_req_nop(req->hdr.cmd);

        case DECK_CMD_GET_BOOT_FW_VERSION:
            return deck_req_get_boot_fw_version();

        case DECK_CMD_GET_APP_FW_VERSION:
            return deck_req_get_app_fw_version();

        case DECK_CMD_GET_BOARD_INFO:
            return deck_req_get_board_info();

        case DECK_CMD_READ:
            return deck_req_read();

        case DECK_CMD_READ_DATA:
            return deck_req_read_data();

        case DECK_CMD_WRITE:
            return deck_req_write(req->bytes);

        default:
            dprintf("Printer: RFID: Unhandled command %#02x\n", req->hdr.cmd);

            return S_OK;
    }
}

static HRESULT deck_req_get_boot_fw_version(void) {
    struct deck_resp_get_boot_fw_version resp;

    dprintf("Printer: RFID: Get Boot FW Version\n");

    resp.hdr.sync = 0xE0;
    resp.hdr.cmd = DECK_CMD_GET_BOOT_FW_VERSION;
    resp.hdr.status = 0;
    resp.hdr.nbytes = 1;
    resp.boot_fw_version = 0x90;

    return deck_frame_encode(&deck_uart.readable, &resp, sizeof(resp));
}

static HRESULT deck_req_get_app_fw_version(void) {
    struct deck_resp_get_app_fw_version resp;

    dprintf("Printer: RFID: Get App FW Version\n");

    resp.hdr.sync = 0xE0;
    resp.hdr.cmd = DECK_CMD_GET_APP_FW_VERSION;
    resp.hdr.status = 0;
    resp.hdr.nbytes = 1;
    resp.app_fw_version = 0x90;

    return deck_frame_encode(&deck_uart.readable, &resp, sizeof(resp));
}

static HRESULT deck_req_get_board_info(void) {
    struct deck_resp_get_board_info resp;

    dprintf("Printer: RFID: Get Board Info\n");

    resp.hdr.sync = 0xE0;
    resp.hdr.cmd = DECK_CMD_GET_BOARD_INFO;
    resp.hdr.status = 0;
    resp.hdr.nbytes = 9;
    memcpy(resp.board, (void *)"837-15347", 9);

    return deck_frame_encode(&deck_uart.readable, &resp, sizeof(resp));
}

static HRESULT deck_req_read(void) {
    struct deck_resp_read resp;

    dprintf("Printer: RFID: Read Card\n");

    resp.hdr.sync = 0xE0;
    resp.hdr.cmd = DECK_CMD_READ;
    resp.hdr.status = DECK_READ_START;
    resp.hdr.nbytes = 0;

    current_card_idx = 0;
    read_pending = true;

    return deck_frame_encode(&deck_uart.readable, &resp.hdr, sizeof(resp.hdr));
}

static HRESULT deck_req_read_data(void) {
    struct deck_resp_read_data resp;

    dprintf("Printer: RFID: Read Card Data\n");

    resp.hdr.sync = 0xE0;
    resp.hdr.cmd = DECK_CMD_READ_DATA;
    resp.hdr.status = 0;
    resp.hdr.nbytes = 32;
    memcpy(resp.card_data, cardDataBuffer, 32);

    return deck_frame_encode(&deck_uart.readable, &resp, sizeof(resp));
}

static HRESULT deck_req_write(const uint8_t *req_bytes) {
    struct deck_resp_hdr resp;
    uint8_t off;

    resp.sync = 0xE0;
    resp.cmd = DECK_CMD_WRITE;
    resp.status = 0;
    resp.nbytes = 0;

    off = (req_bytes[5] - 1) * 2;
    cardDataBuffer[off] = req_bytes[6];
    cardDataBuffer[off + 1] = req_bytes[7];
    if (req_bytes[5] == 0x10) {
        dprintf("Printer: RFID: Card Data Buffer: \n");
        dump(cardDataBuffer, 0x20);
    }

    return deck_frame_encode(&deck_uart.readable, &resp, sizeof(resp));
}

static void deck_read_one(void) {
    struct deck_resp_read resp;

    dprintf("Printer: RFID: Read One Card\n");

    resp.hdr.sync = 0xE0;
    resp.hdr.cmd = DECK_CMD_READ;

    if (current_card_idx < 1) {
        resp.hdr.status = DECK_READ_DATA;
        resp.hdr.nbytes = 44;
        memset(resp.card_data, 0, 44);
        memcpy(resp.card_data, cardRFID, 0xC);
        memcpy(resp.card_data + 0x0C, cardDataBuffer, 0x20);
        dump(resp.card_data, 44);

        deck_frame_encode(&deck_uart.readable, &resp, sizeof(resp));
        current_card_idx++;
    } else {
        resp.hdr.status = DECK_READ_END;
        resp.hdr.nbytes = 0;
        deck_frame_encode(&deck_uart.readable, &resp.hdr, sizeof(resp.hdr));
        read_pending = false;
    }
}

static HRESULT deck_req_nop(uint8_t cmd) {
    struct deck_resp_hdr resp;

    dprintf("Printer: RFID: No-op cmd %#02x\n", cmd);

    resp.sync = 0xE0;
    resp.cmd = cmd;
    resp.status = 0;
    resp.nbytes = 0;

    return deck_frame_encode(&deck_uart.readable, &resp, sizeof(resp));
}

static void deck_frame_sync(struct iobuf *src) {
    size_t i;

    for (i = 0; i < src->pos && src->bytes[i] != 0xE0; i++)
        ;

    src->pos -= i;
    memmove(&src->bytes[0], &src->bytes[i], i);
}

static HRESULT deck_frame_accept(const struct iobuf *dest) {
    if (dest->pos < 2 || dest->pos != dest->bytes[2] + 4) {
        return S_FALSE;
    }

    return S_OK;
}

static HRESULT deck_frame_decode(struct iobuf *dest, struct iobuf *src) {
    uint8_t byte;
    bool escape;
    size_t i;
    HRESULT hr;

    assert(dest != NULL);
    assert(dest->bytes != NULL || dest->nbytes == 0);
    assert(dest->pos <= dest->nbytes);
    assert(src != NULL);
    assert(src->bytes != NULL || src->nbytes == 0);
    assert(src->pos <= src->nbytes);

    dest->pos = 0;
    escape = false;

    for (i = 0, hr = S_FALSE; i < src->pos && hr == S_FALSE; i++) {
        /* Step the FSM to unstuff another byte */

        byte = src->bytes[i];

        if (dest->pos >= dest->nbytes) {
            hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        } else if (i == 0) {
            dest->bytes[dest->pos++] = byte;
        } else if (byte == 0xE0) {
            hr = E_FAIL;
        } else if (byte == 0xD0) {
            if (escape) {
                hr = E_FAIL;
            }

            escape = true;
        } else if (escape) {
            dest->bytes[dest->pos++] = byte + 1;
            escape = false;
        } else {
            dest->bytes[dest->pos++] = byte;
        }

        /* Try to accept the packet we've built up so far */

        if (SUCCEEDED(hr)) {
            hr = deck_frame_accept(dest);
        }
    }

    /* Handle FSM terminal state */

    if (hr != S_FALSE) {
        /* Frame was either accepted or rejected, remove it from src */
        memmove(&src->bytes[0], &src->bytes[i], src->pos - i);
        src->pos -= i;
    }

    return hr;
}

static HRESULT deck_frame_encode(
    struct iobuf *dest,
    const void *ptr,
    size_t nbytes) {
    const uint8_t *src;
    uint8_t checksum;
    uint8_t byte;
    size_t i;
    HRESULT hr;

    assert(dest != NULL);
    assert(dest->bytes != NULL || dest->nbytes == 0);
    assert(dest->pos <= dest->nbytes);
    assert(ptr != NULL);

    src = ptr;

    assert(nbytes >= 2 && src[0] == 0xE0 && src[3] + 4 == nbytes);

    if (dest->pos >= dest->nbytes) {
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }

    dest->bytes[dest->pos++] = 0xE0;
    checksum = 0x0;

    for (i = 1; i < nbytes; i++) {
        byte = src[i];
        checksum += byte;

        hr = deck_frame_encode_byte(dest, byte);

        if (FAILED(hr)) {
            return hr;
        }
    }

    return deck_frame_encode_byte(dest, checksum);
}

static HRESULT deck_frame_encode_byte(struct iobuf *dest, uint8_t byte) {
    if (byte == 0xD0 || byte == 0xE0) {
        if (dest->pos + 2 > dest->nbytes) {
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        }

        dest->bytes[dest->pos++] = 0xD0;
        dest->bytes[dest->pos++] = byte - 1;
    } else {
        if (dest->pos + 1 > dest->nbytes) {
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        }

        dest->bytes[dest->pos++] = byte;
    }

    return S_OK;
}

// C3XXFWDLusb stubs

int WINAPI fwdlusb_open(uint16_t *rResult) {
    dprintf("Printer: C3XXFWDLusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

void fwdlusb_close() {}

int WINAPI fwdlusb_listupPrinter(uint8_t *rIdArray) {
    dprintf("Printer: C3XXFWDLusb: %s\n", __func__);
    memset(rIdArray, 0xFF, 0x80);
    rIdArray[0] = idNumber;
    return 1;
}

int WINAPI fwdlusb_listupPrinterSN(uint64_t *rSerialArray) {
    dprintf("Printer: C3XXFWDLusb: %s\n", __func__);
    memset(rSerialArray, 0xFF, 0x400);
    rSerialArray[0] = serialNo;
    return 1;
}

int WINAPI fwdlusb_selectPrinter(uint8_t printerId, uint16_t *rResult) {
    dprintf("Printer: C3XXFWDLusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI fwdlusb_selectPrinterSN(uint64_t printerSN, uint16_t *rResult) {
    dprintf("Printer: C3XXFWDLusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI fwdlusb_getPrinterInfo(uint16_t tagNumber, uint8_t *rBuffer, uint32_t *rLen) {
    dprintf("Printer: C3XXFWDLusb: %s\n", __func__);
    switch (tagNumber) {
        case 0:  // getPaperInfo
            if (*rLen != 0x67) *rLen = 0x67;
            if (rBuffer) memset(rBuffer, 0, *rLen);
            return 1;
        case 3:  // getFirmwareVersion
            if (*rLen != 0x99) *rLen = 0x99;
            if (rBuffer) {
                memset(rBuffer, 0, *rLen);
                rBuffer[0] = 4; // firmware count
                // bootFirmware
                int i = 1;
                memcpy(rBuffer + i, mainFirmware, sizeof(mainFirmware));
                // mainFirmware
                i += 0x26;
                memcpy(rBuffer + i, mainFirmware, sizeof(mainFirmware));
                // printParameterTable
                i += 0x26;
                memcpy(rBuffer + i, paramFirmware, sizeof(paramFirmware));
                // dspFirmware (C300 only)
                i += 0x26;
                memcpy(rBuffer + i, dspFirmware, 0x26);
            }
            return 1;
        case 4:  // getPrintCountInfo (C300 only)
            if (!rBuffer) {
                *rLen = 0x1C;
                return 1;
            }
            int32_t bInfoC300[10] = {0};
            uint16_t printCounterC300;
            bInfoC300[0] = 22;   // printCounter0
            bInfoC300[1] = 23;   // printCounter1
            bInfoC300[2] = 33;   // feedRollerCount
            bInfoC300[3] = 55;   // cutterCount
            bInfoC300[4] = 88;   // headCount
            bInfoC300[5] = 999;  // ribbonRemain
            bInfoC300[6] = 0;    // dummy
            if (*rLen <= 0x1Cu) {
                memcpy(rBuffer, bInfoC300, *rLen);
            } else {
                bInfoC300[7] = 0;  // TODO
                if (*rLen > 0x20u) *rLen = 0x20;
                memcpy(rBuffer, bInfoC300, *rLen);
            }
            break;
        case 5:  // getPrintCountInfo
            if (!rBuffer) {
                *rLen = 0x28;
                return 1;
            }
            int32_t bInfo[10] = {0};
            bInfo[0] = 22;   // printCounter0
            bInfo[1] = 23;   // printCounter1
            bInfo[2] = 33;   // feedRollerCount
            bInfo[3] = 55;   // cutterCount
            bInfo[4] = 88;   // headCount
            bInfo[5] = 999;  // ribbonRemain
            bInfo[6] = 99;   // holoHeadCount
            if (*rLen <= 0x1Cu) {
                memcpy(rBuffer, bInfo, *rLen);
            } else {
                bInfo[7] = 69;  // paperCount
                bInfo[8] = 21;  // printCounter2
                bInfo[9] = 20;  // holoPrintCounter
                if (*rLen > 0x28u) *rLen = 0x28;
                memcpy(rBuffer, bInfo, *rLen);
            }
            break;
        case 26:  // getPrinterSerial
            if (*rLen != 8) *rLen = 8;
            if (rBuffer) memcpy(rBuffer, &serialNo, 8);
            return 1;
        default:
            dprintf("Printer: %s: Unknown parameter 'tagNumber' value %d.\n", __func__, tagNumber);
            break;
    }
    return 1;
}

int WINAPI fwdlusb_status(uint16_t *rResult) {
    dprintf("Printer: C3XXFWDLusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI fwdlusb_statusAll(uint8_t *idArray, uint16_t *rResultArray) {
    dprintf("Printer: C3XXFWDLusb: %s\n", __func__);
    for (int i = 0; *(uint8_t *)(idArray + i) != 255 && i < 128; ++i) {
        *(uint16_t *)(rResultArray + 2 * i) = 0;
    }

    return 1;
}

int WINAPI fwdlusb_resetPrinter(uint16_t *rResult) {
    dprintf("Printer: C3XXFWDLusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI fwdlusb_getFirmwareVersion(uint8_t *buffer, int size) {
    int8_t a;
    uint32_t b = 0;
    for (int32_t i = 0; i < size; ++i) {
        if (*(int8_t *)(buffer + i) < 0x30 || *(int8_t *)(buffer + i) > 0x39) {
            if (*(int8_t *)(buffer + i) < 0x41 || *(int8_t *)(buffer + i) > 0x46) {
                if (*(int8_t *)(buffer + i) < 0x61 || *(int8_t *)(buffer + i) > 0x66) {
                    return 0;
                }
                a = *(int8_t *)(buffer + i) - 0x57;
            } else {
                a = *(int8_t *)(buffer + i) - 0x37;
            }
        } else {
            a = *(int8_t *)(buffer + i) - 0x30;
        }
        b = a + 0x10 * b;
    }
    return b;
}

int WINAPI fwdlusb_updateFirmware_main(uint8_t update, LPCSTR filename, uint16_t *rResult) {
    DWORD result;
    HANDLE fwFile = NULL;
    DWORD bytesWritten = 0;

    if (filename) {
        HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return 0;
        {
            if (rResult) *rResult = 1005;
            result = 0;
        }

        DWORD read;
        uint8_t buffer[0x40];
        result = ReadFile(hFile, buffer, 0x40, &read, NULL);
        CloseHandle(hFile);
        if (result && read > 0x24) {
            uint8_t rBuffer[0x40] = {0};

            memcpy(rBuffer, buffer + 0x2, 0x8);
            memcpy(rBuffer + 0x8, buffer + 0xA, 0x10);
            memcpy(rBuffer + 0x18, buffer + 0x20, 0xA);
            *(rBuffer + 0x22) = (uint8_t)fwdlusb_getFirmwareVersion(buffer + 0x1A, 0x2);
            *(rBuffer + 0x23) = (uint8_t)fwdlusb_getFirmwareVersion(buffer + 0x1C, 0x2);
            memcpy(rBuffer + 0x24, buffer + 0x2A, 0x2);

            memcpy(mainFirmware, rBuffer, 0x40);

            fwFile = CreateFileW(printer_config.main_fw_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (fwFile != NULL) {
                WriteFile(fwFile, rBuffer, 0x40, &bytesWritten, NULL);
                CloseHandle(fwFile);
            }

            if (rResult) *rResult = 0;
            result = 1;
        } else {
            if (rResult) *rResult = 1005;
            result = 0;
        }
    } else {
        if (rResult) *rResult = 1006;
        result = 0;
    }

    return result;
}

int WINAPI fwdlusb_updateFirmware_dsp(uint8_t update, LPCSTR filename, uint16_t *rResult) {
    DWORD result;
    HANDLE fwFile = NULL;
    DWORD bytesWritten = 0;

    if (filename) {
        HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return 0;
        {
            if (rResult) *rResult = 1005;
            result = 0;
        }

        DWORD read;
        uint8_t buffer[0x40];
        result = ReadFile(hFile, buffer, 0x40, &read, NULL);
        CloseHandle(hFile);
        if (result && read > 0x24) {
            uint8_t rBuffer[0x40] = {0};

            memcpy(rBuffer, buffer + 0x2, 8);
            memcpy(rBuffer + 0x8, buffer + 0xA, 0x10);
            memcpy(rBuffer + 0x18, buffer + 0x20, 0xA);
            memcpy(rBuffer + 0x22, buffer + 0x1A, 0x1);
            memcpy(rBuffer + 0x23, buffer + 0x1C, 0x1);
            memcpy(rBuffer + 0x24, buffer + 0x2A, 0x2);

            memcpy(dspFirmware, rBuffer, 0x40);

            fwFile = CreateFileW(printer_config.dsp_fw_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (fwFile != NULL) {
                WriteFile(fwFile, rBuffer, 0x40, &bytesWritten, NULL);
                CloseHandle(fwFile);
            }

            if (rResult) *rResult = 0;
            result = 1;
        } else {
            if (rResult) *rResult = 1005;
            result = 0;
        }
    } else {
        if (rResult) *rResult = 1006;
        result = 0;
    }

    return result;
}

int WINAPI fwdlusb_updateFirmware_param(uint8_t update, LPCSTR filename, uint16_t *rResult) {
    DWORD result;
    HANDLE fwFile = NULL;
    DWORD bytesWritten = 0;

    if (filename) {
        HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return 0;
        {
            if (rResult) *rResult = 1005;
            result = 0;
        }

        DWORD read;
        uint8_t buffer[0x40];
        result = ReadFile(hFile, buffer, 0x40, &read, NULL);
        CloseHandle(hFile);
        if (result && read > 0x24) {
            uint8_t rBuffer[0x40] = {0};

            memcpy(rBuffer, buffer + 0x2, 8);
            memcpy(rBuffer + 0x8, buffer + 0xA, 0x10);
            memcpy(rBuffer + 0x18, buffer + 0x20, 0xA);
            memcpy(rBuffer + 0x22, buffer + 0x1A, 0x1);
            memcpy(rBuffer + 0x23, buffer + 0x1C, 0x1);
            memcpy(rBuffer + 0x24, buffer + 0x2A, 0x2);

            memcpy(paramFirmware, rBuffer, 0x40);

            fwFile = CreateFileW(printer_config.param_fw_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (fwFile != NULL) {
                WriteFile(fwFile, rBuffer, 0x40, &bytesWritten, NULL);
                CloseHandle(fwFile);
            }

            if (rResult) *rResult = 0;
            result = 1;
        } else {
            if (rResult) *rResult = 1005;
            result = 0;
        }
    } else {
        if (rResult) *rResult = 1006;
        result = 0;
    }

    return result;
}

int WINAPI fwdlusb_updateFirmware(uint8_t update, LPCSTR filename, uint16_t *rResult) {
    dprintf("Printer: C3XXFWDLusb: %s\n", __func__);
    if (update == 1) {
        return fwdlusb_updateFirmware_main(update, filename, rResult);
    } else if (update == 2) {
        return fwdlusb_updateFirmware_dsp(update, filename, rResult);
    } else if (update == 3) {
        return fwdlusb_updateFirmware_param(update, filename, rResult);
    } else {
        *rResult = 0;
        return 1;
    }
}

int WINAPI fwdlusb_getFirmwareInfo_main(LPCSTR filename, uint8_t *rBuffer, uint32_t *rLen, uint16_t *rResult) {
    DWORD result;

    if (filename) {
        HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return 0;
        {
            if (rResult) *rResult = 1005;
            result = 0;
        }

        DWORD read;
        uint8_t buffer[0x40];
        result = ReadFile(hFile, buffer, 0x40, &read, NULL);
        if (result && read > 0x24) {
            memcpy(rBuffer, buffer + 0x2, 0x8);
            memcpy(rBuffer + 0x8, buffer + 0xA, 0x10);
            memcpy(rBuffer + 0x18, buffer + 0x20, 0xA);
            *(rBuffer + 0x22) = (uint8_t)fwdlusb_getFirmwareVersion(buffer + 0x1A, 0x2);
            *(rBuffer + 0x23) = (uint8_t)fwdlusb_getFirmwareVersion(buffer + 0x1C, 0x2);
            memcpy(rBuffer + 0x24, buffer + 0x2A, 0x2);

            if (rResult) *rResult = 0;
            result = 1;
        } else {
            if (rResult) *rResult = 1005;
            result = 0;
        }
    } else {
        if (rResult) *rResult = 1006;
        result = 0;
    }

    return result;
}

int WINAPI fwdlusb_getFirmwareInfo_dsp(LPCSTR filename, uint8_t *rBuffer, uint32_t *rLen, uint16_t *rResult) {
    DWORD result;

    if (filename) {
        HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return 0;
        {
            if (rResult) *rResult = 1005;
            result = 0;
        }

        DWORD read;
        uint8_t buffer[0x40];
        result = ReadFile(hFile, buffer, 0x40, &read, NULL);
        if (result && read > 0x24) {
            memcpy(rBuffer, buffer + 0x2, 8);
            memcpy(rBuffer + 0x8, buffer + 0xA, 0x10);
            memcpy(rBuffer + 0x18, buffer + 0x20, 0xA);
            memcpy(rBuffer + 0x22, buffer + 0x1A, 0x1);
            memcpy(rBuffer + 0x23, buffer + 0x1C, 0x1);
            memcpy(rBuffer + 0x24, buffer + 0x2A, 0x2);

            if (rResult) *rResult = 0;
            result = 1;
        } else {
            if (rResult) *rResult = 1005;
            result = 0;
        }
    } else {
        if (rResult) *rResult = 1006;
        result = 0;
    }

    return result;
}

int WINAPI fwdlusb_getFirmwareInfo_param(LPCSTR filename, uint8_t *rBuffer, uint32_t *rLen, uint16_t *rResult) {
    DWORD result;

    if (filename) {
        HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return 0;
        {
            if (rResult) *rResult = 1005;
            result = 0;
        }

        DWORD read;
        uint8_t buffer[0x40];
        result = ReadFile(hFile, buffer, 0x40, &read, NULL);
        if (result && read > 0x24) {
            memcpy(rBuffer, buffer + 0x2, 8);
            memcpy(rBuffer + 0x8, buffer + 0xA, 0x10);
            memcpy(rBuffer + 0x18, buffer + 0x20, 0xA);
            memcpy(rBuffer + 0x22, buffer + 0x1A, 0x1);
            memcpy(rBuffer + 0x23, buffer + 0x1C, 0x1);
            memcpy(rBuffer + 0x24, buffer + 0x2A, 0x2);

            if (rResult) *rResult = 0;
            result = 1;
        } else {
            if (rResult) *rResult = 1005;
            result = 0;
        }
    } else {
        if (rResult) *rResult = 1006;
        result = 0;
    }

    return result;
}

int WINAPI fwdlusb_getFirmwareInfo(uint8_t update, LPCSTR filename, uint8_t *rBuffer, uint32_t *rLen, uint16_t *rResult) {
    dprintf("Printer: C3XXFWDLusb: %s(update: %d, filename: %s)\n", __func__, update, filename);
    if (!rBuffer) {
        *rLen = 38;
        return 1;
    }
    if (*rLen > 38) *rLen = 38;
    if (update == 1) {
        return fwdlusb_getFirmwareInfo_main(filename, rBuffer, rLen, rResult);
    } else if (update == 2) {
        return fwdlusb_getFirmwareInfo_dsp(filename, rBuffer, rLen, rResult);
    } else if (update == 3) {
        return fwdlusb_getFirmwareInfo_param(filename, rBuffer, rLen, rResult);
    } else {
        if (rResult) *rResult = 0;
        return 1;
    }
}

int WINAPI fwdlusb_MakeThread(uint16_t maxCount) {
    dprintf("Printer: C3XXFWDLusb: %s\n", __func__);
    return 1;
}

int WINAPI fwdlusb_ReleaseThread(uint16_t *rResult) {
    dprintf("Printer: C3XXFWDLusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI fwdlusb_AttachThreadCount(uint16_t *rCount, uint16_t *rMaxCount) {
    dprintf("Printer: C3XXFWDLusb: %s\n", __func__);
    *rCount = 0;
    *rMaxCount = 1;
    return 1;
}

int WINAPI fwdlusb_getErrorLog(uint16_t index, uint8_t *rData, uint16_t *rResult) {
    dprintf("Printer: C3XXFWDLusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

// C3XXusb stubs

int WINAPI chcusb_MakeThread(uint16_t maxCount) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    return 1;
}

int WINAPI chcusb_open(uint16_t *rResult) {
    // Seed random for card id generation
    srand(time(NULL));
    generate_rfid();

    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

void chcusb_close() {
    dprintf("Printer: C3XXusb: %s\n", __func__);
}

int WINAPI chcusb_ReleaseThread(uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    return 1;
}

int WINAPI chcusb_listupPrinter(uint8_t *rIdArray) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    memset(rIdArray, 0xFF, 0x80);
    rIdArray[0] = idNumber;
    return 1;
}

int WINAPI chcusb_listupPrinterSN(uint64_t *rSerialArray) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    memset(rSerialArray, 0xFF, 0x400);
    rSerialArray[0] = serialNo;
    return 1;
}

int WINAPI chcusb_selectPrinter(uint8_t printerId, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_selectPrinterSN(uint64_t printerSN, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_getPrinterInfo(uint16_t tagNumber, uint8_t *rBuffer, uint32_t *rLen) {
    // dprintf("Printer: C3XXusb: %s\n", __func__);

    switch (tagNumber) {
        // getPaperInfo
        case 0:
            if (*rLen != 0x67) *rLen = 0x67;
            if (rBuffer) memset(rBuffer, 0, *rLen);
            break;

        case 3:  // getFirmwareVersion
            if (*rLen != 0x99) *rLen = 0x99;
            if (rBuffer) {
                memset(rBuffer, 0, *rLen);
                rBuffer[0] = 4; // firmware count
                // bootFirmware
                int i = 1;
                memcpy(rBuffer + i, mainFirmware, sizeof(mainFirmware));
                // mainFirmware
                i += 0x26;
                memcpy(rBuffer + i, mainFirmware, sizeof(mainFirmware));
                // printParameterTable
                i += 0x26;
                memcpy(rBuffer + i, paramFirmware, sizeof(paramFirmware));
                // dspFirmware (C300 only)
                i += 0x26;
                memcpy(rBuffer + i, dspFirmware, 0x26);
            }
            break;

        case 4:  // getPrintCountInfo (C300 only)
            if (!rBuffer) {
                *rLen = 0x1C;
                return 1;
            }
            int32_t bInfoC300[10] = {0};
            bInfoC300[0] = 22;   // printCounter0
            bInfoC300[1] = 23;   // printCounter1
            bInfoC300[2] = 33;   // feedRollerCount
            bInfoC300[3] = 55;   // cutterCount
            bInfoC300[4] = 88;   // headCount
            bInfoC300[5] = 999;  // ribbonRemain
            bInfoC300[6] = 0;    // dummy
            if (*rLen <= 0x1Cu) {
                memcpy(rBuffer, bInfoC300, *rLen);
            } else {
                bInfoC300[7] = 0;  // TODO
                if (*rLen > 0x20u) *rLen = 0x20;
                memcpy(rBuffer, bInfoC300, *rLen);
            }
            break;

        case 5: // getPrintCountInfo2 (C310A and later)
            if (!rBuffer) {
                *rLen = 0x28;
                return 1;
            }
            int32_t bInfo[10] = {0};
            bInfo[0] = 22;   // printCounter0
            bInfo[1] = 23;   // printCounter1
            bInfo[2] = 33;   // feedRollerCount
            bInfo[3] = 55;   // cutterCount
            bInfo[4] = 88;   // headCount
            bInfo[5] = 999;  // ribbonRemain
            bInfo[6] = 99;   // holoHeadCount
            if (*rLen <= 0x1Cu) {
                memcpy(rBuffer, bInfo, *rLen);
            } else {
                bInfo[7] = 69;  // paperCount
                bInfo[8] = 21;  // printCounter2
                bInfo[9] = 20;  // holoPrintCounter
                if (*rLen > 0x28u) *rLen = 0x28;
                memcpy(rBuffer, bInfo, *rLen);
            }
            break;

        case 6:  // pageStatusInfo
            *rLen = 32;
            if (rBuffer) {
                memset(rBuffer, 0, 32);
                rBuffer[0] = 88; // holoRemain
                rBuffer[1] = 0;
                rBuffer[2] = 0;
                rBuffer[3] = 0;
            }
            break;

        case 7:  // svc
            *rLen = 2;
            if (rBuffer) {
                memset(rBuffer, 0, 2);
                rBuffer[0] = 0;  // mainError
                rBuffer[1] = 0;  // subError
            }
            break;

        case 8:  // printStandby
            *rLen = 1;
            if (awaitingCardExit)
                *rBuffer = 0xF0;
            else if (STATUS == 1)
                *rBuffer = 1;
            else
                *rBuffer = 0;
            break;

        case 16:  // memoryInfo
            *rLen = 18;
            if (rBuffer) memset(rBuffer, 0, 18);
            break;

        case 20:  // printMode
            dprintf("Printer: C3XXusb: Unimpl tagNumber 20\n");
            break;

        case 26:  // getPrinterSerial
            if (*rLen != 8) *rLen = 8;
            if (rBuffer) memcpy(rBuffer, &serialNo, 8);
            break;

        case 30:  // TODO
            dprintf("Printer: C3XXusb: Unimpl tagNumber 30\n");
            break;

        case 31:  // TODO, possibly CardRFIDCheck?
            *rLen = 1;
            if (rBuffer) memset(rBuffer, 0, 1);
            break;

        case 40:  // temperature
            *rLen = 10;
            if (rBuffer) {
                memset(rBuffer, 0, 10);
                rBuffer[0] = 1;
                rBuffer[1] = 2;
                rBuffer[2] = 3;
            }
            break;

        case 50:  // errHistory
            *rLen = 61;
            if (rBuffer) memset(rBuffer, 0, 61);
            break;

        case 60:  // getToneTable
            *rLen = 6;
            if (rBuffer) memset(rBuffer, 0, 6);
            break;

        default:
            dprintf("Printer: unknown tagNumber, %d", tagNumber);
            break;
    }

    return 1;
}

int WINAPI chcusb_imageformat(
        uint16_t format,
        uint16_t ncomp,
        uint16_t depth,
        uint16_t width,
        uint16_t height,
        uint8_t *image,
        uint16_t *rResult) {
    return chcusb_imageformat_330(format, ncomp, depth, width, height, rResult);
}

int WINAPI chcusb_imageformat_330(
    uint16_t format,
    uint16_t ncomp,
    uint16_t depth,
    uint16_t width,
    uint16_t height,
    uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);

    WIDTH = width;
    HEIGHT = height;

    *rResult = 0;
    return 1;
}

int __thiscall chcusb_setmtf(int32_t *mtf) {
    dprintf("Printer: C3XXusb: %s\n", __func__);

    memcpy(MTF, mtf, sizeof(MTF));
    return 1;
}

int WINAPI chcusb_makeGamma(uint16_t k, uint8_t *intoneR, uint8_t *intoneG, uint8_t *intoneB) {
    dprintf("Printer: C3XXusb: %s\n", __func__);

    uint8_t tone;
    int32_t value;
    double power;

    double factor = (double)k / 100.0;

    for (int i = 0; i < 256; ++i) {
        power = pow((double)i, factor);
        value = (int)(power / pow(255.0, factor) * 255.0);

        if (value > 255)
            tone = 255;
        if (value >= 0)
            tone = value;
        else
            tone = 0;

        if (intoneR)
            *(uint8_t *)(intoneR + i) = tone;
        if (intoneG)
            *(uint8_t *)(intoneG + i) = tone;
        if (intoneB)
            *(uint8_t *)(intoneB + i) = tone;
    }

    return 1;
}

int WINAPI chcusb_setIcctable(
    LPCSTR icc1,
    LPCSTR icc2,
    uint16_t intents,
    uint8_t *intoneR,
    uint8_t *intoneG,
    uint8_t *intoneB,
    uint8_t *outtoneR,
    uint8_t *outtoneG,
    uint8_t *outtoneB,
    uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);

    if (intoneR != NULL && intoneG != NULL && intoneB != NULL && outtoneR != NULL && outtoneG != NULL && outtoneB != NULL) {
        for (int i = 0; i < 256; ++i) {
            intoneR[i] = i;
            intoneG[i] = i;
            intoneB[i] = i;
            outtoneR[i] = i;
            outtoneG[i] = i;
            outtoneB[i] = i;
        }
    }

    *rResult = 0;
    return 1;
}

int WINAPI chcusb_copies(uint16_t copies, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_status(uint16_t *rResult) {
    // dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_statusAll(uint8_t *idArray, uint16_t *rResultArray) {
    dprintf("Printer: C3XXusb: %s\n", __func__);

    for (int i = 0; *(uint8_t *)(idArray + i) != 255 && i < 128; ++i) {
        *(uint16_t *)(rResultArray + 2 * i) = 0;
    }

    return 1;
}

int WINAPI chcusb_startpage(uint16_t postCardState, uint16_t *pageId, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);

    STATUS = 2;

    *pageId = 1;
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_startpage_300(uint16_t postCardState, uint16_t *rResult) {
    dprintf("Printer: C300usb: %s\n", __func__);

    STATUS = 2;

    *rResult = 0;
    return 1;
}

int WINAPI chcusb_endpage(uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);

    awaitingCardExit = true;

    *rResult = 0;
    return 1;
}

int WINAPI chcusb_write(uint8_t *data, uint32_t *writeSize, uint16_t *rResult) {
    SYSTEMTIME t;
    GetLocalTime(&t);

    wchar_t dumpPath[MAX_PATH];
    uint8_t metadata[44];
    swprintf_s(
        dumpPath, MAX_PATH,
        L"%s\\C3XX_%04d%02d%02d_%02d%02d%02d.bmp",
        printer_out_path, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);

    memcpy(metadata, cardRFID, 12);
    memcpy(metadata + 12, cardDataBuffer, 32);

    WriteDataToBitmapFile(dumpPath, 24, WIDTH, HEIGHT, data, *writeSize, metadata, 44, rotate180);
    // WriteArrayToFile(dumpPath, data, IMAGE_SIZE, FALSE);
    dprintf("Printer: C3XXusb: %s\n", __func__);
    dwprintf(L"Printer: File written: %s\n", dumpPath);

    *rResult = 0;

    return 1;
}

int WINAPI chcusb_writeLaminate(uint8_t *data, uint32_t *writeSize, uint16_t *rResult) {
    SYSTEMTIME t;
    GetLocalTime(&t);

    wchar_t dumpPath[MAX_PATH];
    swprintf_s(
        dumpPath, MAX_PATH,
        L"%s\\C3XX_%04d%02d%02d_%02d%02d%02d_laminate.bmp",
        printer_out_path, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);

    // WriteDataToBitmapFile(dumpPath, 8, WIDTH, HEIGHT, data, HOLO_SIZE, NULL, 0, rotate180);
    dprintf("Printer: C3XXusb: %s\n", __func__);

    // *writeSize = written;
    *rResult = 0;

    return 1;
}

int WINAPI chcusb_writeHolo(uint8_t *data, uint32_t *writeSize, uint16_t *rResult) {
    SYSTEMTIME t;
    GetLocalTime(&t);

    wchar_t dumpPath[MAX_PATH];
    swprintf_s(
        dumpPath, MAX_PATH,
        L"%s\\C3XX_%04d%02d%02d_%02d%02d%02d_holo.bmp",
        printer_out_path, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);

    WriteDataToBitmapFile(dumpPath, 8, WIDTH, HEIGHT, data, HOLO_SIZE, NULL, 0, rotate180);
    dprintf("Printer: C3XXusb: %s\n", __func__);

    *writeSize = HOLO_SIZE;
    *rResult = 0;

    return 1;
}

int WINAPI chcusb_setPrinterInfo(uint16_t tagNumber, uint8_t *rBuffer, uint32_t *rLen, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);

    switch (tagNumber) {
        case 0:  // setPaperInfo
            memcpy(PAPERINFO, rBuffer, sizeof(PAPERINFO));
            break;
        case 20:  // setPolishInfo
            memcpy(POLISH, rBuffer, sizeof(POLISH));
            break;
        default:
            break;
    }

    *rResult = 0;
    return 1;
}

int WINAPI chcusb_getGamma(LPCSTR filename, uint8_t *r, uint8_t *g, uint8_t *b, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);

    for (int i = 0; i < 256; ++i) {
        r[i] = i;
        g[i] = i;
        b[i] = i;
    }

    *rResult = 0;
    return 1;
}

int WINAPI chcusb_getMtf(LPCSTR filename, int32_t *mtf, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);

    HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return 0;

    DWORD read;
    uint8_t buffer[0x40];
    BOOL result = ReadFile(hFile, buffer, 0x40, &read, NULL);
    if (!result) return 0;

    int a, c;
    int b = 1;
    int d = c = 0;

    memset(mtf, 0, sizeof(MTF));

    for (DWORD i = 0; i < read; i++) {
        a = buffer[i] - 0x30;
        if (a == -3 && c == 0) {
            b = -1;
        } else if (a >= 0 && a <= 9) {
            mtf[d] = mtf[d] * 10 + a;
            c++;
        } else if (c > 0) {
            mtf[d] *= b;
            b = 1;
            c = 0;
            d++;
        }
        if (d > 9) break;
    }

    *rResult = 0;
    return 1;
}

int WINAPI chcusb_cancelCopies(uint16_t pageId, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_setPrinterToneCurve(uint16_t type, uint16_t number, uint16_t *data, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    if ((type > 0 && type < 3) && (number > 0 && number < 3)) {
        CURVE[type][number] = *data;
    }
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_getPrinterToneCurve(uint16_t type, uint16_t number, uint16_t *data, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    if ((type > 0 && type < 3) && (number > 0 && number < 3)) {
        *data = CURVE[type][number];
    }
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_blinkLED(uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_resetPrinter(uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_AttachThreadCount(uint16_t *rCount, uint16_t *rMaxCount) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rCount = 0;
    *rMaxCount = 1;
    return 1;
}

int WINAPI chcusb_getPrintIDStatus(uint16_t pageId, uint8_t *rBuffer, uint16_t *rResult) {
    // dprintf("Printer: C3XXusb: %s\n", __func__);
    memset(rBuffer, 0, 8);

    if (STATUS > 1)
    {
        STATUS = 0;
        *((uint16_t*)(rBuffer + 6)) = 2212;
    }
    else
    {
        *((uint16_t*)(rBuffer + 6)) = 2300;
    }

    *rResult = 0;
    return 1;
}

int WINAPI chcusb_setPrintStandby(uint16_t position, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);

    if (STATUS == 0)
    {
        STATUS = 1;
        *rResult = 2100;
    }
    else
    {
        *rResult = 0;
    }

    return 1;
}

int WINAPI chcusb_setPrintStandby_300(uint16_t *rResult) {
    dprintf("Printer: C300usb: %s\n", __func__);

    *rResult = 0;
    if (awaitingCardExit){ // 300 does not use exitCard, so reset this for getPrinterInfo.
        awaitingCardExit = false;
        STATUS = 1;
    }
    if (STATUS == 0)
    {
        STATUS = 1;
    }

    return 1;
}

int WINAPI chcusb_testCardFeed(uint16_t mode, uint16_t times, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_exitCard(uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);

    awaitingCardExit = false;
    generate_rfid();

    *rResult = 0;
    return 1;
}

int WINAPI chcusb_getCardRfidTID(uint8_t *rCardTID, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);

    memcpy(rCardTID, cardRFID, sizeof(cardRFID));
    *rResult = 2405;
    return 1;
}

int WINAPI chcusb_commCardRfidReader(uint8_t *sendData, uint8_t *rRecvData, uint32_t sendSize, uint32_t *rRecvSize, uint16_t *rResult) {
    uint8_t off;

    dprintf("Printer: C3XXusb: %s\n", __func__);
    // dprintf("Printer: C3XXusb: commCardRfidReader send buffer: \n");
    // dump(sendData, sendSize);

    *rResult = 0;

    switch (sendData[0]) {
        case 0x02:
            memset(rRecvData, 0, 35);
            rRecvData[0] = 2;
            rRecvData[2] = 32;
            memcpy(rRecvData + 3, cardDataBuffer, 32);
            *rRecvSize = 35;
            break;
        case 0x03:
            off = (sendData[4] - 1) * 2;
            cardDataBuffer[off] = sendData[5];
            cardDataBuffer[off + 1] = sendData[6];
            if (sendData[4] == 0x10) {
                dprintf("Printer: C3XXusb: commCardRfidReader card data buffer: \n");
                dump(cardDataBuffer, 0x20);
            }
            memset(rRecvData, 0, 3);
            rRecvData[0] = 0x03;
            *rRecvSize = 3;
            break;
        case 0x84:
            memset(rRecvData, 0, 4);
            rRecvData[0] = 0x84;
            rRecvData[2] = 1;
            rRecvData[3] = 0x90;
            *rRecvSize = 4;
            break;
        case 0x85:
            memset(rRecvData, 0, 12);
            rRecvData[0] = 0x85;
            rRecvData[2] = 9;
            memcpy(rRecvData + 3, (void *)"837-15345", 9);
            *rRecvSize = 12;
            break;
        case 0x42:
            memset(rRecvData, 0, 4);
            rRecvData[0] = 0x42;
            rRecvData[2] = 1;
            rRecvData[3] = 0x91;
            *rRecvSize = 4;
            break;
        default:
            memset(rRecvData, 0, 3);
            rRecvData[0] = sendData[0];
            *rRecvSize = 3;
            break;
    }

    // dprintf("Printer: C3XXusb: commCardRfidReader recv buffer: \n");
    // dump(rRecvData, *rRecvSize);

    return 1;
}

int WINAPI chcusb_updateCardRfidReader(uint8_t *data, uint32_t size, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_getErrorLog(uint16_t index, uint8_t *rData, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_getErrorStatus(uint16_t *rBuffer) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    memset(rBuffer, 0, 0x80);
    return 1;
}

int WINAPI chcusb_setCutList(uint8_t *rData, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_setLaminatePattern(uint16_t index, uint8_t *rData, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_color_adjustment(LPCSTR filename, int32_t a2, int32_t a3, int16_t a4, int16_t a5, int64_t a6, int64_t a7, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_color_adjustmentEx(int32_t a1, int32_t a2, int32_t a3, int16_t a4, int16_t a5, int64_t a6, int64_t a7, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_getEEPROM(uint8_t index, uint8_t *rData, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_setParameter(uint8_t a1, uint32_t a2, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_getParameter(uint8_t a1, uint8_t *a2, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

int WINAPI chcusb_universal_command(int32_t a1, uint8_t a2, int32_t a3, uint8_t *a4, uint16_t *rResult) {
    dprintf("Printer: C3XXusb: %s\n", __func__);
    *rResult = 0;
    return 1;
}

/* PrintDll hooks */

int CFW_AttachThreadCount(const void *handle, uint16_t *rCount, uint16_t *rMaxCount)
{
   return fwdlusb_AttachThreadCount(rCount, rMaxCount);
}

void CFW_close(const void *handle)
{
   fwdlusb_close();
}

int CFW_getErrorLog(const void *handle, uint16_t index, uint8_t *rData, uint16_t *rResult)
{
   return fwdlusb_getErrorLog(index, rData, rResult);
}

int CFW_getFirmwareInfo(const void *handle, uint8_t update, LPCSTR filename, uint8_t *rBuffer, uint32_t *rLen, uint16_t *rResult)
{
   return fwdlusb_getFirmwareInfo(update, filename, rBuffer, rLen, rResult);
}

int CFW_getPrinterInfo(const void *handle, uint16_t tagNumber, uint8_t *rBuffer, uint32_t *rLen)
{
   return fwdlusb_getPrinterInfo(tagNumber, rBuffer, rLen);
}

int CFW_init(LPCSTR dllpath)
{
    dprintf("Printer PrintDLL: %s (dllpath: %s)\n", __func__, dllpath);
    return 1;
}

int CFW_listupPrinter(const void *handle, uint8_t *rIdArray)
{
   return fwdlusb_listupPrinter(rIdArray);
}

int CFW_listupPrinterSN(const void *handle, uint64_t *rSerialArray)
{
   return fwdlusb_listupPrinterSN(rSerialArray);
}

int CFW_MakeThread(const void *handle, uint16_t maxCount)
{
   return fwdlusb_MakeThread(maxCount);
}

int CFW_open(const void *handle, uint16_t *rResult)
{
   return fwdlusb_open(rResult);
}

int CFW_ReleaseThread(const void *handle, uint16_t *rResult)
{
   return fwdlusb_ReleaseThread(rResult);
}

int CFW_resetPrinter(const void *handle, uint16_t *rResult)
{
   return fwdlusb_resetPrinter(rResult);
}

int CFW_selectPrinter(const void *handle, uint8_t printerId, uint16_t *rResult)
{
   return fwdlusb_selectPrinter(printerId, rResult);
}

int CFW_selectPrinterSN(const void *handle, uint64_t printerSN, uint16_t *rResult)
{
   return fwdlusb_selectPrinterSN(printerSN, rResult);
}

int CFW_status(const void *handle, uint16_t *rResult)
{
   return fwdlusb_status(rResult);
}

int CFW_statusAll(const void *handle, uint8_t *idArray, uint16_t *rResultArray)
{
   return fwdlusb_statusAll(idArray, rResultArray);
}

void CFW_term(const void *handle)
{
    dprintf("Printer PrintDLL: %s\n", __func__);
}

int CFW_updateFirmware(const void *handle, uint8_t update, LPCSTR filename, uint16_t *rResult)
{
   return fwdlusb_updateFirmware(update, filename, rResult);
}

int CHCUSB_AttachThreadCount(const void *handle, uint16_t *rCount, uint16_t *rMaxCount)
{
    return chcusb_AttachThreadCount(rCount, rMaxCount);
}

int CHCUSB_MakeThread(const void *handle, uint16_t maxCount)
{
    return chcusb_MakeThread(maxCount);
}

int CHCUSB_ReleaseThread(const void *handle, uint16_t *rResult)
{
    return chcusb_ReleaseThread(rResult);
}

int CHCUSB_blinkLED(const void *handle, uint16_t *rResult)
{
    return chcusb_blinkLED(rResult);
}

int CHCUSB_cancelCopies(const void *handle, uint16_t pageId, uint16_t *rResult)
{
    return chcusb_cancelCopies(pageId, rResult);
}

void CHCUSB_close(const void *handle)
{
    chcusb_close();
}

int CHCUSB_commCardRfidReader(const void *handle, uint8_t *sendData, uint8_t *rRecvData, uint32_t sendSize, uint32_t *rRecvSize, uint16_t *rResult)
{
    return chcusb_commCardRfidReader(sendData, rRecvData, sendSize, rRecvSize, rResult);
}

int CHCUSB_copies(const void *handle, uint16_t copies, uint16_t *rResult)
{
    return chcusb_copies(copies, rResult);
}

int CHCUSB_endpage(const void *handle, uint16_t *rResult)
{
    return chcusb_endpage(rResult);
}

int CHCUSB_exitCard(const void *handle, uint16_t *rResult)
{
    return chcusb_exitCard(rResult);
}

int CHCUSB_getCardRfidTID(const void *handle, uint8_t *rCardTID, uint16_t *rResult)
{
    return chcusb_getCardRfidTID(rCardTID, rResult);
}

int CHCUSB_getErrorLog(const void *handle, uint16_t index, uint8_t *rData, uint16_t *rResult)
{
    return chcusb_getErrorLog(index, rData, rResult);
}

int CHCUSB_getErrorStatus(const void *handle, uint16_t *rBuffer)
{
    return chcusb_getErrorStatus(rBuffer);
}

int CHCUSB_getGamma(const void *handle, LPCSTR filename, uint8_t *r, uint8_t *g, uint8_t *b, uint16_t *rResult)
{
    return chcusb_getGamma(filename, r, g, b, rResult);
}

int CHCUSB_getMtf(const void *handle, LPCSTR filename, int32_t *mtf, uint16_t *rResult)
{
    return chcusb_getMtf(filename, mtf, rResult);
}

int CHCUSB_getPrintIDStatus(const void *handle, uint16_t pageId, uint8_t *rBuffer, uint16_t *rResult)
{
    return chcusb_getPrintIDStatus(pageId, rBuffer, rResult);
}

int CHCUSB_getPrinterInfo(const void *handle, uint16_t tagNumber, uint8_t *rBuffer, uint32_t *rLen)
{
    return chcusb_getPrinterInfo(tagNumber, rBuffer, rLen);
}

int CHCUSB_getPrinterToneCurve(const void *handle, uint16_t type, uint16_t number, uint16_t *data, uint16_t *rResult)
{
    return chcusb_getPrinterToneCurve(type, number, data, rResult);
}

int CHCUSB_imageformat(const void *handle, uint16_t format, uint16_t ncomp, uint16_t depth, uint16_t width, uint16_t height, uint8_t *inputImage, uint16_t *rResult)
{
    return chcusb_imageformat(format, ncomp, depth, width, height, inputImage, rResult);
}

int CHCUSB_init(LPCSTR dllpath)
{
    dprintf("Printer PrintDLL: %s (dllpath: %s)\n", __func__, dllpath);
    return 1;
}

int CHCUSB_listupPrinter(const void *handle, uint8_t *rIdArray)
{
    return chcusb_listupPrinter(rIdArray);
}

int CHCUSB_listupPrinterSN(const void *handle, uint64_t *rSerialArray)
{
    return chcusb_listupPrinterSN(rSerialArray);
}

int CHCUSB_makeGamma(const void *handle, uint16_t k, uint8_t *intoneR, uint8_t *intoneG, uint8_t *intoneB)
{
    return chcusb_makeGamma(k, intoneR, intoneG, intoneB);
}

int CHCUSB_open(const void *handle, uint16_t *rResult)
{
    return chcusb_open(rResult);
}

int CHCUSB_resetPrinter(const void *handle, uint16_t *rResult)
{
    return chcusb_resetPrinter(rResult);
}

int CHCUSB_selectPrinter(const void *handle, uint8_t printerId, uint16_t *rResult)
{
    return chcusb_selectPrinter(printerId, rResult);
}

int CHCUSB_selectPrinterSN(const void *handle, uint64_t printerSN, uint16_t *rResult)
{
    return chcusb_selectPrinterSN(printerSN, rResult);
}

int CHCUSB_setIcctableProfile(const void *handle, LPCSTR icc1, LPCSTR icc2, uint16_t intents, uint16_t *rResult)
{
    dprintf("Printer PrintDLL: %s (icc1: %s, icc2: %s)\n",
            __func__, icc1, icc2);
    *rResult = 0;
    return 1;
}

int CHCUSB_setIcctable(const void *handle, uint16_t intents, uint8_t *intoneR, uint8_t *intoneG, uint8_t *intoneB, uint8_t *outtoneR, uint8_t *outtoneG, uint8_t *outtoneB, uint16_t *rResult)
{
    return chcusb_setIcctable("CFW", "CFW", intents, intoneR, intoneG, intoneB, outtoneR, outtoneG, outtoneB, rResult);
}

int CHCUSB_setPrintStandby(const void *handle, uint16_t position, uint16_t *rResult)
{
    return chcusb_setPrintStandby(position, rResult);
}

int CHCUSB_setPrinterInfo(const void *handle, uint16_t tagNumber, uint8_t *rBuffer, uint32_t *rLen, uint16_t *rResult)
{
    return chcusb_setPrinterInfo(tagNumber, rBuffer, rLen, rResult);
}

int CHCUSB_setPrinterToneCurve(const void *handle, uint16_t type, uint16_t number, uint16_t *data, uint16_t *rResult)
{
    return chcusb_setPrinterToneCurve(type, number, data, rResult);
}

int CHCUSB_setmtf(const void *handle, int32_t *mtf)
{
    return chcusb_setmtf(mtf);
}

int CHCUSB_startpage(const void *handle, uint16_t postCardState, uint16_t *pageId, uint16_t *rResult)
{
    return chcusb_startpage(postCardState, pageId, rResult);
}

int CHCUSB_status(const void *handle, uint16_t *rResult)
{
    return chcusb_status(rResult);
}

int CHCUSB_statusAll(const void *handle, uint8_t *idArray, uint16_t *rResultArray)
{
    return chcusb_statusAll(idArray, rResultArray);
}

int CHCUSB_testCardFeed(const void *handle, uint16_t mode, uint16_t times, uint16_t *rResult)
{
    return chcusb_testCardFeed(mode, times, rResult);
}

void CHCUSB_term(const void *handle)
{
    dprintf("Printer PrintDLL: %s\n", __func__);
}

int CHCUSB_updateCardRfidReader(const void *handle, uint8_t *data, uint32_t size, uint16_t *rResult)
{
    return chcusb_updateCardRfidReader(data, size, rResult);
}

int CHCUSB_write(const void *handle, uint8_t *data, uint32_t offset, uint32_t *writeSize, uint16_t *rResult)
{
    return chcusb_write(data, writeSize, rResult);
}

int CHCUSB_writeHolo(const void *handle, uint8_t *data, uint32_t offset, uint32_t *writeSize, uint16_t *rResult)
{
    return chcusb_writeHolo(data, writeSize, rResult);
}

int CHCUSB_writeLaminate(const void *handle, uint8_t *data, uint32_t offset, uint32_t *writeSize, uint16_t *rResult)
{
    return chcusb_writeLaminate(data, writeSize, rResult);
}

// copy pasted from https://dev.s-ul.net/domeori/c310emu
#define BITMAPHEADERSIZE 0x36

DWORD ConvertDataToBitmap(
    DWORD dwBitCount,
    DWORD dwWidth, DWORD dwHeight,
    PBYTE pbInput, DWORD cbInput,
    PBYTE pbOutput, DWORD cbOutput,
    PDWORD pcbResult,
    bool pFlip) {
    if (!pbInput || !pbOutput || dwBitCount < 8) return -3;

    if (cbInput < (dwWidth * dwHeight * dwBitCount / 8)) return -3;

    PBYTE pBuffer = (PBYTE)malloc(cbInput);
    if (!pBuffer) return -2;

    BYTE dwColors = (BYTE)(dwBitCount / 8);
    if (!dwColors) return -1;

    UINT16 cbColors;
    RGBQUAD pbColors[256];

    switch (dwBitCount) {
        case 1:
            cbColors = 1;
            break;
        case 2:
            cbColors = 4;
            break;
        case 4:
            cbColors = 16;
            break;
        case 8:
            cbColors = 256;
            break;
        default:
            cbColors = 0;
            break;
    }

    if (cbColors) {
        BYTE dwStep = (BYTE)(256 / cbColors);

        for (UINT16 i = 0; i < cbColors; ++i) {
            pbColors[i].rgbRed = dwStep * i;
            pbColors[i].rgbGreen = dwStep * i;
            pbColors[i].rgbBlue = dwStep * i;
            pbColors[i].rgbReserved = 0;
        }
    }

    DWORD dwTable = cbColors * sizeof(RGBQUAD);
    DWORD dwOffset = BITMAPHEADERSIZE + dwTable;

    // calculate the padded row size, again
    DWORD dwLineSize = (dwWidth * dwBitCount / 8 + 3) & ~3;

    BITMAPFILEHEADER bFile = {0};
    BITMAPINFOHEADER bInfo = {0};

    bFile.bfType = 0x4D42;  // MAGIC
    bFile.bfSize = dwOffset + cbInput;
    bFile.bfOffBits = dwOffset;

    bInfo.biSize = sizeof(BITMAPINFOHEADER);
    bInfo.biWidth = dwWidth;
    bInfo.biHeight = dwHeight;
    bInfo.biPlanes = 1;
    bInfo.biBitCount = (WORD)dwBitCount;
    bInfo.biCompression = BI_RGB;
    bInfo.biSizeImage = cbInput;

    if (cbOutput < bFile.bfSize) return -1;

    // Flip the image (if necessary) and add padding to each row
    if (pFlip) {
        for (size_t i = 0; i < dwHeight; i++) {
            for (size_t j = 0; j < dwWidth; j++) {
                for (size_t k = 0; k < dwColors; k++) {
                    // Calculate the position in the padded buffer
                    // Make sure to also flip the colors from RGB to BRG
                    size_t x = (dwHeight - i - 1) * dwLineSize + (dwWidth - j - 1) * dwColors + (dwColors - k - 1);
                    size_t y = (dwHeight - i - 1) * dwWidth * dwColors + j * dwColors + k;
                    *(pBuffer + x) = *(pbInput + y);
                }
            }
        }
    } else {
        for (size_t i = 0; i < dwHeight; i++) {
            for (size_t j = 0; j < dwWidth; j++) {
                for (size_t k = 0; k < dwColors; k++) {
                    // Calculate the position in the padded buffer
                    size_t x = i * dwLineSize + j * dwColors + (dwColors - k - 1);
                    size_t y = (dwHeight - i - 1) * dwWidth * dwColors + j * dwColors + k;
                    *(pBuffer + x) = *(pbInput + y);
                }
            }
        }
    }

    memcpy(pbOutput, &bFile, sizeof(BITMAPFILEHEADER));
    memcpy(pbOutput + sizeof(BITMAPFILEHEADER), &bInfo, sizeof(BITMAPINFOHEADER));
    if (cbColors) memcpy(pbOutput + BITMAPHEADERSIZE, pbColors, dwTable);
    memcpy(pbOutput + dwOffset, pBuffer, cbInput);

    *pcbResult = bFile.bfSize;

    free(pBuffer);
    return 0;
}

DWORD WriteDataToBitmapFile(
    LPCWSTR lpFilePath, DWORD dwBitCount,
    DWORD dwWidth, DWORD dwHeight,
    PBYTE pbInput, DWORD cbInput,
    PBYTE pbMetadata, DWORD cbMetadata,
    bool pFlip) {
    if (!lpFilePath || !pbInput) return -3;

    HANDLE hFile;
    DWORD dwBytesWritten;

    hFile = CreateFileW(
        lpFilePath,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE) return -1;

    // calculate the padded row size and padded image size
    DWORD dwLineSize = (dwWidth * dwBitCount / 8 + 3) & ~3;
    DWORD dwImageSize = dwLineSize * dwHeight;

    DWORD cbResult;
    DWORD cbBuffer = dwImageSize + 0x500;
    PBYTE pbBuffer = (PBYTE)calloc(cbBuffer, 1);
    if (!pbBuffer) return -2;

    if (ConvertDataToBitmap(dwBitCount, dwWidth, dwHeight, pbInput, dwImageSize, pbBuffer, cbBuffer, &cbResult, pFlip) < 0) {
        cbResult = -1;
        goto WriteDataToBitmapFile_End;
    }

    WriteFile(hFile, pbBuffer, cbResult, &dwBytesWritten, NULL);

    if (pbMetadata)
        WriteFile(hFile, pbMetadata, cbMetadata, &dwBytesWritten, NULL);

    CloseHandle(hFile);

    cbResult = dwBytesWritten;

WriteDataToBitmapFile_End:
    free(pbBuffer);
    return cbResult;
}

DWORD WriteArrayToFile(LPCSTR lpOutputFilePath, LPVOID lpDataTemp, DWORD nDataSize, BOOL isAppend) {
#ifdef NDEBUG

    return nDataSize;

#else

    HANDLE hFile;
    DWORD dwBytesWritten;
    DWORD dwDesiredAccess;
    DWORD dwCreationDisposition;

    if (isAppend) {
        dwDesiredAccess = FILE_APPEND_DATA;
        dwCreationDisposition = OPEN_ALWAYS;
    } else {
        dwDesiredAccess = GENERIC_WRITE;
        dwCreationDisposition = CREATE_ALWAYS;
    }

    hFile = CreateFileA(
        lpOutputFilePath,
        dwDesiredAccess,
        FILE_SHARE_READ,
        NULL,
        dwCreationDisposition,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    WriteFile(hFile, lpDataTemp, nDataSize, &dwBytesWritten, NULL);
    CloseHandle(hFile);

    return dwBytesWritten;

#endif
}

void printer_set_dimensions(int width, int height){
    WIDTH = width;
    HEIGHT = height;
}