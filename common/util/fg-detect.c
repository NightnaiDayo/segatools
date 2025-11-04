#include <stdbool.h>
#include <windows.h>

#include "util/dprintf.h"
#include "util/fg-detect.h"

#include <assert.h>

static HWND window_handle;
static const wchar_t* window_title = NULL;
static bool foreground_state = true;
static bool partial_match;
static wchar_t scanned_title[256];

static HWND get_window(){
	if (window_handle != NULL){
		return window_handle;
	}

    // try detecting the window
	HWND hwnd = GetForegroundWindow();
	if (GetWindowTextW(hwnd, scanned_title, sizeof(scanned_title)) == 0) {
	    return NULL;
	}
    if (partial_match) {
        if (wcsstr(scanned_title, window_title) == NULL) {
            return NULL;
        }
    } else {
        if (wcscmp(scanned_title, window_title) != 0) {
            return NULL;
        }
    }
    dprintf("FG-Detect: Program window detected\n");
    window_handle = hwnd;
    return window_handle;
}

void fgdet_init(const wchar_t* wnd_title, const bool wnd_partial_match) {
    assert(wnd_title != NULL);

    window_handle = NULL;
    window_title = wnd_title;
    partial_match = wnd_partial_match;
    dprintf("FG-Detect: Searching for \"%ls\"\n", window_title);
}

bool fgdet_in_foreground(){
    return foreground_state;
}

void fgdet_poll(){
    if (window_title == NULL){
        return;
    } else if (GetForegroundWindow() == get_window()){
        if (!foreground_state){
            dprintf("FG-Detect: Got focus\n");
            foreground_state = true;
        }
    } else {
        if (foreground_state){
            dprintf("FG-Detect: Lost focus\n");
            foreground_state = false;
        }
    }
}
