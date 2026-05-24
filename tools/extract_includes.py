import json
import os

with open('build/compile_commands.json', 'r') as f:
    data = json.load(f)

try:
    for entry in data:
        file_path = entry.get('file', '').replace('\\', '/')
        if 'sketch.ino' in file_path or 'CommandHandler' in file_path:
            cmds = entry['command']
            includes = [s[2:] for s in cmds.split() if s.startswith('-I')]
            print(f"File: {os.path.basename(file_path)}")
            for inc in sorted(includes):
                print(inc)
            print("---")
            break
except Exception as e:
    print(f"Error: {e}")