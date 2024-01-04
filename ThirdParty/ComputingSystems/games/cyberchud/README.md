# CyberChud

[CyberChud](https://scumgames.neocities.org/cyberchud) is a software 3D renderer for TempleOS written in C, compiled with gcc/clang, loaded with a HolyC ELF loader and linked to a mininum set of libc and SDL2. Desktop and Browser/WASM supported as well.

# Usage

## Run

### VMware

### qemu

#### qemu create disk

`qemu-img create -f qcow2 mytosdisk.qcow2 1G`

#### qemu install

`qemu-system-x86_64 -audiodev sdl,id=snd0 -machine pc,accel=kvm,pcspk-audiodev=snd0 -drive file=templeos.qcow2,index=0,media=disk -cdrom TempleOS_Distro.ISO -m 4G -smp 8`

`-machine pc`
TempleOS needs the older `Standard PC (i440FX + PIIX, 1996)` style machine, as opposed to the newer `q35` `Standard PC (Q35 + ICH9, 2009)` implementation

`accel=kvm`
Executing the game while using qemu's default `tcg` accelerator will result in a crash

`-smp 8`
The game is highly multithreaded, please set this to however many vcores you have.

## Build

### Build Dependencies

* deps for `holyass`
	* [assimp](https://github.com/assimp/assimp)
* deps for `ericw-tools`
	* [embree3](https://github.com/embree/embree)
	* [tbb](https://github.com/oneapi-src/oneTBB)

#### Dependency Install Example (Arch Linux)

```sh
pacman -S assimp onetbb embree3
```

### Regular Compile

```sh
git clone --recursive https://gitgud.io/CrunkLord420/cyberchud.git
mkdir build
cd build
# Release Style Build
cmake .. -GNinja -DBUILD_LIB=1 -DBUILD_MODE=Release -DSANITIZE=0 -DVERBOSE=0 -DFLTO=1 -DHW_ACCEL=1 ..
ninja
```

### WASM Compile

You will need [emsdk](https://github.com/emscripten-core/emsdk). A quick installation would look something like this.

Note: current latest emsdk 3.1.44 has a breaking bug and cannot be used: https://github.com/emscripten-core/emscripten/issues/19921

```sh
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install 3.1.42 # https://github.com/emscripten-core/emscripten/issues/19921
./emsdk activate latest
source emsdk_env.sh
```

```sh
mkdir embuild
cd embuild
emcmake cmake .. -GNinja -DBUILD_MODE=Release
ninja
```

* WASM build cannot share the same directly as a non-WASM build, so lets use `embuild` instead.
* `emcmake` will wrap `cmake` command.

### WASM Hosting

Your web server must add a CSP header like the following nginx config

```
add_header Cross-Origin-Embedder-Policy require-corp;
add_header Cross-Origin-Opener-Policy same-origin;
add_header Content-Security-Policy "script-src 'self' 'unsafe-inline' 'wasm-unsafe-eval' data:;" always;
```

## License Notices

* [cglm](https://github.com/recp/cglm)
* [libccd](https://github.com/danfis/libccd)
* [musl](https://musl.libc.org/)
* [stb_ds.h](https://github.com/nothings/stb)
* [stb_sprintf.h](https://github.com/nothings/stb)
* [FTEQW](https://www.fteqw.org/)
* [vkQuake](https://github.com/Novum/vkQuake)
* [zstd](https://github.com/facebook/zstd)

## Assets

### Original Contributions

* Slobbermutt Model by SSumma (@SSumma@anthro.cc)
* Major BSP Brushwork by WaveCruising
* Minor BSP Brushwork by Token (@coin@asimon.org)

### Fonts

* [Matchup by somepx](https://somepx.itch.io/humble-fonts-free)
* [Pinzelan by GGBotNet](https://ggbot.itch.io/pinzelan-font)

### Models

* 11.10melons: https://sketchfab.com/3d-models/submachine-gun-model-5635984ac39d4a01967db587adeff09f (CC BY 4.0)
* Shedmon: https://sketchfab.com/3d-models/double-barreled-shotgun-99734ee367dc4872b6183412c9d76a4c (CC BY 4.0)
* sc8di: https://sketchfab.com/3d-models/capsule-gun-3639c42106334abab57d0592dae745df (CC BY 4.0)

### Models Maybe

* Andrei Milin: https://sketchfab.com/3d-models/lazer-gun-aep-9-4603956c1f824e49a1ffe8c6263317cd (CC BY 4.0)
* Andrei Milin: https://sketchfab.com/3d-models/revolver-rs-baikal-9ce9af8811c345ba8da22c931c73e53b (CC BY 4.0)
* Andrei Milin: https://sketchfab.com/3d-models/sci-fi-pistol-fab28aeb14894752b49149ef00926aff (CC BY 4.0)
* https://sketchfab.com/3d-models/9-mm-5124e7fe60fb4d3ab62460609d23f365
* https://sketchfab.com/3d-models/racecar-lowpoly-45840e2136c44080b4c1e7521cce8db3
* https://sketchfab.com/3d-models/shotgun-shells-1807bebe90c64dbaaf60d6de50eb0a3d
* NksXxX: https://sketchfab.com/3d-models/gun-8530f957dae0409bbbaa4ffea0bd7efe (CC BY 4.0)

### Texture Baked Models

* DHCGelectron: https://sketchfab.com/3d-models/cyberpunk-trash-bin-a2114dcd728a4d7abd105780fd4bb2d3 (CC BY 4.0)
* Spellkaze: https://sketchfab.com/3d-models/server-rack-62f6779cb7e448b19aaf58544c3c7218 (CC BY 4.0)

### Misc 

* [BMZ cursor by HERRBATKA](https://www.pling.com/p/1158321)

## Tool Dependencies

* [assimp](https://github.com/assimp/assimp)
* [lodepng](https://github.com/lvandeve/lodepng)
* [ericw-tools](https://github.com/ericwa/ericw-tools)

## Snippets & Educational

* [LearnOpenGL](https://learnopengl.com/)
* [olcConsoleGameEngine](https://github.com/OneLoneCoder/Javidx9)
* [tinyrenderer](https://github.com/ssloy/tinyrenderer)
* [scratchapixel](https://www.scratchapixel.com/)
* [Cem Yuksel](https://www.youtube.com/@cem_yuksel/videos)
* [Real-Time Collision Detection by Christer Ericson](https://realtimecollisiondetection.net/)

## Extra Special Thanks

Alec Murphy's ELF/LibC/SDL library was used as the proof of concept for this project. These libraries have been heavily modified, in particular the ELF loader will now properly relocate the object into memory.

## License

[AGPL-3.0-only+NIGGER](https://plusnigger.autism.exposed/)

*[MIT+NIGGER](https://plusnigger.autism.exposed/) for applicable code (see LICENSE for details)*
