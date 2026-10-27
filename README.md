# FroggyScummVM

**FroggyScummVM** is a standalone, hardware-tailored port of **ScummVM** for the **Data Frog SF2000** and **GB300 / GB300 V2** handheld retro consoles running the MIPS32r2 SoC.

It integrates seamlessly with **FrogUI** and the multicore loader, featuring direct game launching via `.gba` stubs, custom handheld joypad-to-virtual-mouse mapping, full audio mixing, and 320×200/240 native pixel scaling.

---

## Features

- **Standalone Execution:** Runs natively on bare metal via `core_87000000` / `scummvm.sf2k` / `scummvm.gb3k` without requiring RetroArch.
- **FrogUI Integration:** Launches directly from the FrogUI game list with cover thumbnails (`.rgb565`) and auto-detection.
- **Direct Joypad Mouse Control:** Smooth, multi-tiered virtual mouse emulation with cursor acceleration driven directly by the console D-Pad.
- **Tailored Memory Footprint:** Optimized lightweight build using ~11 MB of RAM, leaving ~47 MB of heap room on the console's memory pool.
- **Full Sound Support:** Real-time software MIDI / AdLib / OPL synthesis and digital voice / SFX playback downmixed for the console's mono speaker / headphone jack.
- **Supported Engines (Lite Embedded Set):**
  - **SCUMM v1 – v8:** *The Secret of Monkey Island*, *Monkey Island 2: LeChuck's Revenge*, *Indiana Jones and the Fate of Atlantis*, *Day of the Tentacle*, *Sam & Max Hit the Road*, *Full Throttle*, *The Dig*, *The Curse of Monkey Island*.
  - **SKY:** *Beneath a Steel Sky*.
  - **AGI:** Classic Sierra adventure titles (*King's Quest*, *Space Quest*, *Leisure Suit Larry*).
  - **Queen:** *Flight of the Amazon Queen*.
  - **Gob:** *Gobliiins* series.

---

## Controls

The handheld controls are mapped directly to ScummVM inputs in real time:

| Handheld Button | Function |
|---|---|
| **D-Pad (Up / Down / Left / Right)** | Move Virtual Mouse Cursor (adaptive acceleration) |
| **A** or **L** | Left Mouse Button (Click / Select / Interact) |
| **B** or **R** | Right Mouse Button (Examine / Cancel / Context Menu) |
| **Start** | ScummVM Main Menu (Save / Load / Settings) |
| **Select** or **X** | Space Bar (Skip dialogue lines) |
| **Y** | Escape key (Pause / Skip cutscenes) |

---

## Installation

### Method 1: Using Pre-built Release Packages (Recommended)

1. Download the release package matching your console:
   - `scummvm-sf2000-sd.zip` for **Data Frog SF2000**
   - `scummvm-gb300-sd.zip` for **GB300 / GB300 V2**
2. Extract the archive directly to the root directory of your console's microSD card.
3. The archive automatically populates:
   - `cores/scummvm/` and `system/Deimos/cores/` (Core binaries)
   - `configs/custom_cores/scummvm/games.opt` (FrogUI core mappings)
   - `ROMS/scummvm/` (Launchers, games, and `.rgb565` cover art)
   - `bios/bisrv.asd` (Multicore patched BIOS)
4. Insert the microSD card into the console and boot.
5. Navigate to the **ScummVM** section in FrogUI or open `ROMS/scummvm/` and launch any game with **A**.

---

### Method 2: Automatic Installer Script

If you build from source or have the repository cloned:

```bash
# Insert your microSD card and run:
./install_to_sd.sh /media/<your_user>/<SD_CARD_MOUNT_POINT>
```

---

## Adding New Games

1. Create a dedicated folder for your game inside `ROMS/scummvm/<game_folder>/`:
   ```
   ROMS/scummvm/monkey2/
   ```
2. Copy your game data files into that directory.
3. Create a launcher file in `ROMS/scummvm/<GameName>.gba` containing a single line:
   ```text
   scummvm;scummvm/<game_folder>;<startup_file>.
   ```
   *(Note the trailing dot `.` at the end of the filename — it ensures multicore argument parsing preserves file extensions).*

   **Example:**
   ```text
   scummvm;scummvm/monkey2;monkey2.001.
   ```
4. *(Optional)* Add a 160×160 RGB565 raw thumbnail named `<GameName>.rgb565` for FrogUI box art.
5. In `configs/custom_cores/scummvm/games.opt`, add your launcher name:
   ```text
   <GameName>.gba|scummvm
   ```

---

## Building from Source

### Prerequisites

- Linux (x86_64)
- Frog MIPS Toolchain (`mipsel-mti-elf-gcc`, `mipsel-mti-elf-g++`) installed in `/opt/frog-toolchain/mipsel-mti-elf_stable/bin`
- Host build tools: `gcc`, `g++`, `make`, `python3`, `ffmpeg`

### Compile Core & Runner

```bash
# 1. Compile ScummVM static library for SF2000/GB300:
make -f Makefile.port core

# 2. Build the PC test runner:
make -f Makefile.port runner

# 3. Test on PC and produce frame dumps / screenshots:
make -f Makefile.port test

# 4. Build final console packages (.sf2k / .gb3k and SD distribution):
make -f Makefile.port device
```

---

## Architecture & Technical Details

1. **Bare-Metal Coroutine Context Switching (`sjlj_sf2000.c`):**
   ScummVM runs its virtual machine threads using coroutines (`libco`). The standard MIPS `setjmp`/`longjmp` implementation was replaced with `sjlj_sf2000.c`, strictly adhering to MIPS O32 ABI requirements with 16-byte stack frame alignment and parameter save reserves.
2. **Deterministic Framebuffer Delivery:**
   Forces pure software RGB565 rendering (`VIDEO_GRAPHIC_MODE_REQUEST_SW`) avoiding OpenGL HW context calls that cause white-screen lockups on non-accelerated framebuffers.
3. **Null-Safe Initial State:**
   Guards early input polling and window metric queries prior to the first graphic surface commit, preventing `nullptr` exceptions during bootup.

---

## License & Credits

- **ScummVM:** Distributed under the terms of the **GNU General Public License v3.0 (GPLv3)**. See `COPYING` for full details.
- **Multicore / FrogUI:** Built with the Data Frog SF2000 / GB300 open-source development ecosystem.
- Individual games, logos, and trademarks belong to their respective copyright holders.
