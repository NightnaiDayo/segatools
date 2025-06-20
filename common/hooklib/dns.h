#pragma once

#include <windows.h>

#include <stddef.h>
void http_hook_init();
void port_hook_init(unsigned short _startup_port, unsigned short _billing_port, unsigned short _aimedb_port);
// if to_src is NULL, all lookups for from_src will fail
HRESULT dns_hook_push(const wchar_t *from_src, const wchar_t *to_src);

void dns_hook_apply_hooks(HMODULE mod);
