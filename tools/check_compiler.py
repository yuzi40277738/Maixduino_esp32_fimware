import json
import sys

with open('build/compile_commands.json', 'r') as f:
    data = json.load(f)

for entry in data:
    file_path = entry.get('file', '').replace('\\', '/')
    if 'sketch.ino' in file_path:
        cmds = entry['command']
        parts = cmds.split()
        for p in parts:
            if 'xtensa-esp32-elf-gcc.exe' in p:
                print("COMPILER:", p)
                break
        break
else:
    print("NOT FOUND")