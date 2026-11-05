#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../../../.." && pwd)
WAT="$ROOT/tools_local/wasm/apps/dungeon-crawler-carl-text/game.wat"
WAT2WASM="$ROOT/tools_local/wasm/wabt/node_modules/wabt/bin/wat2wasm"
PACKAGE="$ROOT/tools_local/wasm/apps/dungeon-crawler-carl-text/package/.crossnext/apps/Dungeon Crawler Carl"
SIM_PACKAGE="$ROOT/tools_local/wasm/sdcard/.crossnext/apps/Dungeon Crawler Carl"
mkdir -p "$PACKAGE" "$SIM_PACKAGE"
"$WAT2WASM" "$WAT" -o "$PACKAGE/app.wasm"
cp "$PACKAGE/app.wasm" "$SIM_PACKAGE/app.wasm"
cp "$ROOT/tools_local/wasm/apps/dungeon-crawler-carl-text/package/.crossnext/apps/Dungeon Crawler Carl/manifest.json" "$SIM_PACKAGE/manifest.json"
echo "Built $PACKAGE/app.wasm ($(stat -c %s "$PACKAGE/app.wasm") bytes)"
