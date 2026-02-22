# KobraDoom — Anycubic Kobra 2 Pro

[https://www.youtube.com/embed/vQZF1pdODIo?si=75__o_qgdN5cou42](https://youtu.be/vQZF1pdODIo?si=UrtsiVXG3oMMBpgu)

A fork of the [milkpirate/kobradoom](https://github.com/milkpirate/kobradoom) project, specially adapted for the **Anycubic Kobra 2 Pro** 3D printer.

### Important note from the author
> I am not a programmer, just an enthusiast who wanted to run DOOM on my 3D printer. All the code was adapted with heavy support from AI. My goal wasn't to do everything "perfectly and according to best practices" — the main objective was just to get it to launch and work.

## What was it tested on
* **Printer:** Anycubic Kobra 2 Pro
* **Firmware:** Stock version **2.3.9** (rooted).
* *Note:* It was tested **only** on this specific configuration. There is a chance it might work on other firmwares/Anycubic printers, but there are no guarantees.

## Why was this fork needed?
The original `milkpirate/kobradoom` project displayed a black screen when launched on my printer. To fix this and adapt the game for the Kobra 2 Pro, I made the following changes:
1. **Video output:** The graphics output code was changed so the image displays correctly on the printer's screen.
2. **Resolution:** Adapted to the native resolution of the Kobra 2 Pro display.
3. **Controls:** Input is tied to `/dev/input/event1` (a connected USB keyboard).

## How to run
You will need the executable file and the game resource file (WAD).

1. Plug a standard USB keyboard directly into the printer's USB port.
2. Put the `kobradoom` executable and the `doom1.wad` file in the same directory on the printer (e.g., via SSH or SFTP).
3. Connect to the printer via SSH.
4. Navigate to the folder with the files and run the game:
   ```bash
   ./kobradoom
   ```

## Building from source
If you want to compile the project yourself, you will need an ARM cross-compiler. I built this on my PC using a toolchain for Rockchip processors.

**Build commands:**
```bash
cd doomgeneric
make -f Makefile.kobra LDFLAGS="-static" CC=arm-rockchip830-linux-uclibcgnueabihf-gcc CXX=arm-rockchip830-linux-uclibcgnueabihf-g++
```

## Credits
* Thanks to **milkpirate** for the [original port](https://github.com/milkpirate/kobradoom), which served as a great base.
* To the Artificial Intelligence, without which this project wouldn't exist.
