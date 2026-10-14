#!/usr/bin/env python3
"""Actual temporary-parent/group helpers and portable transform arithmetic."""

import math
import sys
import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_attribute_batches import Graph
from test_python_rig_safety import functions
from DPyTools.core import geometry


class TransformGraph(Graph):
    def __init__(self):
        super().__init__()
        self.parent_calls = []
        self.pm.parent = Mock(side_effect=self.parent)
        self.pm.xform = Mock(side_effect=self.xform)
        self.transforms = NS(**functions('python/DPyTools/scene/transforms.py', {
            'pm': self.pm, 'sys': sys, 'geometry': geometry, 'pmu': NS(VectorN=tuple), 'parameters': self.parameters}))
        self.groups = NS(**functions('python/DPyTools/scene/groups.py', {
            'pm': self.pm, 'sys': sys, 'geometry': geometry, 'parameters': self.parameters,
            'transforms': self.transforms, 'ownership': self.ownership}))
    def create(self, kind, **kwargs):
        node = super().create(kind, **kwargs)
        node.position = (0, 0, 0)
        if kind in ('transform', 'joint'):
            for name in ('translate', 'rotate', 'scale'):
                compound = node.add_port(name, 'double3', (1, 1, 1) if name == 'scale' else (0, 0, 0))
                compound.children = [node.add_port(name + axis) for axis in 'XYZ']
                for child in compound.children: child.parents = [compound]
        return node
    def parent(self, node, destination=None, **kwargs):
        self.parent_calls.append((node, destination, kwargs))
        if node.parent is not None: node.parent.children.remove(node)
        node.parent = None if kwargs.get('world') else destination
        if node.parent is not None: node.parent.children.append(node)
        return [node]
    def xform(self, node, **kwargs):
        if kwargs.get('query') or kwargs.get('q'):
            return node.position
        node.position = kwargs['translation']


class TemporaryParentTests(unittest.TestCase):
    def setUp(self):
        self.graph = TransformGraph()
        self.original = self.graph.create('transform', name='original')
        self.source = self.graph.create('transform', name='source', parent=self.original)
        self.target = self.graph.create('transform', name='target')
        self.run_align = self.graph.transforms.setTransformToObjectZero
    def test_success_preserves_world_unparent_semantics(self):
        self.assertIs(self.run_align(self.source, self.target), self.source)
        self.assertIsNone(self.source.parent)
        self.assertEqual(self.source.attr('scale').value, (1.0, 1.0, 1.0))
    def test_keep_parent_success_remains_under_target(self):
        self.run_align(self.source, self.target, isUnParent=False)
        self.assertIs(self.source.parent, self.target)
    def test_late_locked_channel_prevents_parenting_and_every_write(self):
        self.source.attr('scale').children[-1].locked = True
        with self.assertRaises(ValueError): self.run_align(self.source, self.target)
        self.graph.pm.parent.assert_not_called()
        self.graph.pm.setAttr.assert_not_called()
    def test_native_write_failure_restores_original_parent_without_channel_rollback(self):
        self.graph.pm.setAttr.side_effect = [None, RuntimeError('locked rotate')]
        with self.assertRaisesRegex(RuntimeError, 'native edits may remain'):
            self.run_align(self.source, self.target)
        self.assertIs(self.source.parent, self.original)
        self.assertEqual(self.graph.pm.setAttr.call_count, 2)
    def test_parent_changes_state_then_raises_is_restored(self):
        def dispatch(node, target=None, **kwargs):
            result = self.graph.parent(node, target, **kwargs)
            if target is self.target: raise RuntimeError('changed then failed')
            return result
        self.graph.pm.parent.side_effect = dispatch
        with self.assertRaisesRegex(RuntimeError, 'changed then failed'): self.run_align(self.source, self.target)
        self.assertIs(self.source.parent, self.original)
    def test_world_unparent_failure_restores_original_parent(self):
        def dispatch(node, target=None, **kwargs):
            result = self.graph.parent(node, target, **kwargs)
            if kwargs.get('world'): raise RuntimeError('unparent failed')
            return result
        self.graph.pm.parent.side_effect = dispatch
        with self.assertRaises(RuntimeError): self.run_align(self.source, self.target)
        self.assertIs(self.source.parent, self.original)
    def test_root_source_is_restored_to_world_after_interruption(self):
        self.graph.parent(self.source, world=True)
        self.graph.pm.setAttr.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt): self.run_align(self.source, self.target)
        self.assertIsNone(self.source.parent)
    def test_cleanup_failure_warns_without_masking_primary_error(self):
        def dispatch(node, target=None, **kwargs):
            if target is self.original: raise RuntimeError('restore refused')
            return self.graph.parent(node, target, **kwargs)
        self.graph.pm.parent.side_effect = dispatch
        self.graph.pm.setAttr.side_effect = ValueError('primary')
        with self.assertRaisesRegex(RuntimeError, 'primary'): self.run_align(self.source, self.target)
        self.graph.pm.warning.assert_called_once()
    def test_cycles_instances_references_and_non_boolean_options_precede_parenting(self):
        child = self.graph.create('transform', name='child', parent=self.source)
        for target in (self.source, child):
            with self.assertRaises(ValueError): self.run_align(self.source, target)
        self.source.instanced = True
        with self.assertRaises(ValueError): self.run_align(self.source, self.target)
        self.source.instanced = False
        self.target.referenced = True
        with self.assertRaises(ValueError): self.run_align(self.source, self.target)
        with self.assertRaises(TypeError): self.run_align(self.source, self.target, isTrans=1)
        self.graph.pm.parent.assert_not_called()


class TransformMathTests(unittest.TestCase):
    def setUp(self):
        self.graph = TransformGraph()
        self.source = self.graph.create('transform', name='source')
        self.target = self.graph.create('transform', name='target')
    def test_opposite_foot_directions_receive_opposite_y_rotations(self):
        for aim, expected in (((1, 0, 0), 90), ((-1, 0, 0), -90)):
            self.graph.transforms.setRotateAimAxis(self.source, (0, 0, 1), aim)
            self.assertAlmostEqual(self.source.attr('rotate').value[1], expected)
    def test_planar_angle_is_scale_safe_and_handles_reversal(self):
        self.assertEqual(geometry.signed_y_angle_degrees((0, 0, 1e308), (1e-308, 0, 0)), 90)
        self.assertEqual(abs(geometry.signed_y_angle_degrees((0, 0, 1), (0, 0, -1))), 180)
        self.assertEqual(geometry.signed_y_angle_degrees((1, 0, 0), (1, 0, 0)), 0)
    def test_zero_nonfinite_boolean_and_nonplanar_aims_prevent_writes(self):
        for aim in ((0, 0, 0), (math.nan, 0, 1), (True, 0, 1), (0, 1, 0), (1e308, 1e-308, 0)):
            with self.subTest(aim=aim), self.assertRaises((TypeError, ValueError)):
                self.graph.transforms.setRotateAimAxis(self.source, (0, 0, 1), aim)
        self.graph.pm.setAttr.assert_not_called()
    def test_wide_opposite_finite_endpoints_have_finite_midpoint(self):
        self.source.position, self.target.position = (1e308, 0, 0), (-1e308, 0, 0)
        self.assertEqual(self.graph.transforms.getPositionBetweenObjects(self.source, self.target), (0, 0, 0))
    def test_distance_handles_large_tiny_and_ordinary_positions(self):
        for position, expected in (((1e200, 0, 0), 1e200), ((1e-200, 0, 0), 1e-200), ((3, 4, 0), 5)):
            self.target.position = position
            self.assertEqual(self.graph.transforms.getDistanceBetweenObjects(self.source, self.target), expected)
    def test_unrepresentable_distance_and_invalid_positions_fail(self):
        for first, second in (((1e308, 0, 0), (-1e308, 0, 0)), ((math.inf, 0, 0), (0, 0, 0))):
            with self.assertRaises(ValueError): geometry.distance3(first, second)
    def test_fraction_limits_and_instance_admission_are_explicit(self):
        for fraction in (-0.1, 1.1, math.nan, True):
            with self.assertRaises((TypeError, ValueError)):
                self.graph.transforms.getPositionBetweenObjects(self.source, self.target, fraction)
        self.source.instanced = True
        with self.assertRaises(ValueError): self.graph.transforms.getDistanceBetweenObjects(self.source, self.target)
    def test_readonly_position_queries_allow_locked_referenced_nodes(self):
        self.source.locked = self.source.referenced = True
        self.target.position = (3, 4, 0)
        self.assertEqual(self.graph.transforms.getDistanceBetweenObjects(self.source, self.target), 5)


class GroupFactoryTests(unittest.TestCase):
    def setUp(self):
        self.graph = TransformGraph()
    def test_world_position_and_explicit_parent_are_retained(self):
        parent = self.graph.create('transform', name='parent')
        result = self.graph.groups.createEmpty('new', [parent], (1, 2, 3))
        self.assertEqual(result.position, (1.0, 2.0, 3.0))
        self.assertIs(result.parent, parent)
        self.graph.pm.createNode.assert_called_with('transform', name=':new', skipSelect=True)
    def test_invalid_position_multiple_parents_and_collision_prevent_allocation(self):
        parent = self.graph.create('transform', name='occupied')
        self.graph.pm.createNode.reset_mock()
        for args in [('new', None, (math.inf, 0, 0)), ('new', [parent, parent], (0, 0, 0)), ('occupied', None, (0, 0, 0))]:
            with self.assertRaises(ValueError): self.graph.groups.createEmpty(*args)
        self.graph.pm.createNode.assert_not_called()
    def test_native_move_failure_cleans_owned_group_not_borrowed_parent(self):
        parent = self.graph.create('transform', name='parent')
        self.graph.pm.xform.side_effect = RuntimeError('move refused')
        with self.assertRaises(RuntimeError): self.graph.groups.createEmpty('new', parent)
        self.assertTrue(parent.exists)
        self.assertFalse(self.graph.created[-1].exists)
    def test_alignment_failure_cleans_new_group_and_preserves_target(self):
        target = self.graph.create('transform', name='target')
        self.graph.pm.setAttr.side_effect = RuntimeError('write refused')
        with self.assertRaises(RuntimeError): self.graph.groups.createEmptyWithAlign('new', target)
        self.assertTrue(target.exists)
        self.assertFalse(self.graph.created[-1].exists)
    def test_invalid_alignment_target_and_flags_do_not_create_group(self):
        target = self.graph.create('utility', name='target')
        self.graph.pm.createNode.reset_mock()
        with self.assertRaises(ValueError): self.graph.groups.createEmptyWithAlign('new', target)
        target.kind = 'transform'
        with self.assertRaises(TypeError): self.graph.groups.createEmptyWithAlign('new', target, isRot=1)
        self.graph.pm.createNode.assert_not_called()


if __name__ == '__main__':
    unittest.main()
