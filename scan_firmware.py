#!/usr/bin/env python3
"""
Scan a binary file for ARM Cortex-M firmware patterns.
Usage: python3 scan_firmware.py <file>
"""

import sys
import struct

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 scan_firmware.py <file>")
        sys.exit(1)
    
    filename = sys.argv[1]
    
    print(f"Reading: {filename}")
    with open(filename, "rb") as f:
        data = f.read()
    
    print(f"File size: {len(data)} bytes ({len(data)/1024:.1f} KB)")
    
    # Search for ARM Cortex-M vector table patterns
    # First word should be initial SP (0x20xxxxxx for NUC123)
    # Second word should be reset handler (0x0000xxxx or 0x0010xxxx, Thumb bit set)
    
    found = []
    for i in range(0, len(data) - 32, 4):
        sp = struct.unpack('<I', data[i:i+4])[0]
        reset = struct.unpack('<I', data[i+4:i+8])[0]
        
        # Check for valid NUC123 vector table
        # SP should be in SRAM range (0x20000000 for NUC123)
        sp_valid = (sp & 0xF0000000) == 0x20000000
        
        # Reset handler should be in flash with Thumb bit set (LSB = 1)
        reset_valid = (reset & 0xFFFFFF01) == 0x00000001
        
        if sp_valid and reset_valid:
            # Verify more vector table entries
            if len(data) >= i + 32:
                nmi = struct.unpack('<I', data[i+8:i+12])[0]
                hardfault = struct.unpack('<I', data[i+12:i+16])[0]
                memmanage = struct.unpack('<I', data[i+16:i+20])[0]
                busfault = struct.unpack('<I', data[i+20:i+24])[0]
                usagefault = struct.unpack('<I', data[i+24:i+28])[0]
                svcall = struct.unpack('<I', data[i+28:i+32])[0]
                
                # Check if exception handlers are in valid flash range
                all_valid = True
                handlers = [nmi, hardfault, memmanage, busfault, usagefault, svcall]
                for h in handlers:
                    # Should be in flash (0x0000xxxx or 0x0010xxxx) with Thumb bit
                    if (h & 0xFFFFFF01) not in [0x00000001, 0x00100001]:
                        all_valid = False
                        break
                
                if all_valid:
                    found.append((i, sp, reset, nmi, hardfault))
    
    print(f"\nFound {len(found)} potential ARM firmware locations:")
    
    for idx, (offset, sp, reset, nmi, hf) in enumerate(found):
        print(f"\n  [{idx}] Offset: {hex(offset)} ({offset} bytes)")
        print(f"      Initial SP: {hex(sp)}")
        print(f"      Reset Handler: {hex(reset)}")
        print(f"      NMI: {hex(nmi)}")
        print(f"      HardFault: {hex(hf)}")
        
        # Estimate firmware size based on reset handler position
        if reset < 0x00100000:
            # Firmware is in APROM (0x00000000)
            estimated_size = 0x10000  # 64KB typical APROM
        else:
            # Firmware might be in LDROM (0x00100000)
            estimated_size = 0x10000  # 64KB
        
        # Extract potential firmware
        firmware_data = data[offset:offset + min(estimated_size, len(data) - offset)]
        out_filename = f"firmware_candidate_{idx}_offset{hex(offset)}.bin"
        
        with open(out_filename, "wb") as f:
            f.write(firmware_data)
        
        print(f"      Saved: {out_filename} ({len(firmware_data)} bytes)")
        
        # Show first 64 bytes as hex
        print(f"      First 64 bytes:")
        for j in range(0, min(64, len(firmware_data)), 16):
            hex_str = " ".join(f"{b:02x}" for b in firmware_data[j:j+16])
            print(f"        {hex(offset + j)}: {hex_str}")
    
    if not found:
        print("\nNo ARM Cortex-M firmware patterns found.")
        print("\nThe firmware may be:")
        print("  - Compressed")
        print("  - Encrypted")
        print("  - Stored in a custom format")
        print("  - Not embedded in this executable")
        print("\nAlternative approaches:")
        print("  1. Run the executable in a VM and capture USB traffic")
        print("  2. Use a debugger to trace the flashing process")
        print("  3. Check if vendor provides standalone firmware files")
        print("  4. Use the keyboard's DFU mode to dump existing firmware")

if __name__ == "__main__":
    main()