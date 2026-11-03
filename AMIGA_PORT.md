# Amiga port intake: OpenSupaplex

- Port Atlas: [g-opensupaplex](https://www.amicradle.com/ports/#g-opensupaplex)
- Priority: `P2`
- Atlas decision: `port`
- Upstream source: `https://github.com/sergiou87/open-supaplex`
- Atlas source pin: `bad56a4e174e628643995284ea55d4c49af3137c`
- Imported commit: `bad56a4e174e628643995284ea55d4c49af3137c`
- Rights state: `review`

## Target contract

Primary target: AmigaOS 3.x / AmiCradle AC090 / Motorola 68040 + FPU.

Use the OPEN platform libraries where applicable: OpenGPU, OpenAudio/OpenAHI, OpenInput, OpenSocket and measured OpenMulticore work. Preserve a correct 68040 fallback and do not assume Roadshow.

No build, runtime, playability or release claim is made by this source-intake commit. Upstream submodules are not initialised. Original commercial game data remains external.

## Rights and data boundary

review; Confirm source/data licences before release.

## Build and controls

    make -f amiga/Makefile.amigaos STOVE=... NDK=... ACGAME=...

The platform layer is `amiga/platform_acgame.c`: 320x200 in 16 colours through ACGame (AGA or RTG), keyboard through ACGame's raw keys (amicradle-guest #564), mouse, pads, and sound effects through ahi.device (16-bit mono, 22 kHz). The game runs at its original 70 frames a second; the display shows at most one frame per vertical blank. Music is played from ProTracker MODs by a small replayer of our own (`amiga/modplay.c`), mixed into the same AHI stream; see `amiga/music/README.md` for why the XM tunes were replaced. Two upstream files carry small Amiga changes: `src/utils.c` (byte swaps without SDL) and `src/graphics.c` (deadline frame pacing).

Keys are the original game's: arrows or keypad move, Space with a direction eats or pushes without moving, Esc quits a level, F-keys and the mouse work the menus. Help stands in for F12, the keypad brackets for Num Lock and Scroll Lock.
