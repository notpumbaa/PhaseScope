#!/usr/bin/env bash
# Collects the license and documentation files that ship inside every download ZIP.
#
# Usage: packaging/stage-docs.sh <juce-source-dir> <output-dir> <source-ref>
#   juce-source-dir  JUCE checkout used for the build (e.g. build/_deps/juce-src)
#   output-dir       folder that receives LICENSE.txt, README.txt, ... (created if missing)
#   source-ref       git tag or commit the build was made from (used in the source-code link)
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <juce-source-dir> <output-dir> <source-ref>" >&2
    exit 2
fi

root="$(cd "$(dirname "$0")/.." && pwd)"
juce="$1"
out="$2"
ref="$3"

version="$(sed -n 's/^project(PhaseScope VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")"
[ -n "$version" ] || { echo "could not read version from CMakeLists.txt" >&2; exit 1; }

mkdir -p "$out/third-party-licenses"

# Project license + readme
cp "$root/LICENSE" "$out/LICENSE.txt"
title="PhaseScope $version"
underline="$(printf '%*s' "${#title}" '' | tr ' ' '=')"
sed -e "s|@VERSION@|$version|g" \
    -e "s|@UNDERLINE@|$underline|g" \
    -e "s|@SOURCE_URL@|https://github.com/notpumbaa/PhaseScope/tree/$ref|g" \
    "$root/packaging/README.txt.in" > "$out/README.txt"
cp "$root/packaging/THIRD-PARTY-NOTICES.txt" "$out/THIRD-PARTY-NOTICES.txt"

# Third-party licenses: "<source path relative to JUCE>|<target name>"
licenses=(
    "LICENSE.md|JUCE-LICENSE.md"
    "modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt|VST3_SDK-LICENSE.txt"
    "modules/juce_audio_plugin_client/AU/AudioUnitSDK/LICENSE.txt|AudioUnitSDK-LICENSE.txt"
    "modules/juce_core/zip/zlib/README|zlib-README.txt"
    "modules/juce_graphics/image_formats/pnglib/LICENSE|pnglib-LICENSE.txt"
    "modules/juce_graphics/image_formats/jpglib/README|jpeglib-README.txt"
    "modules/juce_audio_formats/codecs/flac/Flac Licence.txt|FLAC-LICENSE.txt"
    "modules/juce_audio_formats/codecs/oggvorbis/Ogg Vorbis Licence.txt|OggVorbis-LICENSE.txt"
    "modules/juce_graphics/fonts/harfbuzz/COPYING|HarfBuzz-COPYING.txt"
    "modules/juce_graphics/unicode/sheenbidi/LICENSE|SheenBidi-LICENSE.txt"
)

for entry in "${licenses[@]}"; do
    src="$juce/${entry%%|*}"
    dst="$out/third-party-licenses/${entry##*|}"
    if [ ! -f "$src" ]; then
        echo "missing license file: $src" >&2
        exit 1
    fi
    cp "$src" "$dst"
done

echo "Staged license and documentation files (PhaseScope $version, $ref) in $out"
