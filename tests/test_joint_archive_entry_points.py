#!/usr/bin/env python3
"""Actual legacy Joint prototype admission without Maya scene operations."""

import inspect
import unittest
from unittest.mock import Mock, call

from test_python_rig_safety import functions


class JointArchiveEntryPointTests(unittest.TestCase):
    def setUp(self):
        self.pm = Mock()
        self.scope = functions('python/DPyTools/joint/JointOp.py', {'pm': self.pm})
        self.create_symmetry = self.scope['createSymmetryJoint']

    def assert_warning_only(self):
        self.pm.warning.assert_called_once()
        message = self.pm.warning.call_args.args[0]
        self.assertIn('archived', message)
        self.assertIn('unimplemented', message)
        self.assertIn('no joints were created', message)
        self.assertEqual(self.pm.mock_calls, [call.warning(message)])

    def test_default_empty_and_nonempty_calls_report_unimplemented_false(self):
        for args in ((), (None,), ([],), (['left_joint'],)):
            with self.subTest(args=args):
                self.pm.reset_mock()
                self.assertIs(self.create_symmetry(*args), False)
                self.assert_warning_only()

    def test_supplied_joint_list_is_not_mutated(self):
        joints = ['left_joint', 'center_joint', 'right_joint']
        retained = list(joints)
        self.assertIs(self.create_symmetry(jnt_list=joints), False)
        self.assertEqual(joints, retained)
        self.assert_warning_only()

    def test_disabled_prototype_does_not_inspect_or_iterate_input(self):
        class Uninspectable:
            def __iter__(self):
                raise AssertionError('A disabled prototype must not inspect input')

            def __len__(self):
                raise AssertionError('A disabled prototype must not inspect input')

        self.assertIs(self.create_symmetry(Uninspectable()), False)
        self.assert_warning_only()

    def test_historical_name_parameter_and_default_remain_available(self):
        self.assertEqual(self.create_symmetry.__name__, 'createSymmetryJoint')
        parameters = inspect.signature(self.create_symmetry).parameters
        self.assertEqual(tuple(parameters), ('jnt_list',))
        self.assertIsNone(parameters['jnt_list'].default)

    def test_warning_failure_or_interruption_is_not_hidden(self):
        for error in (RuntimeError('warning failed'), KeyboardInterrupt()):
            with self.subTest(error=type(error).__name__):
                self.pm.reset_mock()
                self.pm.warning.side_effect = error
                with self.assertRaises(type(error)) as caught:
                    self.create_symmetry(['left_joint'])
                self.assertIs(caught.exception, error)
                self.assert_warning_only()


if __name__ == '__main__':
    unittest.main()
