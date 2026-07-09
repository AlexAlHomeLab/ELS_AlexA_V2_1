import os
import re

root = os.path.join(os.path.dirname(__file__), "..", "src")
types_name = "els_types.h"
pattern = re.compile(r"uint8_t|uint16_t|uint32_t|int32_t|int16_t")

for dirpath, _, files in os.walk(root):
    for fn in files:
        if not fn.endswith(".h") or fn == types_name:
            continue
        path = os.path.join(dirpath, fn)
        with open(path, "r", encoding="utf-8") as f:
            text = f.read()
        if not pattern.search(text):
            continue
        if "#include <stdint.h>" in text or "els_types.h" in text:
            continue
        rel = os.path.relpath(os.path.join(root, types_name), dirpath).replace("\\", "/")
        include_line = f'#include "{rel}"\n'
        m = re.match(r"(#ifndef\s+\w+\s*\n#define\s+\w+\s*\n)", text)
        if m:
            text = text[: m.end()] + "\n" + include_line + text[m.end() :]
        else:
            text = include_line + text
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("fixed", path)
