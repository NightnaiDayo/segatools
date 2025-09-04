#include <stdint.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "3rdparty/websockets/websocket.h"
#include "3rdparty/cjson/cJSON.h"
#include "util/dprintf.h"
#include "../../../hooklib/y3.h"

DWORD __stdcall y3_websocket_thread_proc(LPVOID ctx);

// TODO: make these settings configurable
#define Y3WS_DEBUG true
#define PORT 3497
#define WS_PATH "/y3io"

#define PROTOCOL_VERSION 1

static bool is_initialized = false;

static SOCKET ws_socket = 0;
static HANDLE server_thread = INVALID_HANDLE_VALUE;

static struct CardInfo card_info[32];
static int card_info_size = 0;
static CRITICAL_SECTION card_info_lock;

uint16_t y3_io_get_api_version() {
    return 0x0100;
}

HRESULT y3_io_init() {
    if (is_initialized) {
        return S_FALSE;
    }

    memset(card_info, 0, sizeof(card_info));

    InitializeCriticalSection(&card_info_lock);

    ws_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (ws_socket == -1) {
        dprintf("Y3WS: Failed to create socket\n");
        return E_FAIL;
    }
    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(PORT);
    if (bind(ws_socket, (struct sockaddr *) &local, sizeof(local)) == -1) {
        dprintf("Y3WS: Failed to bind socket\n");
        return E_FAIL;
    }

    if (listen(ws_socket, 1) == -1) {
        dprintf("Y3WS: Failed to listen on socket\n");
        return E_FAIL;
    }

    server_thread = CreateThread(NULL, 0, y3_websocket_thread_proc, NULL, 0, NULL);

    is_initialized = true;

    dprintf("Y3WS: Started server on port %d\n", PORT);

    return S_OK;
}

HRESULT y3_io_close() {
    dprintf("Y3WS: Exiting\n");

    if (ws_socket != 0) {
        closesocket(ws_socket);
        ws_socket = 0;
    }
    if (server_thread != NULL) {
        WaitForSingleObject(server_thread, INFINITE);
        server_thread = INVALID_HANDLE_VALUE;
    }

    is_initialized = false;

    return S_OK;
}

int safeSend(SOCKET client, const uint8_t* buffer, size_t bufferSize) {
    ssize_t written = send(client, (char *) buffer, (int) bufferSize, 0);
    if (written == -1) {
        closesocket(client);
        dprintf("Y3WS: send failed");
        return EXIT_FAILURE;
    }
    if (written != bufferSize) {
        closesocket(client);
        perror("Y3WS: connection interrupted");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void y3ws_make_error_packet(const char* message, uint8_t* output_data, size_t* output_size) {
    dprintf("Y3WS: Error: %s\n", message);
    *output_size = sprintf_s((char*)output_data, *output_size, "{\"version\":\"%d\", \"success\":false,\"error\":\"%s\"}", PROTOCOL_VERSION, message);
}

static void y3ws_make_success_packet(uint8_t* output_data, size_t* output_size) {
    *output_size = sprintf_s((char*)output_data, *output_size, "{\"version\":\"%d\", \"success\":true}", PROTOCOL_VERSION);
}

static void y3ws_handle(const char* input_data, uint32_t input_length, uint8_t* output_data, size_t* output_size) {
#if Y3WS_DEBUG
    dprintf("Y3WS: Message was: %s\n", input_data);
#endif
    cJSON* json = cJSON_ParseWithLength(input_data, input_length);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();

        dprintf("Y3WS: Invalid JSON received!\n");
        dprintf("Y3WS: Message was: %s\n", input_data);
        dprintf("Y3WS: Error at: %s\n", error_ptr);
    }

    const cJSON* ver = cJSON_GetObjectItemCaseSensitive(json, "version");
    const cJSON* cmd = cJSON_GetObjectItemCaseSensitive(json, "command");

    if (!cJSON_IsNumber(ver) || ver->valueint <= 0) {
        y3ws_make_error_packet("Missing version attribute", output_data, output_size);
        goto end;
    }
    if (ver->valueint != PROTOCOL_VERSION) {
        y3ws_make_error_packet("Incompatible version", output_data, output_size);
        goto end;
    }

    if (strcmp(cmd->valuestring, "get_game_id") == 0) {
        // TODO: send current game id (SDGY, SDDD)
    } else if (strcmp(cmd->valuestring, "get_cards") == 0) {
        // TODO: get array of {image_filename, card_id}
    } else if (strcmp(cmd->valuestring, "set_field") == 0) {
        const cJSON* cards = cJSON_GetObjectItemCaseSensitive(json, "cards");
        const cJSON* card = NULL;
        card_info_size = 0;

        EnterCriticalSection(&card_info_lock);
        memset(card_info, 0, sizeof(card_info));
        cJSON_ArrayForEach(card, cards) {
            cJSON* x = cJSON_GetObjectItemCaseSensitive(card, "x");
            cJSON* y = cJSON_GetObjectItemCaseSensitive(card, "y");
            cJSON* r = cJSON_GetObjectItemCaseSensitive(card, "rotation");
            cJSON* id = cJSON_GetObjectItemCaseSensitive(card, "id");

            if (cJSON_IsNumber(x) && cJSON_IsNumber(y) && cJSON_IsNumber(r) && cJSON_IsNumber(id)) {
                card_info[card_info_size].eCardStatus = VALID;
                card_info[card_info_size].fX = (float)x->valuedouble;
                card_info[card_info_size].fY = (float)y->valuedouble;
                card_info[card_info_size].fAngle = (float)r->valuedouble;
                card_info[card_info_size].uID = r->valueint;
            }

            card_info_size++;
        }
        LeaveCriticalSection(&card_info_lock);
        y3ws_make_success_packet(output_data, output_size);
    } else {
        y3ws_make_error_packet("Unknown command", output_data, output_size);
    }

end:
    cJSON_Delete(json);
}

static void y3ws_handle_messages(SOCKET client) {
    enum wsFrameType frameType = WS_INCOMPLETE_FRAME;
    enum wsState state = WS_STATE_OPENING;
    const int BUFFER_SIZE = 4096;
    uint8_t in_buf[BUFFER_SIZE];
    uint8_t out_buf[BUFFER_SIZE];
    size_t out_size;

    uint8_t* message = NULL;
    size_t message_size = 0;

    struct handshake hs;
    nullHandshake(&hs);

    while (frameType == WS_INCOMPLETE_FRAME) {
        ssize_t read = recv(client, (char *) in_buf, BUFFER_SIZE, 0);
        if (!read) {
            dprintf("Y3WS: Accept failed\n");
            break;
        }

        if (state == WS_STATE_OPENING) {
            frameType = wsParseHandshake(in_buf, read, &hs);
        } else {
            frameType = wsParseInputFrame(in_buf, read, &message, &message_size);
        }

        if ((frameType == WS_INCOMPLETE_FRAME && read == BUFFER_SIZE) || frameType == WS_ERROR_FRAME) {
            if (frameType == WS_INCOMPLETE_FRAME) {
                dprintf("Y3WS: buffer too small");
            } else {
                dprintf("Y3WS: error in incoming frame\n");
            }

            if (state == WS_STATE_OPENING) {
                memset(out_buf, 0, BUFFER_SIZE);
                out_size = sprintf((char *) out_buf,
                                   "HTTP/1.1 400 Bad Request\r\n"
                                   "%s%s\r\n\r\n",
                                   versionField,
                                   version);
                safeSend(client, out_buf, out_size);
                break;
            } else {
                memset(out_buf, 0, BUFFER_SIZE);
                wsMakeFrame(NULL, 0, out_buf, &out_size, WS_CLOSING_FRAME);
                if (safeSend(client, out_buf, out_size) == EXIT_FAILURE) {
                    break;
                }
                state = WS_STATE_CLOSING;
                frameType = WS_INCOMPLETE_FRAME;
            }
        }

        if (state == WS_STATE_OPENING) {
            assert(frameType == WS_OPENING_FRAME);
            if (frameType == WS_OPENING_FRAME) {
                if (strcmp(hs.resource, WS_PATH) != 0) {
                    dprintf("Y3WS: Invalid path: %s\n", hs.resource);
                    memset(out_buf, 0, BUFFER_SIZE);
                    out_size = sprintf((char *) out_buf, "HTTP/1.1 404 Not Found\r\n\r\n");
                    safeSend(client, out_buf, out_size);
                    break;
                }

                memset(out_buf, 0, BUFFER_SIZE);
                wsGetHandshakeAnswer(&hs, out_buf, &out_size);
                freeHandshake(&hs);
                if (safeSend(client, out_buf, out_size) == EXIT_FAILURE)
                    break;
                state = WS_STATE_NORMAL;
                frameType = WS_INCOMPLETE_FRAME;
            }
        } else {
            if (frameType == WS_CLOSING_FRAME) {
                if (state == WS_STATE_CLOSING) {
                    break;
                } else {
                    memset(out_buf, 0, BUFFER_SIZE);
                    wsMakeFrame(NULL, 0, out_buf, &out_size, WS_CLOSING_FRAME);
                    safeSend(client, out_buf, out_size);
                    break;
                }
            } else if (frameType == WS_TEXT_FRAME) {
                uint8_t* input_data = malloc(message_size + 1);
                memcpy(input_data, message, message_size);
                input_data[message_size] = 0;

                uint8_t* output_data = malloc(BUFFER_SIZE);
                size_t output_size = BUFFER_SIZE;

                y3ws_handle((const char*)input_data, message_size, output_data, &output_size);

                wsMakeFrame(output_data, output_size, out_buf, &out_size, WS_TEXT_FRAME);
                free(input_data);
                if (safeSend(client, out_buf, out_size) == EXIT_FAILURE) {
                    break;
                }
                frameType = WS_INCOMPLETE_FRAME;
            }
        }
    }

    closesocket(client);
}

DWORD __stdcall y3_websocket_thread_proc(LPVOID ctx) {
    while (true) {
        struct sockaddr_in remote;
        socklen_t sockaddr_len = sizeof(remote);
        SOCKET client = accept(ws_socket, (struct sockaddr *) &remote, &sockaddr_len);
        if (client == -1) {
            break;
        }

        dprintf("Y3WS: Connected %s:%d\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));

        y3ws_handle_messages(client);

        dprintf("Y3WS: Disconnected\n");
    }

    closesocket(ws_socket);

    return 0;
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
