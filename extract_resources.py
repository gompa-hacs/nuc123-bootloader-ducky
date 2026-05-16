#!/usr/bin/env python3
"""
Extract resources from Windows PE executable and identify potential ARM firmware.
Usage: python3 extract_resources.py <exe_file>
"""

import sys
import struct

try:
    import pefile
except ImportError:
    print("Installing pefile...")
    import subprocess
    subprocess.check_call([sys.executable, "-m", "pip", "install", "pefile"])
    import pefile

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 extract_resources.py <exe_file>")
        sys.exit(1)
    
    exe_file = sys.argv[1]
    
    print(f"Opening: {exe_file}")
    pe = pefile.PE(exe_file)
    
    # Get section headers for RVA to file offset conversion
    def rva_to_offset(rva):
        for section in pe.sections:
            section_start = section.VirtualAddress
            section_end = section_start + section.Misc_VirtualSize
            if section_start <= rva < section_end:
                return section.PointerToRawData + (rva - section_start)
        return None
    
    # Try to get resource data using the correct pefile API
    def get_resource_data(pe, entry):
        """Try multiple methods to get resource data."""
        # Method 1: Direct access to data entry attributes
        if hasattr(entry, 'data'):
            data = entry.data
            if data is not None:
                for attr in ['data_rva', 'rva', 'OffsetToData']:
                    if hasattr(data, attr):
                        rva = getattr(data, attr)
                        for size_attr in ['data_size', 'size', 'Size']:
                            if hasattr(data, size_attr):
                                size = getattr(data, size_attr)
                                try:
                                    return pe.get_data(rva, size)
                                except:
                                    pass
        
        # Method 2: Try to access via the entry itself
        for attr in ['OffsetToData', 'rva']:
            if hasattr(entry, attr):
                rva = getattr(entry, attr)
                for size_attr in ['Size', 'size']:
                    if hasattr(entry, size_attr):
                        size = getattr(entry, size_attr)
                        try:
                            return pe.get_data(rva, size)
                        except:
                            pass
        
        # Method 3: Use the directory entry structure
        if hasattr(entry, 'directory'):
            for sub_entry in entry.directory.entries:
                result = get_resource_data(pe, sub_entry)
                if result:
                    return result
        
        return None
    
    resource_types = {
        0: "CURSOR",
        1: "BITMAP",
        2: "ICON",
        3: "MENU",
        4: "DIALOG",
        5: "STRING",
        6: "FONTDIR",
        7: "FONT",
        8: "ACCELERATOR",
        9: "RCDATA",
        10: "MESSAGETABLE",
        11: "GROUP_CURSOR",
        12: "GROUP_ICON",
        14: "MANIFEST",
        16: "VERSION",
        24: "DLGINCLUDE",
        25: "PLUGPLAY",
        26: "VXD",
        27: "ANICURSOR",
        28: "ANIICON",
        29: "HTML",
        30: "MANIFEST_PROXY"
    }
    
    extracted_count = 0
    
    for resource_type_entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        type_id = resource_type_entry.id
        type_name = resource_types.get(type_id, f"TYPE_{type_id}")
        
        print(f"\n=== Resource Type: {type_name} (ID: {type_id}) ===")
        
        for resource_id_entry in resource_type_entry.directory.entries:
            id_val = resource_id_entry.id
            
            for resource_lang_entry in resource_id_entry.directory.entries:
                lang_id = resource_lang_entry.id
                
                # Try to get data using different approaches
                data = None
                
                # Approach 1: Try direct data access
                if hasattr(resource_lang_entry, 'data') and resource_lang_entry.data:
                    data_entry = resource_lang_entry.data
                    for rva_attr in ['data_rva', 'rva', 'OffsetToData']:
                        if hasattr(data_entry, rva_attr):
                            rva = getattr(data_entry, rva_attr)
                            for size_attr in ['data_size', 'size', 'Size']:
                                if hasattr(data_entry, size_attr):
                                    size = getattr(data_entry, size_attr)
                                    try:
                                        data = pe.get_data(rva, size)
                                        break
                                    except:
                                        pass
                        if data:
                            break
                
                # Approach 2: Try using get_data with entry attributes
                if not data:
                    for attr_name in ['OffsetToData', 'rva']:
                        if hasattr(resource_lang_entry, attr_name):
                            rva = getattr(resource_lang_entry, attr_name)
                            for size_attr in ['Size', 'size']:
                                if hasattr(resource_lang_entry, size_attr):
                                    size = getattr(resource_lang_entry, size_attr)
                                    try:
                                        data = pe.get_data(rva, size)
                                        break
                                    except:
                                        pass
                        if data:
                            break
                
                # Approach 3: Use the resource directory structure
                if not data:
                    try:
                        # Access through the data directory
                        dir_entry = resource_lang_entry.directory
                        if dir_entry and hasattr(dir_entry, 'entries'):
                            for sub in dir_entry.entries:
                                if hasattr(sub, 'data') and sub.data:
                                    if hasattr(sub.data, 'OffsetToData'):
                                        rva = sub.data.OffsetToData
                                        if hasattr(sub.data, 'Size'):
                                            size = sub.data.Size
                                            try:
                                                data = pe.get_data(rva, size)
                                            except:
                                                pass
                    except:
                        pass
                
                if not data:
                    print(f"  ID: {id_val}, Lang: {lang_id} - Could not extract data")
                    continue
                
                print(f"  ID: {id_val}, Lang: {lang_id}, Size: {len(data)} bytes")
                
                # Save to file
                filename = f"resource_type{type_id}_id{id_val}_lang{lang_id}.bin"
                with open(filename, "wb") as f:
                    f.write(data)
                print(f"    Saved: {filename}")
                extracted_count += 1
                
                # Check if it looks like ARM Cortex-M firmware
                if len(data) >= 8:
                    sp = struct.unpack('<I', data[0:4])[0]
                    reset = struct.unpack('<I', data[4:8])[0]
                    
                    # NUC123 SRAM starts at 0x20000000
                    # Reset handler should be in flash (0x00000000 or 0x00100000)
                    sp_is_valid = (sp & 0xF0000000) == 0x20000000
                    reset_is_valid = (reset & 0xFFFFFF01) == 0x00000001
                    
                    if sp_is_valid and reset_is_valid:
                        print(f"    *** POTENTIAL ARM FIRMWARE DETECTED ***")
                        print(f"    Initial SP: {hex(sp)}")
                        print(f"    Reset Handler: {hex(reset)}")
                        print(f"    Size: {len(data)} bytes ({len(data)/1024:.1f} KB)")
                        
                        # Calculate approximate flash size
                        if reset < 0x00100000:
                            approx_size = 0x10000 - reset
                        else:
                            approx_size = 0x100000 - reset
                        print(f"    Approximate flash size: {approx_size} bytes ({approx_size/1024:.1f} KB)")
    
    print(f"\n=== Summary ===")
    print(f"Extracted {extracted_count} resource files.")
    print("Check the .bin files for ARM firmware (look for 'POTENTIAL ARM FIRMWARE' messages above)")

if __name__ == "__main__":
    main()