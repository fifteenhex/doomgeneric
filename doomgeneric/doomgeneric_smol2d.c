//doomgeneric for smol2d
//
// Build with Makefile.smol2d: a CMAP256 build, so DG_ScreenBuffer is
// the 8 bit indexed frame and it goes to smol2d as a texture in the
// SMOL2D_CS_C8 colourspace, with doom's palette becoming the smol2d
// palette. Which backend puts it on a screen is smol2d's business:
// the drm backend needs only SMOL2D_DRM_CARD (a real card, or a
// fakedrm) and SMOL2D_KEYBOARD pointing somewhere readable.

#include "doomdef.h"
#include "doomkeys.h"
#include "i_system.h"
#include "i_video.h"
#include "doomgeneric.h"

#include <stdio.h>

#include <smol2d.h>

static void *s_Cntx = NULL;
static struct smol2d_tex *s_Frame = NULL;
static struct smol2d_pipeline *s_Pipeline = NULL;

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned char convertToDoomKey(enum smol2d_key key)
{
    switch (key)
    {
    case SMOL2D_KEY_UP:
        return KEY_UPARROW;
    case SMOL2D_KEY_DOWN:
        return KEY_DOWNARROW;
    case SMOL2D_KEY_LEFT:
        return KEY_LEFTARROW;
    case SMOL2D_KEY_RIGHT:
        return KEY_RIGHTARROW;
    case SMOL2D_KEY_ENTER:
        return KEY_ENTER;
    case SMOL2D_KEY_ESC:
        return KEY_ESCAPE;
    case SMOL2D_KEY_SPACE:
        return KEY_USE;
    case SMOL2D_KEY_CTRL:
        return KEY_FIRE;
    case SMOL2D_KEY_SHIFT:
        return KEY_RSHIFT;
    case SMOL2D_KEY_ALT:
        return KEY_LALT;
    case SMOL2D_KEY_TAB:
        return KEY_TAB;
    case SMOL2D_KEY_Y:
        return 'y';
    case SMOL2D_KEY_N:
        return 'n';
    default:
        return 0;
    }
}

static void pumpKeys(void)
{
    enum smol2d_key key;
    int down;

    while (smol2d_getkey(s_Cntx, &key, &down) == 1)
    {
        unsigned char doomKey = convertToDoomKey(key);

        if (!doomKey)
        {
            continue;
        }

        s_KeyQueue[s_KeyQueueWriteIndex] = (down << 8) | doomKey;
        s_KeyQueueWriteIndex++;
        s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
    }
}

void DG_Init()
{
    struct smol2d_tex *backbuffer;
    struct smol2d_op op = { 0 };

    if (smol2d_init(&s_Cntx, SMOL2D_CS_C8))
    {
        printf("DG_Init: smol2d_init failed\n");
        exit(1);
    }

    if (smol2d_tex_create(s_Cntx, &s_Frame,
                          DOOMGENERIC_RESX, DOOMGENERIC_RESY))
    {
        printf("DG_Init: creating the %dx%d frame texture failed\n",
               DOOMGENERIC_RESX, DOOMGENERIC_RESY);
        exit(1);
    }

    backbuffer = smol2d_getbackbuffer(s_Cntx);

    // centred, or clipped if the screen is smaller than the frame
    op.type = SMOL2D_OP_BLIT;
    op.dst = backbuffer;
    op.rop = SMOL2D_ROP_COPY;
    op.blit.src = s_Frame;
    op.blit.x = ((int) backbuffer->w - DOOMGENERIC_RESX) / 2;
    op.blit.y = ((int) backbuffer->h - DOOMGENERIC_RESY) / 2;

    if (smol2d_pipeline_create(s_Cntx, &op, 1, &s_Pipeline))
    {
        printf("DG_Init: creating the pipeline failed\n");
        exit(1);
    }

    smol2d_setframerate(s_Cntx, TICRATE);
}

void DG_DrawFrame()
{
    int ret;

    if (palette_changed)
    {
        struct smol2d_palette palette;
        int i;

        for (i = 0; i < 256; i++)
        {
            palette.colours[i].r = colors[i].r;
            palette.colours[i].g = colors[i].g;
            palette.colours[i].b = colors[i].b;
        }

        smol2d_setpalette(s_Cntx, &palette);
        palette_changed = false;
    }

    smol2d_tex_load(s_Cntx, s_Frame, DG_ScreenBuffer);
    smol2d_pipeline_run(s_Cntx, s_Pipeline);

    ret = smol2d_present(s_Cntx);

    if (ret)
    {
        // told to stop, or the display went away
        I_Quit();
    }

    pumpKeys();
}

void DG_SleepMs(uint32_t ms)
{
    usleep(ms * 1000);
}

uint32_t DG_GetTicksMs()
{
    // the backend's clock, so that a backend with a virtual one (the
    // mem backend runs a frame per present, wall clocks be damned)
    // paces the game too
    return (uint32_t) smol2d_getticks(s_Cntx);
}

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
    unsigned short keyData;

    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
    {
        return 0;
    }

    keyData = s_KeyQueue[s_KeyQueueReadIndex];
    s_KeyQueueReadIndex++;
    s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

    *pressed = keyData >> 8;
    *doomKey = keyData & 0xFF;

    return 1;
}

void DG_SetWindowTitle(const char *title)
{
    (void) title;
}

int main(int argc, char **argv, char **envp)
{
    doomgeneric_Create(argc, argv);

    for (;;)
    {
        doomgeneric_Tick();
    }

    return 0;
}
