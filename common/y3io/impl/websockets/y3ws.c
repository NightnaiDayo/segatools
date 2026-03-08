#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "3rdparty/cjson/cJSON.h"

#include "config.h"
#include "y3ws.h"

#include <wchar.h>

#include "hooklib/y3.h"

#include "winwebsocket.h"
#include "lib/base64.h"

#include "util/dprintf.h"
#include "util/env.h"

static void onopen(struct wws_connection*);
static void onclose(struct wws_connection*);
static void onmessage(struct wws_connection*, const char*, size_t);
static void onlog(const char*, ...);

#define PROTOCOL_VERSION 1
#define GAME_MAX_CARDS 32
#define MAX_CARDS 500
#define CARD_ID_LEN 5
#define OUTPUT_BUFFER_SIZE (1024 * 1024 * 5)

static struct y3ws_config cfg;

static bool is_initialized = false;
static struct CardInfo card_info[GAME_MAX_CARDS];
static int card_info_size = 0;
static CRITICAL_SECTION card_info_lock;

#pragma region y3-dll functions
uint16_t y3_io_get_api_version() {
    return 0x0100;
}

HRESULT y3_io_init() {
    if (is_initialized) {
        return S_FALSE;
    }

    y3ws_config_load(&cfg, get_config_path());

    memset(card_info, 0, sizeof(card_info));
    InitializeCriticalSection(&card_info_lock);

    wws_set_callbacks(onopen, onclose, onmessage, onlog);
    wws_set_verbose(cfg.debug);

    HRESULT hr = wws_start(cfg.port);

    if (SUCCEEDED(hr)) {
        dprintf("Y3WS: Started server on port %d\n", cfg.port);
        if (cfg.debug) {
            dprintf("Y3WS: WS debug logging is on\n");
        }
        is_initialized = true;
    } else {
        dprintf("Y3WS: Error starting server: %lx\n", hr);
    }

    return hr;
}

HRESULT y3_io_close() {

    HRESULT hr = wws_stop();

    DeleteCriticalSection(&card_info_lock);

    is_initialized = false;

    return hr;
}

HRESULT y3_io_get_cards(struct CardInfo* pCardInfo, int* numCards) {
    EnterCriticalSection(&card_info_lock);
    for (int i = 0; i < card_info_size; i++) {
        memcpy(&pCardInfo[i], &card_info[i], sizeof(struct CardInfo));
    }
    *numCards = card_info_size;
    LeaveCriticalSection(&card_info_lock);
    return S_OK;
}
#pragma endregion

#pragma region y3ws functions
static void y3ws_make_error_packet(const char* message, char* output_data, size_t* output_size) {
    dprintf("Y3WS: Error: %s\n", message);
    *output_size = sprintf_s((char*)output_data, *output_size, "{\"version\":%d, \"success\":false,\"error\":\"%s\"}", PROTOCOL_VERSION, message);
}

static void y3ws_make_success_packet(char* output_data, size_t* output_size) {
    *output_size = sprintf_s((char*)output_data, *output_size, "{\"version\":%d, \"success\":true}", PROTOCOL_VERSION);
}

static void y3ws_read_cards(char* output_data, size_t* output_size) {
    WIN32_FIND_DATAW ffd;

    wchar_t path[MAX_PATH];
    swprintf(path, MAX_PATH, L"%ls\\*", cfg.card_path);

    HANDLE hFind = FindFirstFileW(path, &ffd);
    if (INVALID_HANDLE_VALUE == hFind) {
        dprintf("Y3WS: Failed to access directory: %ls (%ld)\n", path, GetLastError());
        y3ws_make_error_packet("Could not read from card directory", output_data, output_size);
        return;
    }

    cJSON* response = cJSON_CreateObject();
    cJSON* pversion = cJSON_CreateNumber(PROTOCOL_VERSION);
    cJSON_AddItemToObject(response, "version", pversion);
    cJSON* success = cJSON_CreateBool(true);
    cJSON_AddItemToObject(response, "success", success);
    cJSON* cards = cJSON_CreateArray();
    cJSON_AddItemToObject(response, "cards", cards);

    int count = 0;
    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }

        swprintf(path, MAX_PATH, L"%ls\\%ls", cfg.card_path, ffd.cFileName);

        HANDLE hImage = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hImage == INVALID_HANDLE_VALUE) {
            dprintf("Y3WS: Error opening %ls: %ld\n", path, GetLastError());
            continue;
        }
        DWORD ret = SetFilePointer(hImage, -CARD_ID_LEN, NULL, FILE_END);
        if (ret == INVALID_SET_FILE_POINTER) {
            dprintf("Y3WS: Error seeking in %ls: %ld\n", path, GetLastError());
            continue;
        }
        uint8_t buf[CARD_ID_LEN];
        DWORD bytesRead = 0;
        if (!ReadFile(hImage, &buf, CARD_ID_LEN, &bytesRead, NULL) || bytesRead != CARD_ID_LEN) {
            dprintf("Y3WS: Error reading card ID from %ls: %ld\n", path, GetLastError());
            continue;
        }

        cJSON* card = cJSON_CreateObject();
        cJSON_AddItemToArray(cards, card);

        // cJSON isn't wide...
        char fpatha[MAX_PATH];
        size_t fpatha_len = 0;
        wcstombs_s(&fpatha_len, fpatha, MAX_PATH, ffd.cFileName, MAX_PATH);
        fpatha[fpatha_len] = 0;

        // ReSharper disable once CppRedundantCastExpression
        cJSON* path_str = cJSON_CreateString(fpatha);
        cJSON_AddItemToObject(card, "path", path_str);
        cJSON* card_id = cJSON_CreateNumber((double)((uint64_t)buf[0] >> 32 | (uint64_t)buf[1] >> 24 | (uint64_t)buf[2] >> 16 | (uint64_t)buf[3] >> 8 | buf[4]));
        cJSON_AddItemToObject(card, "card_id", card_id);

    } while (FindNextFileW(hFind, &ffd) != 0 && count++ < MAX_CARDS);

    cJSON_PrintPreallocated(response, output_data, (int)*output_size, false);
    *output_size = strlen(output_data);

    dprintf("Y3WS: Sent %d card(s) to the client\n", count);

    cJSON_Delete(response);
    FindClose(hFind);
}

static void y3ws_read_card_image(char* file_name, char* output_data, size_t* output_size) {
    wchar_t path[MAX_PATH];
    swprintf(path, MAX_PATH, L"%ls\\%hs", cfg.card_path, file_name);

    HANDLE hImage = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hImage == INVALID_HANDLE_VALUE) {
        dprintf("Y3WS: Failed to access file: %ls (%ld)\n", path, GetLastError());
        y3ws_make_error_packet("Could not read file", output_data, output_size);
        return;
    }

    DWORD size = GetFileSize(hImage, NULL);
    if (size == INVALID_FILE_SIZE) {
        dprintf("Y3WS: Failed to access file size: %ls (%ld)\n", path, GetLastError());
        y3ws_make_error_packet("Could not read file", output_data, output_size);
        return;
    }

    uint8_t* buf = malloc(size);
    size_t b64size = ((size * 4) / 3) + (size / 96) + 6;
    char* b64buf = malloc(b64size);
    if (buf == NULL) {
        dprintf("Y3WS: Allocation error for file: %ls (%ld)\n", path, GetLastError());
        y3ws_make_error_packet("Could not read file", output_data, output_size);
        goto end;
    }

    DWORD bytesRead = 0;
    if (!ReadFile(hImage, buf, size, &bytesRead, NULL) || bytesRead != size) {
        dprintf("Y3WS: File read failed: %ls (%ld)\n", path, GetLastError());
        y3ws_make_error_packet("Could not read file", output_data, output_size);
        goto end;
    }

    b64size = base64_encode(buf, size, b64buf);

    if (b64size + 100 > *output_size) {
        dprintf("Y3WS: Encoded file size exceeds buffer: %ls (%llu / %llu)\n", path, (uint64_t) b64size, (uint64_t) *output_size);
        y3ws_make_error_packet("File too large", output_data, output_size);
    }

    *output_size = sprintf_s((char*)output_data, *output_size, "{\"version\":%d, \"success\":true,\"data\":\"%s\"}", PROTOCOL_VERSION, (char*)b64buf);
end:
    free(buf);
    free(b64buf);
}

static void y3ws_set_cards_from_json(const cJSON* cards, char* output_data, size_t* output_size) {

    const cJSON* card = NULL;
    card_info_size = 0;

    EnterCriticalSection(&card_info_lock);
    memset(card_info, 0, sizeof(card_info));
    cJSON_ArrayForEach(card, cards) {
        cJSON* x = cJSON_GetObjectItemCaseSensitive(card, "x");
        cJSON* y = cJSON_GetObjectItemCaseSensitive(card, "y");
        cJSON* r = cJSON_GetObjectItemCaseSensitive(card, "rotation");
        cJSON* id = cJSON_GetObjectItemCaseSensitive(card, "card_id");

        if (cJSON_IsNumber(x) && cJSON_IsNumber(y) && cJSON_IsNumber(r) && cJSON_IsNumber(id)) {
            card_info[card_info_size].eCardStatus = VALID;
            card_info[card_info_size].fX = (float)x->valuedouble;
            card_info[card_info_size].fY = (float)y->valuedouble;
            card_info[card_info_size].fAngle = (float)r->valuedouble;
            card_info[card_info_size].uID = id->valueint;
        } else {
            dprintf("Y3WS: Invalid object in card array at index %d\n", card_info_size);
        }

        if (++card_info_size >= GAME_MAX_CARDS) {
            dprintf("Y3WS: too many cards specified, truncating!\n");
            break;
        }

    }
    LeaveCriticalSection(&card_info_lock);
    y3ws_make_success_packet(output_data, output_size);
}

static void y3ws_handle(const char* input_data, uint32_t input_length, char* output_data, size_t* output_size) {
    cJSON* json = cJSON_ParseWithLength(input_data, input_length);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();

        dprintf("Y3WS: Invalid JSON received!\n");
        dprintf("Y3WS: Message was: %s\n", input_data);
        dprintf("Y3WS: Error at: %s\n", error_ptr);
        y3ws_make_error_packet("Invalid JSON", output_data, output_size);
        goto end;
    }

    const cJSON* ver = cJSON_GetObjectItemCaseSensitive(json, "version");
    const cJSON* cmd = cJSON_GetObjectItemCaseSensitive(json, "command");

    if (!cJSON_IsNumber(ver) || ver->valueint <= 0) {
        y3ws_make_error_packet("Missing version attribute", output_data, output_size);
        goto end;
    }
    if (!cJSON_IsString(cmd)) {
        y3ws_make_error_packet("Missing command attribute", output_data, output_size);
        goto end;
    }
    if (ver->valueint != PROTOCOL_VERSION) {
        y3ws_make_error_packet("Incompatible version", output_data, output_size);
        goto end;
    }

    if (strcmp(cmd->valuestring, "ping") == 0) {
        y3ws_make_success_packet(output_data, output_size);
    } else if (strcmp(cmd->valuestring, "get_game_id") == 0) {
        *output_size = sprintf_s((char*)output_data, *output_size, "{\"version\":%d, \"success\":true,\"game_id\":\"%s\"}", PROTOCOL_VERSION, cfg.game_id);
    } else if (strcmp(cmd->valuestring, "get_cards") == 0) {
        y3ws_read_cards(output_data, output_size);
    } else if (strcmp(cmd->valuestring, "get_card_image") == 0) {
        const cJSON* path = cJSON_GetObjectItemCaseSensitive(json, "path");
        if (cJSON_IsString(path)) {
            y3ws_read_card_image(path->valuestring, output_data, output_size);
        } else {
            y3ws_make_error_packet("Missing attribute", output_data, output_size);
        }
    } else if (strcmp(cmd->valuestring, "set_field") == 0) {
        const cJSON* cards = cJSON_GetObjectItemCaseSensitive(json, "cards");
        if (cJSON_IsArray(cards)) {
            y3ws_set_cards_from_json(cards, output_data, output_size);
        } else {
            y3ws_make_error_packet("Missing attribute", output_data, output_size);
        }
    } else {
        y3ws_make_error_packet("Unknown command", output_data, output_size);
    }

end:
    cJSON_Delete(json);
}
#pragma endregion

#pragma region websocket callbacks

static void onopen(struct wws_connection* client) {
    dprintf("Y3WS: Connection opened, addr: %s\n", client->ip_str);
}

static void onclose(struct wws_connection* client) {
    dprintf("Y3WS: Connection closed, addr: %s\n", client->ip_str);
}

static void onmessage(struct wws_connection* client, const char* msg, size_t size) {
    if (cfg.debug) {
        dprintf("Y3WS: Message: %.*s\n", (int)size, msg);
    }
    char* out_buf = malloc(OUTPUT_BUFFER_SIZE);
    if (out_buf == NULL) {
        dprintf("Y3WS: out of memory for allocating response buffer\n");
        client->is_connected = false;
        return;
    }
    size_t out_size = OUTPUT_BUFFER_SIZE;

    y3ws_handle(msg, size, out_buf, &out_size);
    HRESULT hr = wws_send(client, out_buf, out_size);
    if (!SUCCEEDED(hr)) {
        dprintf("Y3WS: Error sending message: %lx\n", hr);
    }

    free(out_buf);
}

static void onlog(const char* format, ...) {
    va_list args;
    va_start (args, format);
    dprintf("Websocket: ");
    dprintfv(format, args);
    va_end (args);
}

#pragma endregion