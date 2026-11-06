#!/bin/sh
# The release archive, for Cradle's stove_build (target package): OpenSupaplex
# built with amiga/Makefile.amigaos on acgame.library (acgame
# fetched and built at a pinned commit by package/acgame-deps.sh), then
# tools/make_package.py makes opensupaplex-VERSION-68040.lha with its .sha256 and
# .readme: the OpenSupaplex drawer (the program and its GlowIcon,
# the game's resources (levels, graphics, sounds), the ReadMe,
# README.upstream.md, and the acgame.library it was built against in Libs/), its
# icon, and beside them the Amiga Installer script Install-OpenSupaplex and its
# icon (package/installer/, made by amicradle scripts/port_installer.py
# from package/installer.json). OUT also
# gets the drawer as it unpacks, as OpenSupaplex.tar (for a lab), and
# package.json. VERSION is package/VERSION. Nothing is published from here.
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
OUT=${OUT:-"$ROOT/build-amiga-package"}
VERSION=$(cat "$HERE/VERSION")
mkdir -p "$OUT"
OUT=$(CDPATH= cd -- "$OUT" && pwd)
BUILD=$(mktemp -d "${TMPDIR:-/tmp}/opensupaplex-package.XXXXXX")
trap 'rm -rf "$BUILD"' EXIT INT TERM
cd "$ROOT"

. "$HERE/acgame-deps.sh"

make -f amiga/Makefile.amigaos STOVE="$STOVE" NDK="$NDK" ACGAME="$ACGAME" ACGAMELIB="$ACGAMELIB" -j4 > "$OUT/build-output.txt" 2>&1 || { tail -n 60 "$OUT/build-output.txt"; exit 1; }
cp build/amigaos/OpenSupaplex/OpenSupaplex "$BUILD/OpenSupaplex"
(cd resources && tar --format=ustar -cf "$BUILD/resources.tar" *)

# What the ReadMe and the check name: the program's SHA-256, the commit, the
# compiler and the commit's date (the archive's dates, so the same inputs
# make the same bytes).
python3 - "$BUILD" "$STOVE/bin/m68k-amigaos-gcc" "OpenSupaplex" "$ACGAME_COMMIT" <<'PY'
import hashlib, json, subprocess, sys
from pathlib import Path
build, cc, exe, acgame = Path(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4]
git = lambda *a: subprocess.run(["git", *a], capture_output=True, text=True).stdout.strip()
receipt = {
    "executable_sha256": hashlib.sha256((build / exe).read_bytes()).hexdigest(),
    "source_commit": git("rev-parse", "HEAD"),
    "compiler_release": subprocess.run([cc, "--version"], capture_output=True,
                                       text=True).stdout.splitlines()[0],
    "source_date_epoch": int(git("log", "-1", "--format=%ct") or 0),
    "acgame_commit": acgame,
    "acgame_library_sha256": hashlib.sha256((build / "acgame.library").read_bytes()).hexdigest(),
}
(build / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
PY
cp "$BUILD/receipt.json" "$OUT/receipt.json"
python3 "$ROOT/tools/make_package.py" --build "$BUILD" --version "$VERSION" --out "$OUT"
