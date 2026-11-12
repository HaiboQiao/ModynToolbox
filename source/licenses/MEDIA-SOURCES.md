# Media corresponding source and build inputs

This release supplies ModynToolbox-1.2.0-third-party-source.zip beside the binary downloads. It contains the exact FFmpeg 9.0.2 and x264 inputs used for the processing engine, the project build recipe, recorded configuration/toolchain, and Qt 6.8.3/FFmpeg 7.1 playback sources. Original URLs and SHA256 are locked in media-sources.lock.json. No processing-engine source modifications are applied.

Processing engine recipe is tools/video-clip/engine/build-msys2.sh. Use a private MSYS2 UCRT64 environment with GCC, make, diffutils and NASM, download/verify the two engine inputs, and pass the project root to the script. Its generated prefix/output remains .local/media-engine. The engine disables network, third-party autodetection, unused encoders and device capture (internal lavfi generation remains for validation); libx264 supplies H.264, native AAC supplies audio. Exact configuration/version and toolchain records accompany the binary engine.

Unmodified Qt SDK playback FFmpeg 7.1 configuration is

```text
--prefix=installed --disable-programs --disable-doc --disable-debug --enable-network --disable-lzma --enable-pic --disable-vulkan --disable-v4l2-m2m --disable-decoder=truemotion1 --toolchain=msvc --enable-shared --disable-static
```

Qt sources are tagged v6.8.3. Sources and component notices inside the source archives retain their upstream licenses. Qt SDK binary components are unmodified and DLLs stay replaceable. Original application code has no owner-selected open-source license; component notices apply to their respective components.
