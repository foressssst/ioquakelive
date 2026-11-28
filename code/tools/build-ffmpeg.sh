#!/bin/sh
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$1
shift
if command -v cygpath >/dev/null 2>&1; then
	OUT=$(cygpath -u "$OUT")
fi
case $OUT in
	/*) ;;
	*) OUT=$(pwd)/$OUT ;;
esac
BUILD="$ROOT/build/ffmpeg-$(basename "$OUT")"
rm -rf "$BUILD"
mkdir -p "$BUILD"
cd "$BUILD"
"$ROOT/code/ffmpeg/configure" \
	--prefix="$OUT" \
	--disable-everything \
	--disable-programs \
	--disable-doc \
	--disable-network \
	--disable-autodetect \
	--disable-asm \
	--disable-debug \
	--disable-avdevice \
	--disable-avfilter \
	--disable-swscale \
	--disable-swresample \
	--enable-static \
	--disable-shared \
	--enable-decoder=h264,aac \
	--enable-demuxer=mov \
	--enable-parser=h264,aac \
	--enable-protocol=file \
	"$@"
make -j"${JOBS:-4}"
rm -rf "$OUT"
make install
