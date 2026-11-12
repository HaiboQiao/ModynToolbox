# Third-party software notices

ModynToolbox dynamically links unmodified Qt 6.8.3 Core, Gui, Widgets, Sql, Concurrent, Network, Multimedia and MultimediaWidgets, with Windows platform, image format, style and SQLite plugins. Qt is copyright The Qt Company Ltd. and its contributors. Qt libraries are provided under LGPL-3.0 or their applicable alternative terms. Full LGPL/GPL texts and the SDK's source SPDX inventory accompany this notice. Qt DLLs remain separate and replaceable. Application source/build instructions are supplied in this project.

Qt corresponding source and component notices are available from the Qt 6.8.3 source distribution and official repository.

- https://download.qt.io/archive/qt/6.8/6.8.3/single/
- https://code.qt.io/cgit/qt/qtbase.git/?h=v6.8.3
- https://doc.qt.io/qt-6.8/licenses-used-in-qt.html

The embedded Python 3.14.10 runtime includes its LICENSE.txt, which covers Python and its bundled components. Pillow 13.0.0 includes its distribution license and bundled codec notices in the wheel's .dist-info/licenses directory, retained in the image tool's private runtime. Source locations and locked hashes are in scripts/runtime.lock.json.

- https://www.python.org/downloads/release/python-31410/
- https://pypi.org/project/pillow/13.0.0/
- https://github.com/python-pillow/Pillow/tree/13.0.0

Microsoft Visual C++ runtime files are unmodified redistributables supplied by the installed Visual Studio toolchain, distributed under Microsoft's applicable runtime distribution terms. The app uses dynamic linking to these files. The root launcher links its C/C++ runtime statically.

The project owner has not selected an open-source license for original ModynToolbox code. These third-party licenses govern their respective components.


## Video processing and playback in 1.2.0

Video processing runs separate FFmpeg 9.0.2 and ffprobe executables built by this project from the exact inputs in tools/video-clip/engine/inputs.lock.json. Only x264 is enabled as an external media library. The combined processing executables are GPL-3.0-or-later; their complete source inputs, configuration/build scripts and build-toolchain record are provided in the release's third-party source ZIP. They are separate executable programs accessed through arguments, files and progress pipes. The ModynToolbox host and adapter do not link these processing binaries.

The dynamically linked Qt playback plugin uses the unmodified FFmpeg 7.1 libraries from the Qt 6.8.3 SDK, under LGPL-2.1-or-later and component terms. Qt's shipped configuration is recorded in MEDIA-SOURCES.md. The exact FFmpeg 7.1 upstream source, Qt base/multimedia sources and source inventories accompany the release. The Qt/FFmpeg playback DLLs remain separate and replaceable.

Corresponding-source download for this version is https://github.com/HaiboQiao/ModynToolbox/releases/download/v1.2.0/ModynToolbox-1.2.0-third-party-source.zip . Source hashes and original URLs are in media-sources.lock.json. Build prerequisites are general-purpose MSYS2/UCRT64 GCC, GNU Make and NASM; the exact versions used are recorded with the processing engine.
