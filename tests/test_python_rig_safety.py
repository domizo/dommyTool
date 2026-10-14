#!/usr/bin/env python3
"""Finite frame arithmetic and actual joint cleanup with controlled stubs."""

import ast
import math
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock

from test_python_table_selection import _tree

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'python'))
from DPyTools.core import geometry
from DPyTools.features.curves import geometry as curve_geometry


def functions(relative, namespace):
    tree = _tree(relative)
    body = [node for node in tree.body if isinstance(node, ast.FunctionDef)]
    module = ast.fix_missing_locations(ast.Module(body=body, type_ignores=[]))
    exec(compile(module, str(ROOT / relative), 'exec'), namespace)
    return namespace


class DirectionTests(unittest.TestCase):
    def test_curve_position_is_the_same_shared_function(self):
        self.assertIs(curve_geometry.position3, geometry.position3)

    def test_scale_safe_normalization_handles_large_and_tiny_vectors(self):
        for scale in (1e308, 1e-308, 5e-324, 1.0):
            result = geometry.direction3((scale, scale, 0))
            self.assertAlmostEqual(result[0], math.sqrt(0.5))
            self.assertAlmostEqual(sum(value * value for value in result), 1.0)

    def test_invalid_or_zero_directions_fail(self):
        for value in ((0, 0, 0), (True, 0, 0), (float('nan'), 0, 1),
                      (float('inf'), 0, 1), (1, 2), '123', (10 ** 400, 0, 0)):
            with self.subTest(value=value), self.assertRaises((TypeError, ValueError)):
                geometry.direction3(value)

    def test_parallel_aim_up_frames_fail(self):
        for up in ((2, 0, 0), (-2, 0, 0), (1, 1e-14, 0)):
            with self.assertRaises(ValueError):
                geometry.independent_directions((1, 0, 0), up)

    def test_independent_aim_up_frames_are_normalized(self):
        self.assertEqual(geometry.independent_directions((2, 0, 0), (0, 3, 0)),
                         ((1, 0, 0), (0, 1, 0)))


class JointNode:
    def __init__(self, name, kind='joint'):
        self.name = name
        self.kind = kind
        self.instanced = False

    def nodeType(self):
        return self.kind

    def isInstanced(self):
        return self.instanced


class JointCleanupTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scope = functions('python/DPyTools/rigging/jointActions.py',
                              {'geometry': geometry, 'sys': sys})

    def setUp(self):
        self.root = JointNode('root')
        self.children = [JointNode('child'), JointNode('extra', 'transform')]
        self.parent_node = None
        self.parents = {child: self.root for child in self.children}
        self.warnings = []
        self.parent_calls = []
        self.fail_restore = set()

        def relatives(node, **kwargs):
            return self.children if kwargs.get('children') else (
                [self.parent_node] if self.parent_node else None)

        def parent(node, destination=None, **kwargs):
            self.parent_calls.append((node, destination, kwargs))
            if node in self.fail_restore and not kwargs.get('world'):
                raise RuntimeError('restore failed')
            self.parents[node] = None if kwargs.get('world') else destination
            return [node]

        self.pm = SimpleNamespace(PyNode=lambda node: node, listRelatives=relatives,
                                  parent=Mock(side_effect=parent),
                                  aimConstraint=Mock(return_value=['constraint']),
                                  orientConstraint=Mock(return_value=['constraint']),
                                  delete=Mock(), joint=Mock(), makeIdentity=Mock(),
                                  xform=Mock(), warning=self.warnings.append)
        self.scope['pm'] = self.pm

    def run_axis(self, **kwargs):
        return self.scope['correctJointAxis'](self.root, **kwargs)

    def assert_restored(self):
        self.assertTrue(all(self.parents[child] is self.root for child in self.children))

    def test_success_restores_every_child_and_never_invents_180_flip(self):
        self.run_axis()
        self.assert_restored()
        self.pm.xform.assert_not_called()
        self.pm.aimConstraint.assert_called_once_with(
            self.children[0], self.root, aim=(1, 0, 0), upVector=(0, 1, 0),
            worldUpVector=(0, 1, 0), worldUpType='vector', weight=1.0)
        self.pm.delete.assert_called_once_with(['constraint'])

    def test_each_native_operation_failure_restores_children(self):
        for command in ('aimConstraint', 'joint', 'makeIdentity'):
            with self.subTest(command=command):
                self.setUp()
                error = RuntimeError(command + ' failed')
                getattr(self.pm, command).side_effect = error
                with self.assertRaises(RuntimeError) as caught:
                    self.run_axis()
                self.assertIs(caught.exception, error)
                self.assert_restored()

    def test_detach_command_that_changes_state_then_fails_is_restored(self):
        original = self.pm.parent.side_effect

        def fail(node, destination=None, **kwargs):
            result = original(node, destination, **kwargs)
            if kwargs.get('world'):
                raise RuntimeError('detach changed state then failed')
            return result

        self.pm.parent.side_effect = fail
        with self.assertRaises(RuntimeError):
            self.run_axis()
        self.assert_restored()
        self.pm.aimConstraint.assert_not_called()

    def test_delete_failure_retries_known_constraint_and_restores_children(self):
        self.pm.delete.side_effect = [RuntimeError('delete failed'), None]
        with self.assertRaises(RuntimeError):
            self.run_axis()
        self.assertEqual(self.pm.delete.call_count, 2)
        self.assert_restored()

    def test_unconfirmed_constraint_does_not_freeze_joint_orientation(self):
        for result in (None, []):
            with self.subTest(result=result):
                self.setUp()
                self.pm.aimConstraint.return_value = result
                with self.assertRaises(RuntimeError):
                    self.run_axis()
                self.pm.joint.assert_not_called()
                self.pm.makeIdentity.assert_not_called()
                self.assert_restored()

    def test_cleanup_failure_attempts_remaining_children_and_preserves_primary(self):
        error = RuntimeError('primary failure')
        self.pm.aimConstraint.side_effect = error
        self.fail_restore.add(self.children[0])
        with self.assertRaises(RuntimeError) as caught:
            self.run_axis()
        self.assertIs(caught.exception, error)
        self.assertIs(self.parents[self.children[1]], self.root)
        self.assertTrue(self.warnings)

    def test_cleanup_failure_after_success_is_reported_not_success(self):
        self.fail_restore.add(self.children[0])
        with self.assertRaisesRegex(RuntimeError, 'restore failed'):
            self.run_axis()
        self.assertIs(self.parents[self.children[1]], self.root)

    def test_interrupt_and_memory_failure_are_not_masked(self):
        for error in (KeyboardInterrupt(), MemoryError()):
            with self.subTest(error=type(error)):
                self.setUp()
                self.pm.aimConstraint.side_effect = error
                with self.assertRaises(type(error)):
                    self.run_axis()
                self.assert_restored()

    def test_leaf_uses_parent_and_root_leaf_is_a_noop(self):
        self.children = None
        self.parent_node = JointNode('parent')
        self.run_axis()
        self.pm.orientConstraint.assert_called_once_with(
            self.parent_node, self.root, weight=1.0)
        self.pm.parent.assert_not_called()
        self.parent_node = None
        self.pm.orientConstraint.reset_mock()
        self.run_axis()
        self.pm.orientConstraint.assert_not_called()

    def test_invalid_axes_fail_before_node_resolution_or_mutation(self):
        self.pm.PyNode = Mock()
        for kwargs in ({'aimAxis': (0, 0, 0)}, {'upAxis': (1, 0, 0)},
                       {'upDir': (float('nan'), 0, 0)}, {'upDir': (0, 0, 0)}):
            with self.assertRaises((TypeError, ValueError)):
                self.run_axis(**kwargs)
        self.pm.PyNode.assert_not_called()
        self.pm.parent.assert_not_called()

    def test_nonjoint_fails_before_detaching_any_child(self):
        self.root.kind = 'transform'
        with self.assertRaises(ValueError):
            self.run_axis()
        self.pm.parent.assert_not_called()

    def test_instanced_root_or_child_is_rejected_before_detaching(self):
        for node in (self.root, self.children[0]):
            node.instanced = True
            with self.assertRaises(ValueError):
                self.run_axis()
            node.instanced = False
        self.pm.parent.assert_not_called()


if __name__ == '__main__':
    unittest.main()
