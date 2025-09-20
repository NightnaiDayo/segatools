#pragma once
#include <stdbool.h>

void fgdet_init(const wchar_t* wnd_title);
bool fgdet_in_foreground();
void fgdet_poll();
