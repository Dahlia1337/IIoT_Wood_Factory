Import("env") # type: ignore
import os
import shutil

def generate_and_copy_hex(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    elf_file = os.path.join(build_dir, "firmware.elf")
    hex_file = os.path.join(build_dir, "firmware.hex")
    
    root_dir = env.subst("$PROJECT_DIR")
    target_hex = os.path.join(root_dir, "IIoT_Proteus.hex")

    # 1. Generate .hex from .elf using GCC objcopy tool
    objcopy = env.subst("$OBJCOPY")
    if os.path.exists(elf_file):
        print("\n[INFO] Generating HEX file from ELF...")
        # Lệnh chuyển đổi định dạng từ elf sang intel-hex
        env.Execute(f'"{objcopy}" -O ihex "{elf_file}" "{hex_file}"')

    # 2. Copy the generated hex to the root directory
    if os.path.exists(hex_file):
        shutil.copy(hex_file, target_hex)
        print(f"[SUCCESS] Exported HEX for Proteus: {target_hex}\n")
    else:
        print("\n[ERROR] Failed to generate HEX file!\n")

env.AddPostAction("buildprog", generate_and_copy_hex) # type: ignore