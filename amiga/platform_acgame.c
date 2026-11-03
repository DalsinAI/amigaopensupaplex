/*
 * OpenSupaplex platform layer for AmigaOS 3.x through ACGame: video,
 * keyboard, mouse, pads, timing and sound effects (ahi.device).
 *
 * The game draws 320x200 in 16 colours at 70 frames a second, as on VGA.
 * The display shows at most one frame per vertical blank; frames in
 * between are drawn but not shown, so the game keeps its speed on a 50 Hz
 * screen. The music is played from ProTracker MODs made from the original
 * XM tunes (see amiga/music/README.md).
 *
 * The AmiCradle Team. GPL 3, like OpenSupaplex.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <exec/semaphores.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <acgame/platform.h>

#include "../src/audio.h"
#include "../src/controller.h"
#include "../src/globals.h"
#include "../src/keyboard.h"
#include "../src/logging.h"
#include "../src/system.h"
#include "../src/touchscreen.h"
#include "../src/video.h"
#include "modplay.h"

static const char version[] __attribute__((used)) =
    "$VER: OpenSupaplex 7.1 (03.11.2026) AmiCradle";

/* ------------------------------------------------------------- system */

static acgame_input_state gInput;
static uint32_t gTimeBase;
static int gVideoOpen;

static void presentIfDue(void);

static void pollInput(void)
{
    if (!gVideoOpen)
        return;
    acgame_platform_poll(&gInput);
    if (gInput.quit_requested && gInput.key[ACGAME_KEY_ESCAPE] == 0)
    {
        /* the window was closed, not Esc pressed */
        gShouldExitLevel = 1;
        gShouldExitGame = 1;
    }
}

void initializeSystem(void)
{
    gTimeBase = acgame_platform_ticks_ms();
}

void destroySystem(void)
{
}

uint8_t isOld3DSSystem(void)
{
    return 0;
}

void exitWithError(const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
    destroyAudio();
    destroyVideo();
    exit(errno ? errno : 20);
}

static void int8handler(void)
{
    if (gIsGameRunning != 0)
    {
        gAuxGameSeconds20msAccumulator++;
        if (gAuxGameSeconds20msAccumulator >= 50)
        {
            gAuxGameSeconds20msAccumulator = 0;
            gGameSeconds++;
            if (gGameSeconds >= 60)
            {
                gGameSeconds = 0;
                gGameMinutes++;
                if (gGameMinutes >= 60)
                {
                    gGameMinutes = 0;
                    gGameHours++;
                }
            }
        }
    }
    if (gCurrentSoundDuration != 0)
    {
        gCurrentSoundDuration--;
        if (gCurrentSoundDuration == 0)
            gCurrentSoundPriority = 0;
    }
}

/* The original game's 20 ms timer interrupt. */
static void emulateClock(void)
{
    static uint32_t sLastTickCount = 0;
    static uint32_t sRemainingTicks = 0;

    if (sLastTickCount == 0)
    {
        sLastTickCount = getTime();
        return;
    }
    sRemainingTicks += getTime() - sLastTickCount;
    while (sRemainingTicks > 20)
    {
        int8handler();
        sRemainingTicks -= 20;
    }
    sLastTickCount = getTime();
}

void handleSystemEvents(void)
{
    pollInput();
    presentIfDue();
    emulateClock();
}

uint32_t getTime(void)
{
    /* +1 so the game never sees 0, which it treats as "not started" */
    return acgame_platform_ticks_ms() - gTimeBase + 1;
}

void waitTime(uint32_t time)
{
    if (time)
        usleep(time * 1000UL);
}

/* -------------------------------------------------------------- video */

static uint8_t kScreenPixels[kScreenWidth * kScreenHeight];
uint8_t *gScreenPixels = kScreenPixels;
static ScalingMode gScalingMode = ScalingModeAspectFit;
static acgame_rgb8 gPalette[256];
static int gPointerX = kScreenWidth / 2, gPointerY = kScreenHeight / 2;
static uint32_t gLastPresent;
static int gPresentPending;

#define kPresentInterval 20 /* ms: one 50 Hz frame */

static void doPresent(void)
{
    acgame_indexed_surface *frame = acgame_platform_begin_indexed_frame();
    unsigned y;

    if (frame == NULL)
        return;
    for (y = 0; y < kScreenHeight && y < frame->height; y++)
        memcpy(frame->pixels + (size_t)y * frame->pitch,
               kScreenPixels + y * kScreenWidth, kScreenWidth);
    memcpy(frame->palette, gPalette, sizeof(gPalette));
    acgame_platform_present_indexed(frame);
    gLastPresent = getTime();
    gPresentPending = 0;
}

static void presentIfDue(void)
{
    if (gVideoOpen && gPresentPending && getTime() - gLastPresent >= kPresentInterval)
        doPresent();
}

void initializeVideo(uint8_t fastMode)
{
    acgame_video_config config;

    (void)fastMode;
    memset(&config, 0, sizeof(config));
    config.width = kScreenWidth;
    config.height = kScreenHeight;
    config.refresh_hz = 50;
    config.mode = ACGAME_VIDEO_INDEXED8;
    config.display = ACGAME_DISPLAY_AUTO;
    if (acgame_platform_init(&config) != 0)
    {
        fprintf(stderr, "OpenSupaplex: could not open a 320x200 screen\n");
        exit(20);
    }
    gVideoOpen = 1;
}

void render(void)
{
}

void present(void)
{
    gPresentPending = 1;
    presentIfDue();
}

void destroyVideo(void)
{
    if (gVideoOpen)
        acgame_platform_shutdown();
    gVideoOpen = 0;
}

ScalingMode getScalingMode(void)
{
    return gScalingMode;
}

void setScalingMode(ScalingMode mode)
{
    gScalingMode = mode;
}

void getWindowSize(int *width, int *height)
{
    *width = kScreenWidth;
    *height = kScreenHeight;
}

void centerMouse(void)
{
    moveMouse(kScreenWidth / 2, kScreenHeight / 2);
}

void moveMouse(int x, int y)
{
    gPointerX = x;
    gPointerY = y;
}

void hideMouse(void)
{
}

void getMouseState(int *x, int *y, uint8_t *leftButton, uint8_t *rightButton)
{
    gPointerX += gInput.mouse_dx;
    gPointerY += gInput.mouse_dy;
    gInput.mouse_dx = 0;
    gInput.mouse_dy = 0;
    if (gPointerX < 0) gPointerX = 0;
    if (gPointerX > kScreenWidth) gPointerX = kScreenWidth;
    if (gPointerY < 0) gPointerY = 0;
    if (gPointerY > kScreenHeight) gPointerY = kScreenHeight;
    *x = gPointerX;
    *y = gPointerY;
    *leftButton = (gInput.mouse_buttons & 1) != 0;
    *rightButton = (gInput.mouse_buttons & 2) != 0;
}

void toggleFullscreen(void)
{
}

void setFullscreenMode(uint8_t fullscreen)
{
    (void)fullscreen;
}

uint8_t getFullscreenMode(void)
{
    return 1;
}

void setGlobalPaletteColor(const uint8_t index, const Color color)
{
    gPalette[index].r = color.r;
    gPalette[index].g = color.g;
    gPalette[index].b = color.b;
}

void setColorPalette(const ColorPalette palette)
{
    int i;

    for (i = 0; i < kNumberOfColors; i++)
        setGlobalPaletteColor((uint8_t)i, palette[i]);
}

/* ----------------------------------------------------------- keyboard */

uint8_t gIsEscapeKeyPressed, gIs1KeyPressed, gIs2KeyPressed, gIs3KeyPressed,
    gIs4KeyPressed, gIs5KeyPressed, gIs6KeyPressed, gIs7KeyPressed,
    gIs8KeyPressed, gIs9KeyPressed, gIs0KeyPressed, gIsMinusKeyPressed,
    gIsEqualsKeyPressed, gIsQKeyPressed, gIsWKeyPressed, gIsRKeyPressed,
    gIsPKeyPressed, gIsEnterPressed, gIsLeftControlPressed, gIsSKeyPressed,
    gIsDKeyPressed, gIsHKeyPressed, gIsJKeyPressed, gIsLKeyPressed,
    gIsLeftShiftPressed, gIsZKeyPressed, gIsXKeyPressed, gIsCKeyPressed,
    gIsBKeyPressed, gIsMKeyPressed, gIsRightShiftPressed,
    gIsNumpadMultiplyPressed, gIsLeftAltPressed, gIsSpaceKeyPressed,
    gIsF1KeyPressed, gIsF2KeyPressed, gIsF3KeyPressed, gIsF4KeyPressed,
    gIsF5KeyPressed, gIsF6KeyPressed, gIsF7KeyPressed, gIsF8KeyPressed,
    gIsF9KeyPressed, gIsF10KeyPressed, gIsNumLockPressed,
    gIsScrollLockPressed, gIsHomeKeyPressed, gIsUpKeyPressed,
    gIsRePagKeyPressed, gIsNumpadMinusPressed, gIsLeftKeyPressed,
    gIsNumpad5Pressed, gIsRightKeyPressed, gIsNumpadPlusPressed,
    gIsEndKeyPressed, gIsDownKeyPressed, gIsAvPagKeyPressed,
    gIsInsertKeyPressed, gIsDelKeyPressed, gIsNumpadDividePressed,
    gIsF12KeyPressed;

/* Amiga raw key codes */
enum {
    RK_1 = 0x01, RK_0 = 0x0a, RK_MINUS = 0x0b, RK_EQUALS = 0x0c,
    RK_KP0 = 0x0f, RK_Q = 0x10, RK_P = 0x19, RK_KP1 = 0x1d, RK_KP2, RK_KP3,
    RK_A = 0x20, RK_L = 0x28, RK_KP4 = 0x2d, RK_KP5, RK_KP6, RK_Z = 0x31,
    RK_M = 0x37, RK_KPDOT = 0x3c, RK_KP7, RK_KP8, RK_KP9, RK_SPACE = 0x40,
    RK_BACKSPACE = 0x41, RK_RETURN = 0x44, RK_ESC = 0x45, RK_DEL = 0x46,
    RK_KPMINUS = 0x4a, RK_UP = 0x4c, RK_DOWN, RK_RIGHT, RK_LEFT, RK_F1 = 0x50,
    RK_F10 = 0x59, RK_KPLPAREN = 0x5a, RK_KPRPAREN, RK_KPSLASH, RK_KPSTAR,
    RK_KPPLUS, RK_HELP, RK_LSHIFT = 0x60, RK_RSHIFT, RK_CTRL = 0x63,
    RK_LALT = 0x64
};

/* Raw code to the character the original game's table gives. */
static const char kRawChars[0x48] = {
    0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', 0, 0, 0, 0,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 0, 0, 0, 0, 0, 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0, 0, 0, 0, 0, 0, 0,
    0, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', 0, 0, 0, 0, 0, 0, 0, 0,
    ' ', '\b', 0, 0, '\n', 0, 0, 0
};

static int gKeyPressed = -1;

void updateKeyboardState(void)
{
    const uint8_t *k;
    int i;

    handleSystemEvents();
    k = acgame_platform_rawkeys();

    gKeyPressed = -1;
    for (i = 0; i < ACGAME_RAWKEY_COUNT; i++)
        if (k[i] && i != RK_LSHIFT && i != RK_RSHIFT && i != RK_CTRL &&
            i != RK_LALT && i != 0x65 && i != 0x66 && i != 0x67 && i != 0x62)
        {
            gKeyPressed = i;
            break;
        }

    gIsEscapeKeyPressed = k[RK_ESC];
    gIsSpaceKeyPressed = k[RK_SPACE];
    gIsUpKeyPressed = k[RK_UP] || k[RK_KP8];
    gIsDownKeyPressed = k[RK_DOWN] || k[RK_KP2];
    gIsLeftKeyPressed = k[RK_LEFT] || k[RK_KP4];
    gIsRightKeyPressed = k[RK_RIGHT] || k[RK_KP6];
    gIsInsertKeyPressed = k[RK_KP0];
    gIsEndKeyPressed = k[RK_KP1];
    gIsAvPagKeyPressed = k[RK_KP3];
    gIsNumpad5Pressed = k[RK_KP5];
    gIsHomeKeyPressed = k[RK_KP7];
    gIsRePagKeyPressed = k[RK_KP9];
    gIsDelKeyPressed = k[RK_DEL] || k[RK_KPDOT];
    gIsNumpadDividePressed = k[RK_KPSLASH];
    gIsNumpadMultiplyPressed = k[RK_KPSTAR];
    gIsNumpadPlusPressed = k[RK_KPPLUS];
    gIsNumpadMinusPressed = k[RK_KPMINUS];
    gIsNumLockPressed = k[RK_KPLPAREN];
    gIsScrollLockPressed = k[RK_KPRPAREN];
    gIsLeftAltPressed = k[RK_LALT];
    gIsLeftShiftPressed = k[RK_LSHIFT];
    gIsRightShiftPressed = k[RK_RSHIFT];
    gIsEnterPressed = k[RK_RETURN];
    gIsLeftControlPressed = k[RK_CTRL];
    gIs1KeyPressed = k[RK_1];
    gIs2KeyPressed = k[RK_1 + 1];
    gIs3KeyPressed = k[RK_1 + 2];
    gIs4KeyPressed = k[RK_1 + 3];
    gIs5KeyPressed = k[RK_1 + 4];
    gIs6KeyPressed = k[RK_1 + 5];
    gIs7KeyPressed = k[RK_1 + 6];
    gIs8KeyPressed = k[RK_1 + 7];
    gIs9KeyPressed = k[RK_1 + 8];
    gIs0KeyPressed = k[RK_0];
    gIsMinusKeyPressed = k[RK_MINUS];
    gIsEqualsKeyPressed = k[RK_EQUALS];
    gIsQKeyPressed = k[0x10];
    gIsWKeyPressed = k[0x11];
    gIsRKeyPressed = k[0x13];
    gIsPKeyPressed = k[0x19];
    gIsSKeyPressed = k[0x21];
    gIsDKeyPressed = k[0x22];
    gIsHKeyPressed = k[0x25];
    gIsJKeyPressed = k[0x26];
    gIsLKeyPressed = k[0x28];
    gIsZKeyPressed = k[0x31];
    gIsXKeyPressed = k[0x32];
    gIsCKeyPressed = k[0x33];
    gIsBKeyPressed = k[0x35];
    gIsMKeyPressed = k[0x37];
    gIsF1KeyPressed = k[RK_F1];
    gIsF2KeyPressed = k[RK_F1 + 1];
    gIsF3KeyPressed = k[RK_F1 + 2];
    gIsF4KeyPressed = k[RK_F1 + 3];
    gIsF5KeyPressed = k[RK_F1 + 4];
    gIsF6KeyPressed = k[RK_F1 + 5];
    gIsF7KeyPressed = k[RK_F1 + 6];
    gIsF8KeyPressed = k[RK_F1 + 7];
    gIsF9KeyPressed = k[RK_F1 + 8];
    gIsF10KeyPressed = k[RK_F10];
    gIsF12KeyPressed = k[RK_HELP]; /* no F12 on an Amiga: Help */
}

uint8_t isAnyKeyPressed(void)
{
    return gKeyPressed >= 0;
}

char characterForLastKeyPressed(void)
{
    if (gKeyPressed >= 0 && gKeyPressed < (int)sizeof(kRawChars))
        return kRawChars[gKeyPressed];
    return '\0';
}

uint8_t areAnyF1ToF10KeysPressed(void)
{
    return gKeyPressed >= RK_F1 && gKeyPressed <= RK_F10;
}

/* -------------------------------------------------------- controllers */

#define PADBIT(b) (gInput.pad.connected && \
    (gInput.pad.buttons & ACGAME_PAD_BIT(ACGAME_PAD_ ## b)))

void initializeControllers(void)
{
}

int8_t getGameControllerX(void)
{
    int16_t a = gInput.pad.connected ? gInput.pad.axes[ACGAME_PAD_LEFTX] : 0;
    if (PADBIT(DPAD_LEFT) || a < -16384) return -1;
    if (PADBIT(DPAD_RIGHT) || a > 16384) return 1;
    return 0;
}

int8_t getGameControllerY(void)
{
    int16_t a = gInput.pad.connected ? gInput.pad.axes[ACGAME_PAD_LEFTY] : 0;
    if (PADBIT(DPAD_UP) || a < -16384) return -1;
    if (PADBIT(DPAD_DOWN) || a > 16384) return 1;
    return 0;
}

uint8_t getGameControllerButtonA(void) { return PADBIT(A) ? 1 : 0; }
uint8_t getGameControllerButtonB(void) { return PADBIT(B) ? 1 : 0; }
uint8_t getGameControllerButtonX(void) { return PADBIT(X) ? 1 : 0; }
uint8_t getGameControllerButtonY(void) { return PADBIT(Y) ? 1 : 0; }
uint8_t getGameControllerButtonBack(void) { return PADBIT(BACK) ? 1 : 0; }
uint8_t getGameControllerButtonStart(void) { return PADBIT(START) ? 1 : 0; }
uint8_t getGameControllerButtonLeftShoulder(void) { return PADBIT(LEFTSHOULDER) ? 1 : 0; }
uint8_t getGameControllerButtonRightShoulder(void) { return PADBIT(RIGHTSHOULDER) ? 1 : 0; }
uint8_t getGameControllerConfirmButton(void) { return getGameControllerButtonA(); }
uint8_t getGameControllerCancelButton(void) { return getGameControllerButtonB(); }

uint8_t isAnyGameControllerButtonPressed(void)
{
    return gInput.pad.connected && gInput.pad.buttons != 0;
}

void gameControllerEmulateMouse(float *x, float *y, uint8_t *leftButton, uint8_t *rightButton)
{
    int16_t ax, ay;

    if (!gInput.pad.connected)
        return;
    ax = gInput.pad.axes[ACGAME_PAD_LEFTX];
    ay = gInput.pad.axes[ACGAME_PAD_LEFTY];
    *x = abs(ax) < 8192 ? 0 : (ax < 0 ? -1.0f : 1.0f) * (abs(ax) * 8.0f / 32767 + 1.0f);
    *y = abs(ay) < 8192 ? 0 : (ay < 0 ? -1.0f : 1.0f) * (abs(ay) * 8.0f / 32767 + 1.0f);
    *leftButton = getGameControllerConfirmButton();
    *rightButton = getGameControllerCancelButton();
}

uint8_t readTouchScreen(float *x, float *y)
{
    (void)x;
    (void)y;
    return 0;
}

/* -------------------------------------------------------------- sound */

/* ahi.device's request (AHI's devices/ahi.h, interface version 4). */
struct XAHIRequest {
    struct IOStdReq ahir_Std;
    UWORD ahir_Version;
    UWORD ahir_Pad1;
    ULONG ahir_Private[2];
    ULONG ahir_Type;
    ULONG ahir_Frequency;
    LONG ahir_Volume;
    LONG ahir_Position;
    struct XAHIRequest *ahir_Link;
};
#define XAHI_DEFAULT_UNIT 0
#define XAHIST_M16S 1UL

#define kAudioRate 22050
#define kAudioChunk 512 /* samples per request: 23 ms */

SoundType sndType = SoundTypeNone;
SoundType musType = SoundTypeInternalStandard;

static const char *const kEffectNames[SoundEffectCount] = {
    "explosion", "infotron", "push", "fall", "bug", "base", "exit",
};

typedef struct { int16_t *data; uint32_t length; } Sample;
static Sample gSamples[SoundEffectCount];
static const Sample *volatile gVoice;
static volatile uint32_t gVoicePos;
static volatile uint8_t gEffectsVolume = kMaxVolume;
static volatile uint8_t gMusicVolume = kMaxVolume;
static uint8_t gMusicPlaying;

static struct SignalSemaphore gAudioLock;
static struct Process *gMixer;
static struct Task *gOwner;
static volatile LONG gMixerOk;

static void mix(int16_t *out, unsigned len)
{
    unsigned i;
    int vol = gEffectsVolume;

    ObtainSemaphore(&gAudioLock);
    for (i = 0; i < len; i++)
    {
        int16_t s = 0;
        if (gVoice)
        {
            s = (int16_t)(gVoice->data[gVoicePos] * vol / kMaxVolume);
            if (++gVoicePos >= gVoice->length)
                gVoice = NULL;
        }
        out[i] = s;
    }
    modMix(out, len, gMusicVolume);
    ReleaseSemaphore(&gAudioLock);
}

static void mixerMain(void)
{
    struct MsgPort *port;
    struct XAHIRequest *req[2] = { NULL, NULL };
    struct XAHIRequest *link = NULL;
    int16_t *buf[2] = { NULL, NULL };
    ULONG portsig, sigs;
    int i = 0, opened = 0, quit = 0;

    port = CreateMsgPort();
    if (port)
        req[0] = (struct XAHIRequest *)CreateIORequest(port, sizeof(struct XAHIRequest));
    if (req[0])
    {
        req[0]->ahir_Version = 4;
        opened = OpenDevice((CONST_STRPTR)"ahi.device", XAHI_DEFAULT_UNIT,
                            (struct IORequest *)req[0], 0) == 0;
    }
    if (opened)
    {
        req[1] = AllocVec(sizeof(struct XAHIRequest), MEMF_PUBLIC);
        buf[0] = AllocVec(kAudioChunk * 2, MEMF_PUBLIC);
        buf[1] = AllocVec(kAudioChunk * 2, MEMF_PUBLIC);
    }
    if (!req[1] || !buf[0] || !buf[1])
    {
        gMixerOk = -1;
        goto out;
    }
    CopyMem(req[0], req[1], sizeof(struct XAHIRequest));
    gMixerOk = 1;
    Signal(gOwner, SIGBREAKF_CTRL_F);

    portsig = 1UL << port->mp_SigBit;
    while (!quit)
    {
        mix(buf[i], kAudioChunk);
        req[i]->ahir_Std.io_Message.mn_Node.ln_Pri = 0;
        req[i]->ahir_Std.io_Command = CMD_WRITE;
        req[i]->ahir_Std.io_Data = buf[i];
        req[i]->ahir_Std.io_Length = kAudioChunk * 2;
        req[i]->ahir_Std.io_Offset = 0;
        req[i]->ahir_Type = XAHIST_M16S;
        req[i]->ahir_Frequency = kAudioRate;
        req[i]->ahir_Volume = 0x10000;
        req[i]->ahir_Position = 0x8000;
        req[i]->ahir_Link = link;
        SendIO((struct IORequest *)req[i]);

        if (link)
        {
            while (!CheckIO((struct IORequest *)link))
            {
                sigs = Wait(portsig | SIGBREAKF_CTRL_C);
                if (sigs & SIGBREAKF_CTRL_C)
                {
                    quit = 1;
                    break;
                }
            }
            if (quit)
                AbortIO((struct IORequest *)link);
            WaitIO((struct IORequest *)link);
        }
        else if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
            quit = 1;
        link = req[i];
        i ^= 1;
    }
    if (link)
    {
        AbortIO((struct IORequest *)link);
        WaitIO((struct IORequest *)link);
    }

out:
    if (buf[1]) FreeVec(buf[1]);
    if (buf[0]) FreeVec(buf[0]);
    if (req[1]) FreeVec(req[1]);
    if (opened) CloseDevice((struct IORequest *)req[0]);
    if (req[0]) DeleteIORequest((struct IORequest *)req[0]);
    if (port) DeleteMsgPort(port);
    if (gMixerOk == 0)
        gMixerOk = -1;
    Forbid();
    Signal(gOwner, SIGBREAKF_CTRL_F);
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
        ((uint32_t)p[3] << 24);
}

/* 16-bit mono 44.1 kHz WAV, kept at 22.05 kHz big-endian. */
static void loadSample(Sample *out, const char *path)
{
    FILE *f = fopen(path, "rb");
    uint8_t hdr[12], chunk[8], fmt[16];
    uint32_t size, i, n, rate = 0;
    int bits = 0, have_fmt = 0;
    uint8_t *raw;

    out->data = NULL;
    out->length = 0;
    if (!f)
        return;
    if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4))
        goto done;
    for (;;)
    {
        if (fread(chunk, 1, 8, f) != 8)
            goto done;
        size = le32(chunk + 4);
        if (!memcmp(chunk, "fmt ", 4) && size >= 16)
        {
            if (fread(fmt, 1, 16, f) != 16)
                goto done;
            if (fmt[0] != 1 || fmt[2] != 1)
                goto done; /* PCM mono only */
            rate = le32(fmt + 4);
            bits = fmt[14];
            have_fmt = 1;
            size -= 16;
        }
        else if (!memcmp(chunk, "data", 4))
            break;
        if (fseek(f, (long)((size + 1) & ~1UL), SEEK_CUR) != 0)
            goto done;
    }
    if (!have_fmt || bits != 16)
        goto done;
    raw = malloc(size);
    if (!raw)
        goto done;
    size = (uint32_t)fread(raw, 1, size, f);
    n = size / 2;
    if (rate >= 44100)
        n /= 2;
    out->data = AllocVec(n * 2 + 2, MEMF_PUBLIC);
    if (out->data)
    {
        for (i = 0; i < n; i++)
        {
            const uint8_t *p = raw + (rate >= 44100 ? i * 4 : i * 2);
            out->data[i] = (int16_t)(p[0] | (p[1] << 8));
        }
        out->length = n;
    }
    free(raw);
done:
    fclose(f);
}

int8_t initializeAudio(void)
{
    InitSemaphore(&gAudioLock);
    gOwner = FindTask(NULL);
    gMixerOk = 0;
    SetSignal(0, SIGBREAKF_CTRL_F);
    gMixer = CreateNewProcTags(NP_Entry, (ULONG)mixerMain,
                               NP_Name, (ULONG)"OpenSupaplex sound",
                               NP_Priority, 5, TAG_DONE);
    if (!gMixer)
        return 1;
    Wait(SIGBREAKF_CTRL_F);
    if (gMixerOk != 1)
    {
        gMixer = NULL;
        spLogInfo("ahi.device did not open, playing silently");
        return 1;
    }
    return 0;
}

static void freeSamples(void)
{
    int i;

    ObtainSemaphore(&gAudioLock);
    gVoice = NULL;
    ReleaseSemaphore(&gAudioLock);
    for (i = 0; i < SoundEffectCount; i++)
    {
        if (gSamples[i].data)
            FreeVec(gSamples[i].data);
        gSamples[i].data = NULL;
        gSamples[i].length = 0;
    }
}

void destroyAudio(void)
{
    if (!gMixer)
        return;
    SetSignal(0, SIGBREAKF_CTRL_F);
    Signal((struct Task *)gMixer, SIGBREAKF_CTRL_C);
    Wait(SIGBREAKF_CTRL_F);
    gMixer = NULL;
    freeSamples();
    modFree();
}

void setSoundType(SoundType musicType, SoundType effectsType)
{
    const char *suffix = NULL;
    char path[64];
    int i;

    musType = musicType;
    sndType = effectsType;
    if (!gMixer)
        return;
    freeSamples();

    /* The PC speaker tune has no MOD (its range is too wide for one), so
     * the internal sound types play the AdLib tune. */
    ObtainSemaphore(&gAudioLock);
    if (musicType == SoundTypeNone ||
        modLoad(musicType == SoundTypeRoland ? "audio/music-roland.mod"
                                             : "audio/music-adlib.mod", kAudioRate) != 0)
        modFree();
    else
    {
        modStart();
        if (!gMusicPlaying)
            modStop();
    }
    ReleaseSemaphore(&gAudioLock);

    switch (effectsType)
    {
        case SoundTypeRoland: suffix = "roland"; break;
        case SoundTypeSoundBlaster: suffix = "blaster"; break;
        case SoundTypeAdlib: suffix = "adlib"; break;
        case SoundTypeInternalSamples: suffix = "sample"; break;
        case SoundTypeInternalStandard: suffix = "beep"; break;
        default: return;
    }
    for (i = 0; i < SoundEffectCount; i++)
    {
        snprintf(path, sizeof(path), "audio/%s-%s.wav", kEffectNames[i], suffix);
        loadSample(&gSamples[i], path);
    }
}

uint8_t getMusicVolume(void) { return gMusicVolume; }
void setMusicVolume(uint8_t volume) { gMusicVolume = volume > kMaxVolume ? kMaxVolume : volume; }
uint8_t getSoundEffectsVolume(void) { return gEffectsVolume; }
void setSoundEffectsVolume(uint8_t volume) { gEffectsVolume = volume > kMaxVolume ? kMaxVolume : volume; }

void playMusic(void)
{
    if (!gMixer)
        return;
    ObtainSemaphore(&gAudioLock);
    if (!gMusicPlaying)
        modResume();
    gMusicPlaying = 1;
    ReleaseSemaphore(&gAudioLock);
}

void stopMusic(void)
{
    if (!gMixer)
        return;
    ObtainSemaphore(&gAudioLock);
    modStop();
    gMusicPlaying = 0;
    ReleaseSemaphore(&gAudioLock);
}

void playSoundEffect(SoundEffect soundEffect)
{
    if (!gMixer || soundEffect >= SoundEffectCount || !gSamples[soundEffect].data)
        return;
    ObtainSemaphore(&gAudioLock);
    gVoice = &gSamples[soundEffect];
    gVoicePos = 0;
    ReleaseSemaphore(&gAudioLock);
}
