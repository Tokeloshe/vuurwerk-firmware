# Building VUURWERK

VUURWERK builds like its egzumer parent: an ARM GCC cross-toolchain and GNU
Make. The documented path is the Docker build from the `Dockerfile` in the
repo, driven by `compile-with-docker.sh` or `compile-with-docker.bat`. It
gives you a raw flash image and a packed image for the browser flasher.

One honest note before you start. The release binaries were built with a
pinned toolchain image. A different compiler, including the one a freshly
built Docker image installs today, can produce a slightly different and
sometimes larger image, and the firmware is close to the top of the flash.
If your build does not fit on unchanged source, that is the compiler, not
you. The shipped binary in `release/` is the reference; build from source
when you want to change something.

## Build paths at a glance

| Path | What you need | Matches the release bytes? |
|---|---|---|
| Docker (`Dockerfile`) | Docker | Not guaranteed; may differ slightly |
| Native Linux or macOS | `arm-none-eabi-gcc`, `make`, Python 3 | Not guaranteed; may differ slightly |
| Native Windows | ARM GCC, GNU Make, `win_make.bat` | Not guaranteed; may differ slightly |

Only the maintainer's pinned image reproduces the release byte for byte.
Every path above builds working firmware from the same source.

## The build flow

```
 +-------------+     +----------------+     +----------------------+
 | source tree |     | toolchain      |     | make                 |
 | (this repo) | --> | (Docker image  | --> | arm-none-eabi-gcc    |
 |             |     |  or native)    |     | warnings are errors  |
 +-------------+     +----------------+     +----------+-----------+
                                                       |
        +-------------------------+--------------------+
        v                         v                    v
  vuurwerk-v1.5.1        vuurwerk-v1.5.1.bin    vuurwerk-v1.5.1.packed.bin
  (ELF: size, debug)     (raw image: k5prog)    (for the web flasher)
```

The `.packed.bin` is produced by `fw-pack.py` (the Quansheng updater format
with a version header and CRC) and needs Python with the `crcmod` module.
The Docker image has it already.

## Build with Docker

1. **Install Docker.** On Windows, Docker inside WSL Ubuntu works well; run
   the steps from the WSL shell, or use the `.bat` from a normal prompt with
   Docker Desktop.
2. **Clone the repository** and open a shell in its root.
3. **Run the script.** On Linux, macOS or WSL: `./compile-with-docker.sh`.
   On Windows: `compile-with-docker.bat`. The script builds a fresh image
   tagged `uvk5` from the `Dockerfile` (replacing any image already tagged
   `uvk5`), runs `make` inside it, and copies the results out.
4. **Collect the output** in `compiled-firmware/`: `firmware.bin` (raw image)
   and `firmware.packed.bin` (for the browser flasher).

The first run downloads and installs the toolchain and takes a few minutes;
later runs are quick.

## Native build, Linux and macOS

```bash
# Ubuntu / Debian
sudo apt-get install gcc-arm-none-eabi make python3 python3-pip
pip install crcmod

# macOS
brew install --cask gcc-arm-embedded
brew install make

make clean
make
```

The output lands in the repo root as `vuurwerk-v1.5.1`,
`vuurwerk-v1.5.1.bin` and `vuurwerk-v1.5.1.packed.bin`.

## Native build, Windows

`win_make.bat` documents the direct-on-Windows path: install ARM GCC (from
the ARM developer downloads) and GNU Make 3.81 (gnuwin32), make sure both are
on `PATH`, then run the batch file from the repo root. Python 3 with `crcmod`
is optional: without it you get the raw `.bin` but no `.packed.bin`.

## Build output

| File | What it is |
|---|---|
| `vuurwerk-v1.5.1` (or `firmware`) | ELF executable, for `arm-none-eabi-size` and debugging |
| `vuurwerk-v1.5.1.bin` (or `firmware.bin`) | Raw flash image, for k5prog and similar tools |
| `vuurwerk-v1.5.1.packed.bin` (or `firmware.packed.bin`) | Packed image for the browser flasher |

The Docker script names its copies `firmware*`; a native `make` names them
after the version.

## Size check

The flash holds 61,440 bytes. VUURWERK v1.5.1 uses 60,216 of them, leaving
1,224 free. The linker refuses an image that does not fit, so **if it links,
it fits**.

```bash
arm-none-eabi-size vuurwerk-v1.5.1
```

Watch the `text + data` total against 61,440. There is little room for
casual additions: if you add code, expect to trim something elsewhere. If
the link fails on unchanged source, your compiler made a larger image than
the release toolchain did; use the Docker path.

## Build options

The feature switches live at the top of the `Makefile` as `ENABLE_...`
variables. The shipped configuration is the supported one; switching a
feature off may orphan code that expects it. These are the switches that
shape the shipped build:

| Switch | Shipped | What it controls |
|---|---|---|
| `ENABLE_SPECTRUM` | 1 | The spectrum analyzer, VOX mode and PATROL |
| `ENABLE_FLASHLIGHT` | 1 | The flashlight and its auto-off |
| `ENABLE_RSSI_BAR` | 1 | The signal bar on the centre line |
| `ENABLE_AM_FIX` | 1 | Strong-signal protection in AM |
| `ENABLE_ADAPTIVE_SCAN` | 1 | Fast skipping of empty channels while scanning |
| `ENABLE_SCAN_RANGES` | 1 | Frequency range scanning |
| `ENABLE_WIDE_RX` | 1 | Receive from 18 MHz to 1.3 GHz |
| `ENABLE_UART` | 1 | CHIRP and programming over the cable |
| `ENABLE_NO_CODE_SCAN_TIMEOUT` | 1 | Drops the stock tone-scan timeout; VUURWERK's own 30 s give-up applies either way |
| `ENABLE_CUSTOM_MENU_LAYOUT` | 1 | The categorised menu |
| `ENABLE_NOAA`, `ENABLE_FMRADIO`, `ENABLE_VOICE`, `ENABLE_VOX`, `ENABLE_ALARM`, `ENABLE_DTMF_CALLING` | 0 | Stock features left out of VUURWERK |

The version string is set in the same place: `VERSION_STRING ?= v1.5.1`.
Warnings are treated as errors, so a clean build is warning-free.

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `arm-none-eabi-gcc: command not found` | The toolchain is not on `PATH` | Use the Docker build, or fix your `PATH` |
| Link fails with a region-overflow error | Your compiler made a larger image, or your change does not fit | Use the Docker path; if it still overflows, trim your change |
| `make` inside the container fails strangely | Stale `.o` and `.d` files from an earlier native build | Run `make clean` (or delete them) and build again |
| `.packed.bin` was not built | Python or `crcmod` is missing | `pip install crcmod`; the raw `.bin` is still valid |
| `docker: permission denied` | Your user is not in the `docker` group | Add yourself to it, or run the script with `sudo` |

## Flashing what you built

See [FLASHING.md](FLASHING.md). The browser flasher takes the `.packed.bin`;
k5prog takes the raw `.bin`. For what the firmware does once it is on the
radio, start at [README.md](README.md).
