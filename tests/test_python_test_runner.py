#!/usr/bin/env python3
"""Ensure the shared CI runner admits the complete discovered test inventory."""

import contextlib
import io
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch

import run_python_tests


def _case(identifier):
    class Case(unittest.TestCase):
        def runTest(self):
            pass
        def id(self):
            return identifier
    return Case()


class PythonTestRunnerTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='dommytool-test-inventory-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / 'test_one.py').write_text('# fixture only\n', encoding='utf-8')
        self.suite = unittest.TestSuite([_case('test_one.Case.test_one')])
        self.loader = SimpleNamespace(errors=[], discover=Mock(return_value=self.suite))
        self.patcher = patch.object(run_python_tests.unittest, 'TestLoader', return_value=self.loader)
        self.patcher.start()
        self.addCleanup(self.patcher.stop)

    def test_discovery_uses_naming_pattern_and_complete_inventory(self):
        (self.root / 'test_two.py').write_text('# fixture only\n', encoding='utf-8')
        self.suite.addTest(_case('test_two.Case.test_two'))
        suite, modules, tests = run_python_tests.discover_suite(self.root)
        self.assertIs(suite, self.suite)
        self.assertEqual((modules, tests), (2, 2))
        self.loader.discover.assert_called_once_with(str(self.root), pattern='test_*.py')

    def test_nested_modules_cannot_be_silently_ignored(self):
        nested = self.root / 'nested'
        nested.mkdir()
        (nested / 'test_two.py').write_text('# fixture only\n', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'nested.test_two'):
            run_python_tests.discover_suite(self.root)
        self.suite.addTest(unittest.TestSuite([_case('nested.test_two.Case.test_two')]))
        self.assertEqual(run_python_tests.discover_suite(self.root)[1:], (2, 2))

    def test_empty_or_zero_test_module_fails(self):
        self.loader.discover.return_value = unittest.TestSuite()
        with self.assertRaises(ValueError):
            run_python_tests.discover_suite(self.root)
        (self.root / 'test_two.py').write_text('# fixture only\n', encoding='utf-8')
        self.loader.discover.return_value = self.suite
        with self.assertRaisesRegex(ValueError, 'test_two'):
            run_python_tests.discover_suite(self.root)

    def test_import_errors_and_duplicate_test_ids_fail(self):
        self.loader.errors = ['import failed']
        with self.assertRaisesRegex(ValueError, 'import failed'):
            run_python_tests.discover_suite(self.root)
        self.loader.errors = []
        self.suite.addTest(_case('test_one.Case.test_one'))
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            run_python_tests.discover_suite(self.root)

    def test_list_mode_admits_inventory_without_executing_tests(self):
        output = io.StringIO()
        with patch.object(run_python_tests, 'discover_suite', return_value=(self.suite, 1, 1)), \
                patch.object(run_python_tests.unittest, 'TextTestRunner') as runner, \
                contextlib.redirect_stdout(output):
            self.assertEqual(run_python_tests.main(['--list']), 0)
        runner.assert_not_called()
        self.assertIn('1 modules, 1 tests', output.getvalue())

    def test_runner_failure_and_discovery_failure_are_nonzero(self):
        with patch.object(run_python_tests, 'discover_suite', return_value=(self.suite, 1, 1)), \
                patch.object(run_python_tests.unittest, 'TextTestRunner') as runner, \
                contextlib.redirect_stdout(io.StringIO()):
            runner.return_value.run.return_value.wasSuccessful.return_value = False
            self.assertEqual(run_python_tests.main([]), 1)
        with patch.object(run_python_tests, 'discover_suite', side_effect=ValueError('missing tests')), \
                contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(run_python_tests.main([]), 1)

    def test_successful_test_run_is_zero(self):
        with patch.object(run_python_tests, 'discover_suite', return_value=(self.suite, 1, 1)), \
                patch.object(run_python_tests.unittest, 'TextTestRunner') as runner, \
                contextlib.redirect_stdout(io.StringIO()):
            runner.return_value.run.return_value.wasSuccessful.return_value = True
            self.assertEqual(run_python_tests.main([]), 0)

    def test_publication_gate_uses_shared_runner_not_fixed_test_list(self):
        filename = Path(__file__).resolve().parent / 'check_publication_readiness.sh'
        source = filename.read_text(encoding='utf-8')
        self.assertEqual(source.count('python3 tests/run_python_tests.py'), 1)
        self.assertNotRegex(source, r'python3 tests/test_[A-Za-z0-9_]+\.py')


if __name__ == '__main__':
    unittest.main()
