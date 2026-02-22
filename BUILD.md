# VUURWERK v1.0.0 - Build Instructions

## Prerequisites

### Windows

1. **ARM GCC Toolchain 14.2.1**
   - Download from: https://developer.arm.com/downloads/-/gnu-rm
   - Install to: `C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\`

2. **GNU Make 3.81**
   - Download from: https://gnuwin32.sourceforge.net/packages/make.htm
   - Install to: `C:\Program Files (x86)\GnuWin32\`

3. **Python 3.8+** (optional, for packed .bin)
   - Install crcmod: `pip install crcmod`

### Linux/Mac

```bash
# Ubuntu/Debian
sudo apt-get install gcc-arm-none-eabi make

# macOS
brew install --cask gcc-arm-embedded
brew install make
```

## Building

### Windows (Command Prompt)

```batch
cd Vuurwerk
set PATH=C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin;%PATH%
set PATH=C:\Program Files (x86)\GnuWin32\bin;%PATH%
make clean
make
```

### Windows (Bash/MSYS2)

```bash
cd Vuurwerk
export PATH="/c/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin:$PATH"
export PATH="/c/Program Files (x86)/GnuWin32/bin:$PATH"
make clean
make
```

### Linux/Mac

```bash
cd Vuurwerk
make clean
make
```

## Build Output

Successful build produces:
- `vuurwerk-v1.0.0` - ELF executable
- `vuurwerk-v1.0.0.bin` - Flash-ready binary

## Size Verification

```bash
arm-none-eabi-size vuurwerk-v1.0.0
```

Expected output:
```
   text	   data	    bss	    dec	    hex	filename
  57772	    120	   3620	  61512	   f048	vuurwerk-v1.0.0
```

**Flash usage**: 57,772 / 65,536 bytes (88.2%)
**Free flash**: 7,764 bytes (11.8%)

## Build Options

All features are enabled by default in VUURWERK. To disable specific features, edit Makefile:

```makefile
# Disable spectrum analyzer
ENABLE_SPECTRUM ?= 0

# Disable FM radio
ENABLE_FMRADIO ?= 0
```

**Note**: Disabling features may break VUURWERK functionality.

## Compilation Flags

- **Optimization**: `-Os` (size)
- **LTO**: Enabled (`-flto=auto`)
- **Standard**: C2x (`-std=c2x`)
- **Warnings**: All warnings are errors (`-Werror`)

## Troubleshooting

**"make: command not found"**:
- Ensure make is in PATH
- Use full path to make.exe

**"arm-none-eabi-gcc: command not found"**:
- Ensure ARM toolchain is in PATH
- Verify installation path

**"Error 1" / Compilation errors**:
- Check toolchain version (requires 14.2.1)
- Ensure all source files are present
- Try `make clean` first

**Flash overflow**:
- Should not occur with default VUURWERK
- If modified, reduce features per master prompt cut order

## Clean Build

```bash
make clean
make
```

## Development

- **Source layout**: Flat structure in `Vuurwerk/`
- **Modules**: VUURWERK modules have no subdirectories
- **Headers**: All `.h` files in root or subdirs
- **Firmware**: Links with `firmware.ld` linker script

## CI/CD

To integrate with CI:

```yaml
# GitHub Actions example
- name: Install ARM Toolchain
  run: |
    wget https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
    tar xf arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
    
- name: Build
  run: |
    export PATH=$PWD/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi/bin:$PATH
    cd Vuurwerk
    make
    
- name: Check Size
  run: |
    export PATH=$PWD/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi/bin:$PATH
    cd Vuurwerk
    arm-none-eabi-size vuurwerk-v1.0.0
```
