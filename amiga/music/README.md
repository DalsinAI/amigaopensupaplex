# Music: why MODs instead of XMs

Upstream OpenSupaplex plays its music from three FastTracker 2 modules (`music-adlib.xm`, `music-roland.xm`, `music-beep.xm`) through SDL_mixer. The Amiga port has no SDL_mixer, and an XM player (16-bit samples, envelopes, linear frequencies) is heavy work for a 68040. So we converted the tunes to 4-channel ProTracker MODs, the Amiga's native music format, and took the XMs out of the repository.

- `resources/audio/music-adlib.mod` and `music-roland.mod` were made by `xm2mod.py` from upstream's XMs at the imported commit (bad56a4). Each is about 97 KB, against about 400 KB for each XM.
- The tunes use four channels or fewer and only pattern breaks, volume slides and set-volume, so nothing is lost in the notes. The samples are 8-bit and resampled to fit ProTracker's three octaves. Volume envelopes become a fixed volume, and a key-off becomes a cut.
- The PC-speaker tune (`music-beep.xm`) spans more than five octaves on one square wave, which doesn't fit a MOD. The Amiga has no PC speaker, so the "beep" and "internal" music settings play the AdLib tune instead.
- `amiga/modplay.c` plays the MODs, mixed with the sound effects into one ahi.device stream. Any other MOD player (for example on Paula) can play them as well.

To make them again, take the XMs from upstream at bad56a4 and run:

    python3 amiga/music/xm2mod.py music-adlib.xm resources/audio/music-adlib.mod
    python3 amiga/music/xm2mod.py music-roland.xm resources/audio/music-roland.mod

The other platforms in this tree load the same MODs: SDL_mixer's music path now opens `music-%s.mod` (beep falls back to AdLib), the Vita package lists the MODs, and the Nintendo DS build, where maxmod turns `resources/audio` into a soundbank, plays the AdLib tune for beep.
