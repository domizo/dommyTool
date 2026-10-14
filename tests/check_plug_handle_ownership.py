#!/usr/bin/env python3
"""Guard explicit ownership for plug-acquired (not data-block) handles."""
import pathlib
import re
import sys

sys.dont_write_bytecode = True
from check_active_cpp_safety import strip_comments_and_literals

ROOT = pathlib.Path(__file__).resolve().parents[1]


def violations(source):
    code = strip_comments_and_literals(source)
    problems = []
    if re.search(r"\.asMDataHandle\s*\(", code):
        problems.append("asMDataHandle hides required destructHandle ownership")
    if re.search(r"\b\w*Owner\.read\([^;]+;\s*CHECK_MSTATUS\(", code):
        problems.append("failed scoped handle acquisition is only logged")
    names = set(re.findall(r"\bMDataHandle\s*&?\s*(\w+)", code))
    for declaration in re.findall(r"\bMDataHandle\s+([\w\s,]+);", code):
        names.update(name.strip() for name in declaration.split(","))
    for name in names:
        if re.search(r"\.getValue\s*\(\s*" + re.escape(name) + r"\s*\)", code):
            problems.append("raw MPlug acquisition into " + name)
    return problems


assert violations("MDataHandle h; plug.getValue(h);")
assert violations("MDataHandle first, second; plug.getValue(second);")
assert violations("status = dataOwner.read(plug); CHECK_MSTATUS(status);")
assert violations("return plug.asMDataHandle().asVector();")
assert not violations("MDataHandle h = data.inputValue(attr, &status);")
assert not violations("// plug.asMDataHandle();\nMObject h; plug.getValue(h);")

failures = []
for path in sorted((ROOT / "src").rglob("*")):
    if path.suffix not in (".h", ".hpp", ".cpp", ".inl"):
        continue
    for problem in violations(path.read_text()):
        failures.append("{}: {}".format(path.relative_to(ROOT), problem))
if failures:
    print("\n".join(failures), file=sys.stderr)
    sys.exit(1)
print("Plug-acquired handle ownership check: OK")
