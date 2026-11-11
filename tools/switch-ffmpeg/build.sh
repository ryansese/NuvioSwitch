#!/usr/bin/env bash
# Rebuilds the devkitPro switch-ffmpeg portlib with https and installs it over the stock one.
#
# The stock package (7.1-5) leaves "https" out of its protocol list, so mpv can't open any https stream.
# This is devkitPro's own recipe (PKGBUILD and patches at a pinned commit) with two changes: mbedTLS as the
# TLS library instead of the console's ssl service, so the CA bundle in romfs decides what is trusted, and
# https in the protocol list.
#
# Run inside the devkitpro/devkita64 image. Usage: build.sh [work dir]
set -euo pipefail

FFMPEG_VERSION=7.1
RECIPE_COMMIT=5c46517a8a1a1f0939a90b8068655ea00fa125b9
RECIPE_URL="https://raw.githubusercontent.com/devkitPro/pacman-packages/${RECIPE_COMMIT}/switch/ffmpeg"

WORK="${1:-/tmp/switch-ffmpeg}"
mkdir -p "$WORK"
cd "$WORK"

fetch() { # url, file, sha256
    [ -f "$2" ] || curl -fsSL "$1" -o "$2"
    echo "$3  $2" | sha256sum -c -
}
fetch "https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz" "ffmpeg-${FFMPEG_VERSION}.tar.xz" \
    40973d44970dbc83ef302b0609f2e74982be2d85916dd2ee7472d30678a7abe6
fetch "${RECIPE_URL}/ffmpeg-${FFMPEG_VERSION}.patch" "ffmpeg-${FFMPEG_VERSION}.patch" \
    1792380b992e3554a4abcddf0d7b395bfd8c118ac7c6e38c8f2fb0d39753a390
fetch "${RECIPE_URL}/tls.patch" tls.patch \
    57ea7ec8ed26d13d3172d0fd589b7883d22f0c180d50b7434fcc73fb2b3ab7d7

rm -rf "ffmpeg-${FFMPEG_VERSION}"
tar xf "ffmpeg-${FFMPEG_VERSION}.tar.xz"
cd "ffmpeg-${FFMPEG_VERSION}"
patch -Np1 -i "../ffmpeg-${FFMPEG_VERSION}.patch"
patch -Np1 -i ../tls.patch

source /opt/devkitpro/switchvars.sh

# As the PKGBUILD, except: --enable-version3 --enable-mbedtls in place of --enable-libnx, and https added.
./configure --prefix="$PORTLIBS_PREFIX" --enable-gpl --enable-version3 --disable-shared --enable-static \
    --cross-prefix=aarch64-none-elf- --enable-cross-compile \
    --arch=aarch64 --cpu=cortex-a57 --target-os=horizon --enable-pic \
    --extra-cflags='-D__SWITCH__ -D_GNU_SOURCE -O2 -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC -ftls-model=local-exec' \
    --extra-cxxflags='-D__SWITCH__ -D_GNU_SOURCE -O2 -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC -ftls-model=local-exec' \
    --extra-ldflags='-fPIE -L${PORTLIBS_PREFIX}/lib -L${DEVKITPRO}/libnx/lib' \
    --disable-runtime-cpudetect --disable-programs --disable-debug --disable-doc --disable-autodetect \
    --enable-asm --enable-neon \
    --disable-avdevice --disable-encoders --disable-muxers \
    --enable-swscale --enable-swresample --enable-network \
    --disable-protocols --enable-protocol=file,http,https,ftp,tcp,udp,rtmp,tls,httpproxy \
    --enable-zlib --enable-bzlib --enable-libass --enable-libfreetype --enable-libfribidi --enable-libdav1d \
    --enable-mbedtls --enable-nvtegra

make -j"$(nproc)"
make install
rm -rf "$PORTLIBS_PREFIX/share/ffmpeg"
