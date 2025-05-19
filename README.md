# MultiPLAY

A media player for several structured formats with an eye to audio quality.

## What it plays

MultiPLAY supports several formats, including the more common "module" or tracker files.

### Impulse Tracker

Probably the best-known tracker, Impulse Tracker, created by Jeffrey Lim, creates `*.it` files. Impulse Tracker is, intractably, a DOS program written in x86 assembler, but in recent years, a completely open-source cross-platform rewrite that aims to be a clone of the interface and features was created: <a href="https://schismtracker.org">Schism Tracker</a> by Storlek, Mrs. Brisby and Paper. MultiPLAY can play `*.it` files.

### Scream Tracker

The inspiration for Impulse Tracker in the first place was the first major break-away from the first generation Amiga trackers, Scream Tracker, which was created by Sammi Tammilehto (Psi) of the Future Crew. MultiPLAY can play `*.s3m` files, the format created by ScreamTracker 3.

### MultiTracker

The demoscene group Renaissance created a format and player that could handle realtime playback of up to 32 mixed channels on PCs. MultiPLAY can play `*.mtm` files.

### Amiga Modules

The original gangster of music modules, the `*.mod` file, was introduced with Ultimate Soundtracker for the Amiga by Karsten Obarski in 1987. MultiPLAY can play `*.mod` files.

### PLAY Strings

This is actually what inspired the creation of MultiPLAY in the first place: A tool for playing back BASICA/QBASIC `PLAY` statement strings.

#### Multiple PLAY Strings

The reason it's called MultiPLAY and not SinglePLAY is that it can play back and mix together multiple `PLAY` statements simultaneously. `PLAY` statements are monophonic, but at some point during my grade school years, I came across (or maybe transcribed myself? :-) the Super Mario Brothers theme song in 3-note polyphony, expressed as 3 separate QBASIC programs. If you had 3 computers side-by-side, you could load one of them on each, and then with the help of some friends, press F5 simultaneously on all 3 to play it back. MultiPLAY was written to do this in one go.

#### Instruments

After support was added to MultiPLAY for module files, which represent instruments using sample files, the `PLAY` statement support was updated to be able to use a sampled instrument instead of a simple synthesized waveform.

## How it makes sound

### Raw files

The original implementation of MultiPLAY was made to produce raw audio files. These files no header and default to 16-bit little-endian samples in stereo. This support is still present in the latest code, and can be accessed with the `-output` command-line option.

#### Formats

Command-line options `-stereo` and `-mono` can be used to force 2- or 1-channel output
Command-line options `-lsb` and `-msb` can be used to switch between byte orders for 16-bit output
Command-line options `-64`, `-32`, `-16` and `-8` can be used to set the output sample size (but why would you ever want to use 8-bit samples? :-). The 64- and 32-bit sizes use IEEE floating-point encoding, while the 16- and 8-bit sizes are integers.
Command-line options `-signed` and `-unsigned` can be used to set whether 16- or 8-bit output uses signed or unsigned representation.
Command-line options `-ulaw` and `-alaw` can be used to enable compression with either μ-law or A-law. These are the companding algorithms used by the telephone network.

#### Using

What can you with a raw audio file? Well, at least two things:

* Load them into an audio editor, such as Audacity.

    In Audacity, there is a menu option Import->Raw Data. This can read and interpret the raw data produced by MultiPLAY.

* Provide them as input to FFMPEG.

    The open-source swiss army knife FFMPEG can take raw audio data as input, both to `ffmpeg` and `ffplay`. For instance:

    ```
    ffplay -f s16le output.raw
    ```

### DirectX

When built with the optional DirectX support enabled on Windows, the `-directx` command-line option can be used to use DirectX to emit audio.

### SDL

When built with the optional SDL support enabled (any platform), the `-sdl` command-line option can be used to use SDL to emit audio.

### Default

If DirectX support is enabled, it becomes the default. If SDL support is enabled, it becomes the default. If both DirectX and SDL are enabled, SDL is preferred.

## Supported operating systems

MultiPLAY was originally written and tested on Windows, but was subsequently modified to be able to compile on Linux. It can output sound using DirectX (Windows) or SDL (Windows, Linux, OS X, perhaps others).

### Windows

Building MultiPLAY on Windows requires Microsoft's Visual C++ compiler (though not necessarily the IDE). The configuration targets Platform Toolset v143, which corresponds to Visual Studio 2022. You can either use the Visual Studio 2022 GUI, or you can build from the command-line with one of these commands:
```
msbuild MultiPLAY.vcxproj /p:Target="x64" /p:Configuration="Debug"
msbuild MultiPLAY.vcxproj /p:Target="x64" /p:Configuration="Debug DirectX"
msbuild MultiPLAY.vcxproj /p:Target="x64" /p:Configuration="Debug SDL"
msbuild MultiPLAY.vcxproj /p:Target="x64" /p:Configuration="Release"
msbuild MultiPLAY.vcxproj /p:Target="x64" /p:Configuration="Release DirectX"
msbuild MultiPLAY.vcxproj /p:Target="x64" /p:Configuration="Release SDL"
msbuild MultiPLAY.vcxproj /p:Target="x64" /p:Configuration="Release All"
```
If an SDL configuration is selected, the current `MultiPLAY.vcxproj` file assumes the SDL2 libraries can be found in the folder `X:\SDL2-2.32.6`. You may need to alter this. :-)

#### Visual C++

You can load the `MultiPLAY.sln` file into Visual Studio 2022 to build within the IDE. The default profile "Release All" builds with both DirectX and SDL direct output options (selectable via command-line). There are also profiles that enable only one or the other, and a "Bare" profile that can only output raw files, and also "Debug" options that disable optimizations and enable symbols.

#### Manual

All you need to do is:
```
msbuild MultiPLAY.vcxproj /p:Target=x64 /p:Configuration="Release All"
```
(you may need to place SDL files into a specific folder or update the project file to point at the right place)

You can also build with only one output engine, or with no direct output at all (only raw file output supported):
```
msbuild MultiPLAY.vcxproj /p:Target=x64 /p:Configuration="Release DirectX"
msbuild MultiPLAY.vcxproj /p:Target=x64 /p:Configuration="Release SDL"
msbuild MultiPLAY.vcxproj /p:Target=x64 /p:Configuration="Release Bare"
```

And of course, there are "Debug" profiles as well:
```
msbuild MultiPLAY.vcxproj /p:Target=x64 /p:Configuration="Debug DirectX"
msbuild MultiPLAY.vcxproj /p:Target=x64 /p:Configuration="Debug SDL"
msbuild MultiPLAY.vcxproj /p:Target=x64 /p:Configuration="Debug"
```

### Linux / OS X / other UNIX-y systems

The supplied `Makefile` by default assumes there is a competent compiler called `g++`. It should handle the case where `g++` is really `clang++` in disguise. The default profile also assumes that you will be using SDL for direct output and have the development libraries for SDL installed. If you meet these criteria, simply execute:
```
make
```

If you want to override the name of the compiler, you can specify this with a Make variable:
```
make CXX=clang++
```

If you want a build that doesn't require SDL (raw file output only), you can use the "bare" target:
```
make bare
make bare CXX=clang++
```

You can also create a debug build with symbols using the "debug" target":
```
make debug
```

Finally, there is a configuration that enables warnings up the wazoo called "lint" that can be used to polish the code quality:
```
make lint
```

Build output is placed in a folder named after the configuration:
```
./bin/release/MultiPLAY
./bin/debug/MultiPLAY
./bin/bare/MultiPLAY
./bin/lint/MultiPLAY
```

#### Visual Studio Code

There are `launch.json` and `tasks.json` files in the `.vscode` subdirectory that allow integration with Visual Studio Code, which provides a decent debugging experience using GDB as the underlying engine. Make sure you have the "C/C++" extension installed. You can configure the arguments for a debug run within the `launch.json` file.

## Using MultiPLAY

Run `MultiPLAY` with no command-line arguments to get a comprehensive usage summary.

If you have one or more text files containing BASICA/QBASIC `PLAY` statement syntax, you can play them with a command such as:
```
./bin/release/MultiPLAY -play first.txt -play second.txt
```

If you have a module file, you can play it with a command such as:
```
./bin/release/MultiPLAY -module k_2deep.s3m
```

## Sample Files

_'Sample' in the sense of files to test out MultiPLAY_

This repository has links to a quick grab bag of module files and a few `PLAY` syntax files. To avoid ballooning the Git repository with binary files, these are stored in Git-LFS. Install the Git-LFS extensions and then clone the repository and it should automatically download them into a subdirectory called `Samples`. Some of these are real bangers, others are included because they triggered bugs and thus were good tools for debugging.

I really like `k_2deep.s3m`, personally. :-)
