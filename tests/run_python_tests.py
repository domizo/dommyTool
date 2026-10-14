#!/usr/bin/env python3
"""Discover every host unit-test module and refuse empty/missing inventories."""

import argparse
import sys
import unittest
from pathlib import Path


TEST_ROOT = Path(__file__).resolve().parent


def test_ids(suite):
    for item in suite:
        if isinstance(item, unittest.TestSuite):
            yield from test_ids(item)
        else:
            yield item.id()


def discover_suite(directory=TEST_ROOT):
    directory = Path(directory)
    expected = {'.'.join(path.relative_to(directory).with_suffix('').parts)
                for path in directory.rglob('test_*.py')}
    loader = unittest.TestLoader()
    suite = loader.discover(str(directory), pattern='test_*.py')
    if loader.errors:
        raise ValueError('Python test discovery failed:\n' + '\n'.join(loader.errors))
    identifiers = tuple(test_ids(suite))
    actual = {identifier.rsplit('.', 2)[0] for identifier in identifiers}
    missing = expected.difference(actual)
    if not expected or not identifiers:
        raise ValueError('Python test inventory must not be empty')
    if missing:
        raise ValueError('Python test modules were not collected: ' + ', '.join(sorted(missing)))
    if len(set(identifiers)) != len(identifiers):
        raise ValueError('Python test discovery produced duplicate test IDs')
    return suite, len(expected), len(identifiers)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--list', action='store_true', help='Verify and print inventory without running tests')
    arguments = parser.parse_args(argv)
    try:
        suite, module_count, test_count = discover_suite()
    except (ImportError, OSError, ValueError) as exc:
        print(str(exc), file=sys.stderr)
        return 1
    print('Python unit test inventory: {0} modules, {1} tests'.format(
        module_count, test_count), flush=True)
    if arguments.list:
        return 0
    result = unittest.TextTestRunner(verbosity=1).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    sys.exit(main())
