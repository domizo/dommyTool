#!/usr/bin/env python3
"""Actual bottle builder with graph ownership and late native failures."""

import sys
import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_small_generators import BuilderGraph
from test_python_rig_safety import functions
from DPyTools.core import geometry
from DPyTools.core.numeric import finite_number


class BottleGraph(BuilderGraph):
    def __init__(self):
        super().__init__()
        self.target = self.create('transform', name='bottle_geo')
        self.selected = [self.target]
        self.created[:] = []
        self.pm.createNode.reset_mock()
        self.bounds = (-1, 0, -1, 1, 4, 1)
        original = self.pm.xform.side_effect
        self.pm.xform.side_effect = lambda node, **kw: self.bounds if kw.get('bb') else original(node, **kw)
        self.measurement = NS(**functions('python/DPyTools/features/measurement.py', {'pm': self.pm, 'finite_number': finite_number}))
        self.pm.parentConstraint = Mock(side_effect=self.constraint)
        self.constraint_snapshots = []
        self.operation = functions('python/DPyTools/features/rigging/bottle.py', {
            'pm': self.pm, 'sys': sys, 'geometry': geometry, 'measurement': self.measurement,
            'groups': self.groups, 'ownership': self.ownership, 'parameters': self.parameters,
            'selection': self.selection, 'transforms': self.transforms})['createBottleSpinRig']
    def create(self, kind, **kwargs):
        node = super().create(kind, **kwargs)
        if kind == 'transform':
            node.add_port('displayHandle', 'bool', False)
            node.add_port('visibility', 'bool', True)
            node.add_port('matrix', 'matrix')
            node.add_port('rotatePivot', 'double3', (0, 0, 0))
        if kind == 'decomposeMatrix':
            node.add_port('inputMatrix', 'matrix')
            node.add_port('outputTranslate', 'double3', (0, 0, 0))
        if kind == 'multiplyDivide':
            for name in ('input1Y', 'input2Y', 'outputY'): node.add_port(name)
        return node
    def constraint(self, source, target, **kwargs):
        self.constraint_snapshots.append((len(self.connections), list(self.created)))
        target.attr('translate').value = (3, 4, 5)
        node = self.create('parentConstraint', **kwargs)
        self.selected = [node]
        return [node]


class BottleRigTests(unittest.TestCase):
    def setUp(self): self.graph = BottleGraph()
    def test_success_returns_known_root_and_constraint_is_last(self):
        root = self.graph.operation()
        self.assertEqual(root.name(), ':root_aux')
        self.assertEqual(self.graph.constraint_snapshots[0][0], 4)
        self.assertEqual(self.graph.created[-1].kind, 'parentConstraint')
        self.assertIsNone(self.graph.target.parent)
        self.assertEqual(self.graph.selected, [self.graph.target])
        self.assertTrue(all(node.exists for node in self.graph.created))
        self.assertTrue(all(not kw.get('force', False) for source, target, kw in self.graph.connections))
        self.assertEqual([node.kind for node in self.graph.created].count('locator'), 1)
    def test_noncenter_mode_keeps_historical_zero_positions(self):
        root = self.graph.operation(False)
        self.assertTrue(root)
        self.assertFalse(any(node.name() in (':trans_c_aux', ':rot_c_aux') for node in self.graph.created))
        tilt = next(node for node in self.graph.created if node.name() == ':tilt_aux')
        self.assertEqual(tilt.position, (1.0, 0.0, 0.0))
    def test_every_late_name_collision_precedes_allocation(self):
        for name in ('spin_aux', 'tilt_pivotShape', 'bottle_rig_multiplyDivide', 'bottle_rig_parentConstraint'):
            self.setUp()
            borrowed = self.graph.create('transform', name=name)
            self.graph.created[:] = []
            self.graph.pm.createNode.reset_mock()
            with self.subTest(name=name): self.assertFalse(self.graph.operation())
            self.graph.pm.createNode.assert_not_called()
            self.graph.pm.parentConstraint.assert_not_called()
            self.assertTrue(borrowed.exists)
    def test_empty_multiple_wrong_type_and_nonboolean_selection_are_rejected(self):
        for mode in ('empty', 'multiple', 'joint', 'shape', 'flag'):
            self.setUp()
            if mode == 'empty': self.graph.selected = []
            elif mode == 'multiple': self.graph.selected = [self.graph.target, self.graph.target]
            elif mode == 'joint': self.graph.target.kind = 'joint'
            elif mode == 'shape': self.graph.target.kind = 'mesh'
            with self.subTest(mode=mode): self.assertFalse(self.graph.operation(1 if mode == 'flag' else True))
            self.graph.pm.createNode.assert_not_called()
    def test_locked_referenced_instanced_or_connected_target_channels_precede_creation(self):
        for mode in ('node', 'reference', 'instance', 'channel', 'input'):
            self.setUp()
            if mode == 'node': self.graph.target.locked = True
            elif mode == 'reference': self.graph.target.referenced = True
            elif mode == 'instance': self.graph.target.instanced = True
            elif mode == 'channel': self.graph.target.attr('rotateZ').locked = True
            else: self.graph.target.attr('translate').incoming = [self.graph.target.attr('other')]
            with self.subTest(mode=mode): self.assertFalse(self.graph.operation())
            self.graph.pm.createNode.assert_not_called()
    def test_nonfinite_or_inverted_bounds_precede_creation(self):
        for bounds in ((0, 0, 0, float('nan'), 1, 1), (2, 0, 0, 1, 1, 1)):
            self.graph.bounds = bounds
            with self.subTest(bounds=bounds): self.assertFalse(self.graph.operation())
        self.graph.pm.createNode.assert_not_called()
    def test_group_failure_cleans_confirmed_prefix_without_touching_target(self):
        original = self.graph.pm.xform.side_effect
        def dispatch(node, **kw):
            if node.name() == ':spin_aux' and not kw.get('q'): raise RuntimeError('move failed')
            return original(node, **kw)
        self.graph.pm.xform.side_effect = dispatch
        self.assertFalse(self.graph.operation())
        self.assertTrue(all(not node.exists for node in self.graph.created))
        self.graph.pm.parentConstraint.assert_not_called()
        self.assertTrue(self.graph.target.exists)
    def test_helper_connection_failure_cleans_full_known_graph_before_constraint(self):
        self.graph.pm.connectAttr.side_effect = RuntimeError('helper failed')
        self.assertFalse(self.graph.operation())
        self.assertTrue(all(not node.exists for node in self.graph.created))
        self.graph.pm.parentConstraint.assert_not_called()
        self.assertEqual(self.graph.target.attr('translate').value, (0, 0, 0))
    def test_constraint_failure_cleans_known_helpers_but_does_not_claim_target_rollback(self):
        def fail(*args, **kwargs):
            self.graph.target.attr('translate').value = (7, 8, 9)
            raise RuntimeError('constraint changed then failed')
        self.graph.pm.parentConstraint.side_effect = fail
        self.assertFalse(self.graph.operation())
        self.assertTrue(all(not node.exists for node in self.graph.created))
        self.assertEqual(self.graph.target.attr('translate').value, (7, 8, 9))
        self.assertIn('native constraint edits may remain', self.graph.pm.warning.call_args.args[0])
    def test_borrowed_constraint_return_is_not_deleted(self):
        borrowed = self.graph.create('parentConstraint', name='old_constraint')
        self.graph.created[:] = []
        self.graph.pm.parentConstraint.side_effect = None
        self.graph.pm.parentConstraint.return_value = [borrowed]
        self.assertFalse(self.graph.operation())
        self.assertTrue(borrowed.exists)
        self.assertNotIn(borrowed, self.graph.deleted)
    def test_unknown_native_descendant_is_not_deleted_with_new_root(self):
        unknown = []
        def fail(source, target, **kwargs):
            root = next(node for node in self.graph.created if node.name() == ':root_aux')
            unknown.append(self.graph.create('mesh', parent=root))
            raise RuntimeError('unknown side effect')
        self.graph.pm.parentConstraint.side_effect = fail
        self.assertFalse(self.graph.operation())
        self.assertTrue(unknown[0].exists and unknown[0].parent.exists)
        self.assertGreaterEqual(self.graph.pm.warning.call_count, 2)
    def test_interruption_cleans_known_graph_and_restores_selection(self):
        self.graph.pm.connectAttr.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt): self.graph.operation()
        self.assertTrue(all(not node.exists for node in self.graph.created))
        self.assertEqual(self.graph.selected, [self.graph.target])


if __name__ == '__main__': unittest.main()
