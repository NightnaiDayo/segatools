#pragma once

#include <windows.h>

struct epay_config;

/* Install reader factory and direct-registration hooks in the real payment
 * client (ThincaPayment 1.4.11, TFPS 1.0.3). Requires epay hook=0 on x64 and
 * logs the cause of any failure. Returns S_FALSE in 32-bit processes, which
 * share the config but never run payments. */
HRESULT icasrw_hook_init(const struct epay_config *cfg, HMODULE payment);

/* Load [icas] settings and the shared [aime] file paths and scan key. */
void icasrw_config_load(struct epay_config *cfg, const wchar_t *filename);
