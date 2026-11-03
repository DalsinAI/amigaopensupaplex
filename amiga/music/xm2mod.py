#!/usr/bin/env python3
"""Convert OpenSupaplex's 4-channel XM tunes to ProTracker MODs (M.K.).

    python3 xm2mod.py IN.xm OUT.mod

Each instrument becomes one 8-bit MOD sample, resampled so every note the
tune plays falls in ProTracker's three octaves (C-1..B-3). Volume
envelopes become the sample volume at their sustain point; a key-off
becomes C00. The tunes only use pattern breaks (Dxx), volume slides (Axy)
and set-volume, which MOD has. The AmiCradle Team. GPL 3.
"""
import math
import struct
import sys

from xmparse import read_xm

PERIODS = [856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
           428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
           214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113]
MOD_C2_HZ = 7093789.2 / (428 * 2)
MAX_LEN = 131070


def resample(data, ratio):
    n = int(len(data) * ratio)
    out = []
    for i in range(n):
        x = i / ratio
        a = int(x)
        f = x - a
        s0 = data[a] if a < len(data) else 0
        s1 = data[a + 1] if a + 1 < len(data) else s0
        out.append(s0 + (s1 - s0) * f)
    return out


def convert(src, dst):
    m = read_xm(src)
    assert m['chans'] <= 4, 'more than 4 channels'
    used = {}
    for pat in m['patterns']:
        for row in pat:
            for n, i, v, e, p in row:
                if 0 < n < 97 and i:
                    used.setdefault(i, set()).add(n)

    samples, shift = [], {}
    for idx, ins in enumerate(m['instruments'], 1):
        if idx not in used or not ins.samples:
            samples.append(None)
            continue
        s = ins.samples[0]
        notes = sorted(used[idx])
        # MOD note k (0 = C-1) plays at MOD_C2_HZ * 2^((k-12)/12).
        # XM note n plays at 8363 * 2^((n + rel + ft/128 - 49)/12).
        c = 12 * math.log2(8363 / MOD_C2_HZ) + s.finetune / 128
        best = None
        for j in range(-60, 61):
            ks = [12 + n + s.relnote - 49 + j for n in notes]
            if min(ks) < 0 or max(ks) > 35:
                continue
            ratio = 2 ** ((j - c) / 12)
            if len(s.data) * ratio > MAX_LEN:
                continue
            if best is None or (ratio <= 1.0 and j > best[0]) or (best[1] > 1.0 and ratio < best[1]):
                best = (j, ratio)
        assert best, 'instrument %d does not fit MOD' % idx
        j, ratio = best
        shift[idx] = j
        data = resample(s.data, ratio)
        peak = max(1, max(abs(x) for x in data))
        gain = min(1.0, 32767 / peak)
        pcm = bytes((max(-128, min(127, int(round(x * gain / 256)))) & 0xff) for x in data)
        if len(pcm) % 2:
            pcm += b'\0'
        vol = s.volume
        if ins.volenv:
            vol = vol * ins.volenv[min(ins.sustain, len(ins.volenv) - 1)][1] // 64
        loopstart = looplen = 0
        if s.loop:
            loopstart = int(s.loopstart * ratio) & ~1
            looplen = int(s.looplen * ratio) & ~1
        samples.append((ins.name or s.name, pcm, min(64, vol), loopstart, looplen))

    # patterns: 64 rows each, longer XM patterns split
    out_pats, xm_to_mod = [], {}
    for pi, pat in enumerate(m['patterns']):
        chunks = []
        for start in range(0, len(pat), 64):
            rows = pat[start:start + 64]
            cells = []
            for r in range(64):
                row = []
                for ch in range(4):
                    if r < len(rows) and ch < m['chans']:
                        n, i, v, e, p = rows[r][ch]
                    else:
                        n = i = v = e = p = 0
                    period = 0
                    if 0 < n < 97 and i in shift:
                        k = 12 + n + m['instruments'][i - 1].samples[0].relnote - 49 + shift[i]
                        period = PERIODS[k]
                    elif n == 97 and not e and not p:
                        e, p = 0x0C, 0
                    if not e and not p and 0x10 <= v <= 0x50:
                        e, p = 0x0C, v - 0x10
                    if e not in (0x0, 0xA, 0xC, 0xD, 0xF):
                        e = p = 0
                    row.append([period, i if i in shift else 0, e, p])
                cells.append(row)
            if len(rows) < 64:
                cells[len(rows) - 1][0][2:] = [0x0D, 0]
            chunks.append(cells)
        xm_to_mod[pi] = list(range(len(out_pats), len(out_pats) + len(chunks)))
        out_pats.extend(chunks)

    order = []
    for o in m['order']:
        order.extend(xm_to_mod[o])
    assert len(order) <= 128 and len(out_pats) <= 64

    # speed and tempo on the first row of the first pattern, in free slots
    first = out_pats[order[0]][0]
    want = [(0x0F, m['tempo']), (0x0F, m['bpm'])]
    for cell in first:
        if want and cell[2] == 0 and cell[3] == 0:
            cell[2], cell[3] = want.pop(0)
    assert not want, 'no free slot for speed/tempo'

    b = bytearray(m['name'][:20].encode('latin1').ljust(20, b'\0'))
    for s in samples + [None] * (31 - len(samples)):
        if s is None:
            b += b'\0' * 22 + struct.pack('>HBBHH', 0, 0, 0, 0, 1)
        else:
            name, pcm, vol, ls, ll = s
            b += name[:22].encode('latin1').ljust(22, b'\0')
            b += struct.pack('>HBBHH', len(pcm) // 2, 0, vol, ls // 2, max(1, ll // 2))
    b += bytes([len(order), 127]) + bytes(order).ljust(128, b'\0') + b'M.K.'
    for pat in out_pats:
        for row in pat:
            for period, ins, e, p in row:
                b += bytes([(ins & 0xf0) | (period >> 8), period & 0xff,
                            ((ins & 0x0f) << 4) | e, p])
    for s in samples:
        if s:
            b += s[1]
    open(dst, 'wb').write(b)
    return len(b)


if __name__ == '__main__':
    print(convert(sys.argv[1], sys.argv[2]))
