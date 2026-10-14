#!/usr/bin/env python3
"""Reject expressions that Maya's return-status macro may evaluate twice."""

from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
from check_active_cpp_safety import strip_comments_and_literals


def unsafe_status_macro_calls(source):
    code = strip_comments_and_literals(source)
    for match in re.finditer(r'\bCHECK_MSTATUS_AND_RETURN_IT\s*\(', code):
        depth, end = 1, match.end()
        while end < len(code) and depth:
            if code[end] == '(':
                depth += 1
            elif code[end] == ')':
                depth -= 1
            end += 1
        argument = code[match.end():end - 1].strip()
        # Saved status variables and qualified status constants are harmless.
        # Require every other expression to use the single-evaluation helper.
        if depth or not re.fullmatch(r'[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*', argument):
            yield match.start(), end


def main():
    assert not list(unsafe_status_macro_calls('CHECK_MSTATUS_AND_RETURN_IT(status);'))
    assert not list(unsafe_status_macro_calls('CHECK_MSTATUS_AND_RETURN_IT(MS::kFailure);'))
    assert not list(unsafe_status_macro_calls('// CHECK_MSTATUS_AND_RETURN_IT(run());'))
    assert not list(unsafe_status_macro_calls('"CHECK_MSTATUS_AND_RETURN_IT(run())";'))
    assert len(list(unsafe_status_macro_calls(
        'CHECK_MSTATUS_AND_RETURN_IT(make(1, nested(2)));'))) == 1
    assert len(list(unsafe_status_macro_calls(
        'CHECK_MSTATUS_AND_RETURN_IT((++calls, status));'))) == 1
    errors = []
    root = Path(__file__).resolve().parent.parent
    for path in sorted((root / 'src').rglob('*')):
        if path.suffix not in ('.cpp', '.h', '.hpp', '.inl'):
            continue
        source = path.read_text(encoding='utf-8')
        for start, _ in unsafe_status_macro_calls(source):
            errors.append('{}:{}: potentially repeated status expression'.format(
                path.relative_to(root), source.count('\n', 0, start) + 1))
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    print('Single-evaluation status macro check: OK')
    return 0


if __name__ == '__main__':
    sys.exit(main())
