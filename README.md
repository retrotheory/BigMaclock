BigMaclock 1.0 - RetroTheory 2026
---------------------------------


![alt text](https://github.com/retrotheory/BigMaclock/blob/main/bigmaclock.jpg "BigMaclock running on compact macs")

BigMaclock is a classic full-screen desk clock for early Macintosh systems (optimized for the Macintosh compacts). 
It provides a clean, high-contrast display that is easy to read from across a room.

Features:
- Large digitized time display with blinking colon.
- Date and Day of the week in a custom small font.
- Double-buffered rendering for a flicker-free experience.
- Inversion and Gray Background modes for different lighting conditions.
- Alarm system with customizable time and multiple alert sounds.
- Face mode (Happy Mac) toggle.
- Dynamic Resolution Scaling: Automatically detects screen size (Compact Mac 512x342 vs. Hi-Res 640x480) and scales the digits and layout to fit.

Keyboard Shortcuts:
- 'H': Toggle Happy Mac mode.
- 'I': Toggle Inverted display (Black background).
- 'G': Toggle Gray background.
- 'Q': Quit the program.

Operation:
- Reveal the Menu Bar: Move the mouse to the very top of the screen (within 20 pixels). 
  The menu bar will slide down and stay visible for 4 seconds.
- Sticky Menu: Click anywhere on the screen (when the menu is hidden) to reveal the menu bar 
  and keep it "sticky" until the next click.
- Preferences: Access the Apple Menu -> Preferences to set the alarm and display options.
- Hiding Cursor: The cursor will automatically hide after 2 seconds of inactivity to keep the clock face clean.




Building BigMaclock
Prerequisites
1. Retro68 toolchain - Download and build from: https://github.com/autc04/Retro68
2. CMake - Install via your system's package manager
Setup
1. Clone the repository and navigate to it:
      cd bigclock
   
2. Set the path to your Retro68 installation. Replace /path/to/retro68/toolchain with your actual path:
      export RETRO68_ROOT=/path/to/retro68/toolchain
   
Building
1. Create and enter the build directory:
      rm -rf build && mkdir build && cd build
   
2. Run CMake with the Retro68 toolchain:
      cmake -DCMAKE_TOOLCHAIN_FILE="$RETRO68_ROOT/m68k-apple-macos/cmake/retro68.toolchain.cmake" ..
   
3. Build the project:
      make
   
The output will be in the build/ directory (.dsk disk image, .APPL application).
