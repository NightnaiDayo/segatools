#pragma once

#include <windows.h>
#include <stdbool.h>

struct ffb_config {
    bool enable;
};

struct ffb_ops {
    void (*toggle)(bool active);
    void (*constant_force)(uint8_t direction, uint8_t force);
    void (*rumble)(uint8_t force, uint8_t period);
    void (*damper)(uint8_t force);
};

HRESULT ffb_hook_init(
        const struct ffb_config *cfg,
        const struct ffb_ops *ops,
        unsigned int port_no);
