#!/usr/bin/env python3
"""Check real typed validation without payload copies or oversized allocation."""
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from check_curve_path_safety import run


def check(root, overrides=None, capture=False):
    if overrides:
        # These adjacent headers use basename includes. Shadow the include
        # chain so a mutation cannot include the original validation twice.
        headers = {path: (root/'src/core'/path).read_text() for path in (
            'common/utility/DAttributeValidation.h', 'common/utility/DPointMergeBudget.h',
            'common/utility/DAttributePartitionCopy.h', 'common/utility/DAttributeArrayConversion.h',
            'common/utility/DAttributeCreateNumeric.h', 'common/utility/DPointPayloadBudget.h')}
        headers.update(overrides)
        overrides = headers
    return run(root, '', suite='attribute_validation_allocations', arguments=[],
               header_overrides=overrides, capture=capture,
               extra_flags=('-I'+str(root/'src/core/common/utility'),))


def mutations(root):
    paths = ('common/utility/DAttributeValidation.h', 'common/utility/DPointMergeBudget.h',
             'common/utility/DAttributePartitionCopy.h')
    validation, merge, partitions = ((root/'src/core'/path).read_text() for path in paths)
    cases = (
        ('attribute row copied before admission', paths[0], validation,
         'const T& value = values[index];', 'const T value = values[index];'),
        ('attribute default copied before admission', paths[0], validation,
         'const T& defaultValue = attribute->defaultValue();',
         'const T defaultValue = attribute->getDefaultValue();'),
        ('expanded default copied before admission', paths[0], validation,
         'typed->defaultValue(), valueBudget', 'typed->getDefaultValue(), valueBudget'),
        ('merge default copied before admission', paths[1], merge,
         'typed->defaultValue(), cost', 'typed->getDefaultValue(), cost'),
        ('partition default copied before admission', paths[2], partitions,
         'const T& defaultValue = source->defaultValue();',
         'const T defaultValue = source->getDefaultValue();'),
    )
    check(root)
    for label, path, source, old, new in cases:
        assert old in source, label
        try:
            check(root, {path: source.replace(old, new)}, capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'attribute_validation_allocations_tests', \
                'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 2 and sys.argv[1] == '--mutations':
        mutations(root)
    else:
        check(root)
