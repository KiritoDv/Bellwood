# Bellwood
_An Animal Crossing PC Port_

Lead Developers:
* [Lywx](https://www.github.com/kiritodv)

## Discord

Official Discord: https://discord.gg/harbourmasters

If you're having any trouble after reading through this `README`, feel free ask for help in the Support text channels. Please keep in mind that we do not condone piracy.

# Quick Start

Bellwood does not include any copyrighted assets. You are required to provide a supported copy of the game.

### 1. Verify your disc dump
The supported version is **Animal Crossing (USA)**, game ID `GAFE01`, revision 0. Dolphin shows both on the Info tab of the game's Properties.

The Japanese (`GAFJ01`), European (`GAFP01`) and Australian (`GAFU01`) versions are not supported yet.

### 2. Verify your disc is in a supported format
Bellwood reads `.iso`, `.gcm`, `.ciso` and `.gcz` disc images, or a folder extracted from the disc (containing `sys/` and `files/`). If your dump is an `.rvz` or `.wia`, convert it to `.iso` with Dolphin: right-click the game and choose **Convert File...**.

### 3. Get Bellwood
There are no releases yet. Build Bellwood yourself by following the [building instructions](#building).

### 4. Generating the O2R from the disc and Play!
* Run `bellwood` (`bellwood.exe` on Windows).
* On the first launch Bellwood offers to generate `ac.o2r` from your disc. It uses a supported disc placed next to the executable, or asks you to choose a disc image or an extracted disc folder.
* Extraction runs once; the game starts when it finishes.

You can also pass the disc on the command line: `bellwood /path/to/disc.iso`.

# Configuration

### Default keyboard configuration
| GameCube | A | B | X | Y | Z | L | R | Start | Control stick | C-stick | D-Pad |
| - | - | - | - | - | - | - | - | - | - | - | - |
| Keyboard | X | C | | | Z | E | R | Space | WASD | Arrow keys | TFGH |

X and Y have no keyboard keys by default. You can bind them in **Settings → Controls**.

### Other shortcuts
| Keys | Action |
| - | - |
| Esc or F1 | Toggle menu |
| Ctrl+R (Cmd+R on macOS) | Reset |
| F11 | Fullscreen |
| Tab | Toggle Alternate assets |

### Graphics Backends
Currently, there are four rendering APIs supported: DirectX 11 (Windows), OpenGL (all platforms), Metal (macOS), and Vulkan (only offered when your graphics driver supports it). You can change which API to use in the `Settings` menu, which requires a restart. If you're having an issue with crashing, you can change the API in the `bellwood.cfg.json` file by finding the `"Backend": {` block and changing the `Id` value; the game fills in the matching `Name` on the next launch. The IDs are `1` for `DirectX 11`, `2` for `OpenGL`, `3` for `Metal`, and `4` for `Vulkan`. `DirectX 11` is the default on Windows and `Metal` is the default on macOS.

### Widescreen
The game fills the whole window at any aspect ratio. **Settings → Graphics → HUD Aspect Ratio** keeps the HUD at the window edges or inside a 4:3, 16:9 or 21:9 area.

# Saves
Saves are stored in the `save` folder next to the executable.

To bring over a town from a GameCube memory card, use **Settings → General → Import GameCube Save...** on the title screen, or drop the file onto the window. Bellwood accepts `.gci` and `.gcs` saves and raw memory card images (`.raw`, `.gcp`). Passports, NES games and their saves, and gift letters on the card are imported with the town. Your previous save is backed up in `save/backup` first.

# Game Boy Advance and e-Reader
The PC has no GBA link, so the features that used one work with files instead. Each can be turned off in **Settings → Game Boy Advance**, which also has buttons to open the folders.

* **Island:** Kapp'n takes you to the island without a GBA.
* **e-Reader cards:** put card dumps (`.raw` or `.bin`) in the `ecards` folder. When the game asks for a card, choose one from the list, browse for it, or drop it onto the window.
* **Designs:** the Able Sisters' "Send to GBA" writes your designs as PNG files to the `designs` folder, and "Load from GBA" reads them back.
* **NES games:** memory card NES games (`.gci`) placed in the `famicom` folder show up in the Disk System.

# Custom Assets
Custom assets are packed in `.o2r` or `.otr` files. To use custom assets, place them in the `mods` folder.

If you're interested in creating and/or packing your own custom asset `.o2r`/`.otr` files, check out the following tools:
* [**retro - OTR and O2R generator**](https://github.com/HarbourMasters64/retro)

# Development
### Building

Requires:
* CMake 3.24 or newer
* A C++20 compiler: Visual Studio 2022 on Windows, Clang (Xcode) on macOS, Clang or GCC on Linux
* Git
* SDL2, plus GLEW on macOS (`brew install sdl2 glew`) and the OpenGL development files on Linux. On Windows the libraries are installed automatically through vcpkg.

1. Clone this repository with its submodules (Torch and libultraship):

   ```sh
   git clone --recursive <repository URL>
   ```

   If you cloned without `--recursive`, run `git submodule update --init --recursive`.

2. Configure and build:

   ```sh
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

   The build writes `bellwood` and its `bellwood.o2r` to `build`.

3. Run `build/bellwood` and select your disc on the first launch, as in the [Quick Start](#4-generating-the-o2r-from-the-disc-and-play).

   To generate `ac.o2r` at build time instead, point the build at your disc and build the `ExtractAssets` target:

   ```sh
   cmake -S . -B build -DAC_DISC_PATH=/path/to/disc.iso
   cmake --build build --target ExtractAssets
   ```

### Android, iOS and Web

**Android** needs the Android SDK with NDK `30.0.15729638`, CMake `3.30.3` and JDK 17 or newer. Gradle drives the native build:

```sh
android/gradlew -p android assembleRelease
```

The APK is written to `android/app/build/outputs/apk/release/`. On first launch it asks for your disc image and generates `ac.o2r` on the device. There are no on-screen controls yet, so Android and iOS need a Bluetooth or USB controller.

**iOS** needs Xcode. Add `-DIOS_SIGNING=OFF` for an unsigned build to sideload:

```sh
cmake -S . -B build-ios -G Xcode -DCMAKE_TOOLCHAIN_FILE=cmake/ios.bellwood.toolchain.cmake -DPLATFORM=OS64
cmake --build build-ios --config Release --target Bellwood
```

Copy your disc image into **Files → On My iPhone → Bellwood** and start the app. GameCube saves go in the `import` folder there.

**Web** needs the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html):

```sh
embuilder build zlib
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --target Bellwood
python3 -m http.server -d build-web 8080
```

Open `http://localhost:8080/bellwood.html` and pick your disc image. It is read in the browser, and everything the game writes is kept in the site's storage.

<a href="https://github.com/Kenix3/libultraship/">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="./docs/poweredbylus.darkmode.png">
    <img alt="Powered by libultraship" src="./docs/poweredbylus.lightmode.png">
  </picture>
</a>

# Special Thanks:

* [ACreTeam](https://github.com/ACreTeam/ac-decomp) : for the Animal Crossing decompilation this port is built on.
* [Kenix3](https://github.com/Kenix3) : for helping with the Engine development that were brought into other ports.
* [HM64 Team](https://github.com/harbourMasters) : for creating Libultraship and Torch, and helping with various issues.
