# VUURWERK v1.0.0 - Flashing Instructions

## Requirements

- Quansheng UV-K5, K5(8), or K6 radio
- USB programming cable
- Flashing software: k5prog, k5prog-win, or compatible

## Step-by-Step Instructions

### 1. Backup Current Firmware (Recommended)

```bash
k5prog -r backup.bin
```

### 2. Power Off Radio

Turn off the radio completely before connecting.

### 3. Connect USB Cable

Connect the USB programming cable to both radio and computer.

### 4. Flash Firmware

**Using k5prog (Linux/Mac)**:
```bash
k5prog -F -YYY -b vuurwerk-v1.0.0.bin
```

**Using k5prog-win (Windows)**:
1. Open k5prog-win GUI
2. Select `vuurwerk-v1.0.0.bin`
3. Click "Write to Radio"
4. Wait for completion

**Using other tools**:
Consult your tool's documentation. VUURWERK provides an unpacked .bin file compatible with most tools.

### 5. Power Cycle

Turn radio off, wait 5 seconds, turn on.

### 6. Verify

You should see the VUURWERK boot screen with version v1.0.0.

## Troubleshooting

**"Device not found"**: 
- Check USB cable connection
- Try different USB port
- Ensure radio is powered off before connecting

**"Write failed"**:
- Use `-F` flag to force write (k5prog)
- Ensure cable is genuine/good quality
- Try slower baud rate if supported

**Radio won't boot**:
- Reflash stock firmware from backup
- Contact support if persistent

## Reverting to Stock

Flash your backup.bin or download stock firmware from Quansheng and flash normally.

## Notes

- VUURWERK does NOT support packed .bin format by default
- Most modern tools accept unpacked .bin files
- Total flash time: ~30 seconds
- Do NOT disconnect during flashing
