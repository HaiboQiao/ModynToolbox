#!/usr/bin/env bash
set -euo pipefail
export PATH=/ucrt64/bin:/usr/bin:/bin
project_root="$(cygpath -u "$1")"
work="$project_root/.local/media-engine"
inputs="$project_root/.local/media-source"
prefix="$work/prefix"
mkdir -p "$work/x264" "$work/ffmpeg" "$prefix" "$work/dist"
if [ ! -f "$work/x264/configure" ]; then tar -xzf "$inputs/x264.tar.gz" --strip-components=1 -C "$work/x264"; fi
if [ ! -f "$work/ffmpeg/configure" ]; then tar -xJf "$inputs/ffmpeg-9.0.2.tar.xz" --strip-components=1 -C "$work/ffmpeg"; fi
jobs="${MODYN_ENGINE_JOBS:-8}"
cd "$work/x264"
./configure --prefix="$prefix" --host=x86_64-w64-mingw32 --enable-static --disable-cli --disable-opencl --bit-depth=all
make -r -j"$jobs"
make -r install
export PKG_CONFIG_PATH="$prefix/lib/pkgconfig"
cd "$work/ffmpeg"
./configure --prefix="$prefix" --target-os=mingw32 --arch=x86_64 --cc=gcc --cxx=g++ \
  --disable-autodetect --disable-network --disable-doc --disable-debug --disable-shared --enable-static \
  --disable-programs --enable-ffmpeg --enable-ffprobe --disable-indevs --disable-outdevs --enable-indev=lavfi \
  --enable-gpl --enable-version3 --enable-libx264 --pkg-config-flags=--static \
  --disable-encoders --enable-encoder=libx264,aac,png,rawvideo,pcm_s16le,wrapped_avframe \
  --extra-cflags="-I$prefix/include" --extra-ldflags="-L$prefix/lib -static -static-libgcc"
make -r -j"$jobs" ffmpeg.exe ffprobe.exe
cp ffmpeg.exe ffprobe.exe "$work/dist/"
cp COPYING.GPLv3 "$work/dist/LICENSE"
./ffmpeg.exe -version > "$work/dist/build-version.txt" 2>&1
pacman -Q > "$work/dist/build-toolchain.txt"
cp "$project_root/tools/video-clip/engine/build-msys2.sh" "$work/dist/build-msys2.sh"
cp "$project_root/tools/video-clip/engine/inputs.lock.json" "$work/dist/inputs.lock.json"
