"""Minimal XM reader for the converter (FastTracker 2 module format)."""
import struct


class Sample:
    pass


class Instrument:
    pass


def read_xm(path):
    d = open(path, 'rb').read()
    assert d[:17] == b'Extended Module: '
    m = {}
    m['name'] = d[17:37].rstrip(b'\0 ').decode('latin1')
    hsize = struct.unpack_from('<I', d, 60)[0]
    (m['songlen'], m['restart'], m['chans'], npat, nins, m['flags'], m['tempo'],
     m['bpm']) = struct.unpack_from('<HHHHHHHH', d, 64)
    m['order'] = list(d[80:80 + m['songlen']])
    p = 60 + hsize
    pats = []
    for _ in range(npat):
        phl, ptype, rows, psize = struct.unpack_from('<IBHH', d, p)
        p += phl
        data = d[p:p + psize]
        p += psize
        cells = []
        q = 0
        for r in range(rows):
            row = []
            for c in range(m['chans']):
                if psize == 0:
                    row.append((0, 0, 0, 0, 0))
                    continue
                b = data[q]
                if b & 0x80:
                    q += 1
                    vals = []
                    for bit in range(5):
                        if b & (1 << bit):
                            vals.append(data[q]); q += 1
                        else:
                            vals.append(0)
                    row.append(tuple(vals))
                else:
                    row.append(tuple(data[q:q + 5])); q += 5
            cells.append(row)
        pats.append(cells)
    m['patterns'] = pats
    inss = []
    for _ in range(nins):
        isize = struct.unpack_from('<I', d, p)[0]
        ins = Instrument()
        ins.name = d[p + 4:p + 26].rstrip(b'\0 ').decode('latin1')
        nsmp = struct.unpack_from('<H', d, p + 27)[0]
        ins.samples = []
        ins.keymap = [0] * 96
        ins.volenv = None
        shsize = 40
        if nsmp:
            shsize = struct.unpack_from('<I', d, p + 29)[0]
            ins.keymap = list(d[p + 33:p + 129])
            env = struct.unpack_from('<24H', d, p + 129)
            nvol = d[p + 225]
            voltype = d[p + 233]
            ins.volenv = [(env[i * 2], env[i * 2 + 1]) for i in range(nvol)] if voltype & 1 else None
            ins.sustain = d[p + 227]
            ins.voltype = voltype
            ins.fadeout = struct.unpack_from('<H', d, p + 239)[0]
        p += isize
        hdrs = []
        for _s in range(nsmp):
            s = Sample()
            (s.length, s.loopstart, s.looplen) = struct.unpack_from('<III', d, p)
            s.volume = d[p + 12]
            s.finetune = struct.unpack_from('<b', d, p + 13)[0]
            s.type = d[p + 14]
            s.pan = d[p + 15]
            s.relnote = struct.unpack_from('<b', d, p + 16)[0]
            s.name = d[p + 18:p + 40].rstrip(b'\0 ').decode('latin1')
            hdrs.append(s)
            p += shsize
        for s in hdrs:
            raw = d[p:p + s.length]
            p += s.length
            if s.type & 0x10:
                n = s.length // 2
                vals = struct.unpack('<%dh' % n, raw[:n * 2])
                out, acc = [], 0
                for v in vals:
                    acc = (acc + v + 32768) % 65536 - 32768
                    out.append(acc)
                s.data = out
                s.bits = 16
                s.loopstart //= 2
                s.looplen //= 2
            else:
                out, acc = [], 0
                for v in raw:
                    acc = (acc + (v if v < 128 else v - 256) + 128) % 256 - 128
                    out.append(acc * 256)
                s.data = out
                s.bits = 8
            s.loop = s.type & 3
            ins.samples.append(s)
        inss.append(ins)
    m['instruments'] = inss
    return m
