# Reproducible media processing engine

Engine 9.0.2-modyn1 uses FFmpeg9.0.2 plus the exact x264 commit locked in inputs.lock.json. No source patches are applied. The recipe enables static native Windows binaries, H.264/AAC plus diagnostic raw/PCM/null encoders, disables networking, device capture and external media-library autodetection; keeps internal lavfi for validation, and preserves native file decoding/filtering required by the toolbox.

Ordinary users receive the complete tool package. Developers building the adapter use the locked engine ZIP from the same GitHub release; MSYS2 is only needed to rebuild the engine itself.

For engine rebuilds prepare a private MSYS2 UCRT64 with gcc, mingw-w64-ucrt-x86_64-pkgconf, make, diffutils and nasm. Use a build path without spaces as recommended for MSYS2. Run `python tools/video-clip/engine/prepare.py`. Inputs, build workspace, prefix and outputs stay under .local. build-msys2.sh is the complete build recipe; build-toolchain.txt and build-version.txt in the engine/source assets record the exact tools and actual configuration used for release.

The release contains FFmpeg/x264 source tarballs, build scripts/locks and toolchain records in the third-party-source ZIP. Compiler/build tools are general-purpose prerequisites, not runtime dependencies. The generated processing binaries are GPL-3.0-or-later; preserve their license/source material when redistributing. The Qt playback DLLs are a separate unmodified SDK dependency.
