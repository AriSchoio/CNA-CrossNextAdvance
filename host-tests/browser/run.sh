#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out="${TMPDIR:-/tmp}/browser-tests-$(pwd | cksum | cut -d' ' -f1)"
mkdir -p "$out"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Isrc -Ilib/Epub \
  host-tests/browser/test_core.cpp src/apps_local/browser/BrowserCore.cpp src/util/HtmlToPlainText.cpp \
  lib/Epub/Epub/htmlEntities.cpp -o "$out/test_core"
"$out/test_core"