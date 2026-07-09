import os
import re

root = os.path.join(os.path.dirname(__file__), "..", "src")
skip = {"config.h", "config_modes.h", "hal_pins.h", "mode_grind.h", "config_defs.h"}
fixed = 0

for dirpath, _, files in os.walk(root):
    for fn in files:
        if not fn.endswith(".h"):
            continue
        if fn in skip or fn.endswith("_cfg.h"):
            continue
        path = os.path.join(dirpath, fn)
        with open(path, "r", encoding="utf-8") as f:
            text = f.read()
        m = re.match(r"#ifndef\s+(\w+)", text)
        if not m:
            continue
        guard = m.group(1)
        if re.search(r"^#define\s+" + guard + r"\s*$", text, re.M) and text.rstrip().endswith("#endif"):
            continue
        lines = text.splitlines()
        out = []
        i = 0
        if i < len(lines) and lines[i].startswith("#ifndef"):
            out.append(lines[i])
            i += 1
            if i >= len(lines) or not lines[i].strip().startswith("#define"):
                out.append("#define " + guard)
        while i < len(lines):
            out.append(lines[i])
            i += 1
        text2 = "\n".join(out)
        if 'extern "C"' in text2 and "#ifdef __cplusplus" not in text2:
            text2 = text2.replace('extern "C" {', '#ifdef __cplusplus\nextern "C" {\n#endif', 1)
            idx = text2.rfind("}")
            if idx != -1:
                text2 = text2[:idx] + "#ifdef __cplusplus\n}\n#endif" + text2[idx + 1 :]
        if not text2.rstrip().endswith("#endif"):
            text2 = text2.rstrip() + "\n\n#endif\n"
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text2)
        fixed += 1
        print("fixed", path)

print("total", fixed)
