#include <windows.h>

#include <assert.h>
#include <stdlib.h>

#include "gfxhook/gfx.h"
#include "gfxhook/gl.h"

#include "hook/table.h"

#include "hooklib/dll.h"

#include "util/dprintf.h"

/* Hook functions */

static void WINAPI hook_glutFullScreen(void);
static void WINAPI hook_glutInitDisplayMode(unsigned int mode);

/* Link pointers */

static void (WINAPI *next_glutFullScreen)(void);
static void (WINAPI *next_glutInitDisplayMode)(unsigned int mode);

static struct gfx_config gfx_config;

static const struct hook_symbol glut_hooks[] = {
    {
        .name = "glutFullScreen",
        .patch = hook_glutFullScreen,
        .link = (void **) &next_glutFullScreen,
    }, {
        .name = "glutInitDisplayMode",
        .patch = hook_glutInitDisplayMode,
        .link = (void **) &next_glutInitDisplayMode,
    },
};

void gfx_gl_hook_init(const struct gfx_config *cfg, HINSTANCE self)
{
    assert(cfg != NULL);

    if (!cfg->enable) {
        return;
    }

    memcpy(&gfx_config, cfg, sizeof(*cfg));
    hook_table_apply(NULL, "glut32.dll", glut_hooks, _countof(glut_hooks));

    if (self != NULL) {
        dll_hook_push(self, L"glut32.dll");
    }
}

static void WINAPI hook_glutFullScreen(void)
{
    dprintf("Gfx: glutFullScreen hook hit\n");

    if (gfx_config.windowed) {
        return;
    }

    return next_glutFullScreen();
}

static void WINAPI hook_glutInitDisplayMode(unsigned int mode)
{
    dprintf("Gfx: glutInitDisplayMode hook hit\n");

    // GLUT adds a frame when going windowed
    if (gfx_config.windowed && !gfx_config.framed) {
        // GLUT_BORDERLESS
        mode |= 0x0800;
    }

    return next_glutInitDisplayMode(mode);
}
