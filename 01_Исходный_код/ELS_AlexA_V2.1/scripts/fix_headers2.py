import os
import re

root = os.path.join(os.path.dirname(__file__), "..", "src")

for dirpath, _, files in os.walk(root):
    for fn in files:
        if not fn.endswith(".h"):
            continue
        path = os.path.join(dirpath, fn)
        with open(path, "r", encoding="utf-8") as f:
            text = f.read()
        m = re.match(r"#ifndef\s+(\w+)", text)
        if not m:
            continue
        guard = m.group(1)
        if f"#define {guard}" not in text:
            continue
        endif_count = len(re.findall(r"^#endif\s*$", text, re.M))
        has_extern_c = 'extern "C"' in text
        needed = 2 if has_extern_c else 1
        if endif_count < needed:
            with open(path, "a", encoding="utf-8") as f:
                f.write("\n#endif\n")
            print("fixed", path, endif_count, "->", needed)
