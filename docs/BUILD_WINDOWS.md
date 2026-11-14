# Building for Windows 10/11 (x86-64)

Two routes produce the same program: a native MinGW build on Windows, or a
cross-compile from Linux. The CMake files ship in the release source package.
No ROM is needed to build.

## A. Native build on Windows (MSYS2)

1. Install MSYS2, open the "MSYS2 MINGW64" shell and run
   `pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-SDL2`
2. Unpack the source package, then:

```bash
cd StreetFighterII-Recomp-0.1.0-source
cmake -S . -B build/cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake
cp /mingw64/bin/SDL2.dll build/cmake/bin/
cp -r profiles build/cmake/bin/
./build/cmake/bin/StreetFighterII-Windows-x86_64.exe
```

The SDL2 "mingw" development package also works; pass
`-DSDL2_DIR=<SDL2>\x86_64-w64-mingw32\lib\cmake\SDL2`. Building with MSVC
was not tried.

## B. Cross-compile from Linux

```bash
sudo apt install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64-posix cmake ninja-build
# SDL2 2.30.9 mingw development package from https://github.com/libsdl-org/SDL/releases
tar xzf SDL2-devel-2.30.9-mingw.tar.gz
cmake -S . -B build/win64-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
  -DSDL2_DIR=$PWD/SDL2-2.30.9/x86_64-w64-mingw32/lib/cmake/SDL2
cmake --build build/win64-release
SDL2_DLL=$PWD/SDL2-2.30.9/x86_64-w64-mingw32/bin/SDL2.dll ./scripts/package-windows.sh
```

The package (`dist/StreetFighterII-Windows-x86_64.zip`) contains the
executable, `SDL2.dll`, the three version profiles, licences and docs — no
ROM data. The executable links libgcc/libstdc++ statically; it needs only
system DLLs and `SDL2.dll`.

## Using it

Unzip anywhere and run
`StreetFighterII-Windows-x86_64.exe`. Choose your ROM sets under ROM
Configuration (ROM paths may contain any Unicode characters). Settings and saves go to
`%APPDATA%\OmegaKatana\Street Fighter II\`, or beside the executable with
`--portable` or a `portable.txt` file there. Windows SmartScreen may warn
because the executable is not code-signed.

## Verified (2026-11-14)

- Cross-compiled with MinGW-w64 GCC 13 (posix threads). Under Wine 9.0:
  - the headless runner's per-frame RAM hashes are identical to the Linux
    build's over 4,000 frames of the two-player script (Hyper Fighting),
    including a save-state test;
  - a state saved by the Windows build is byte-identical to the Linux build's
    at the same frame; each build loads the other's state and continues with
    identical hashes;
  - the packaged frontend (Direct3D renderer) starts in portable mode, loads
    `sf2.zip` from a folder with a Japanese name given on the command line,
    runs coin, start and player select, quick-saves, and opens the quick menu;
    the Linux frontend then loads that Windows save state.
- Wine had no audio device in the test container (dummy driver), so sound
  output on Windows was not checked.
- **Not run on a Windows 10/11 PC.**
