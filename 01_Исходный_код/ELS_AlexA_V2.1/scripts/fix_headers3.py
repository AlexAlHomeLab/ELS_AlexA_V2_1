import os

root = os.path.join(os.path.dirname(__file__), "..", "src")

for dirpath, _, files in os.walk(root):
    for fn in files:
        if not fn.endswith(".h"):
            continue
        path = os.path.join(dirpath, fn)
        with open(path, "r", encoding="utf-8") as f:
            lines = [ln.rstrip() for ln in f.readlines()]
        while lines and not lines[-1].strip():
            lines.pop()
        if not lines or not lines[0].startswith("#ifndef"):
            continue
        if lines[-1] != "#endif":
            with open(path, "a", encoding="utf-8") as f:
                f.write("\n#endif\n")
            print("append", path)
            continue
        # ifndef guard: file must end with guard #endif after optional extern C block
        if len(lines) >= 2 and lines[-2] == "}" and lines[-3] == "#ifdef __cplusplus":
            with open(path, "a", encoding="utf-8") as f:
                f.write("\n#endif\n")
            print("append guard", path)
