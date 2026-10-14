#!/usr/bin/env python3
"""Plane/split arithmetic and actual joint preflight/owned failure handling."""

import numbers
import math
import sys
import unittest
import uuid
from types import SimpleNamespace
from unittest.mock import Mock

from test_scene_node_ownership import Scene, DagNode, definitions
from test_python_rig_safety import functions
from DPyTools.core import geometry, strings
from test_plane_direction import adjacent_plane


class JointPlanArithmeticTests(unittest.TestCase):
    def test_wide_position_interpolation_is_finite_and_keeps_endpoints(self):
        first, second = (-1e308, 0, 0), (1e308, 2, 4)
        self.assertEqual(geometry.interpolate_position(first, second, 0.5), (0, 1, 2))
        self.assertEqual(geometry.interpolate_position(first, second, 0), first)
        self.assertEqual(geometry.interpolate_position(first, second, 1), second)

    def test_invalid_interpolation_or_degenerate_plane_is_rejected(self):
        for fraction in (True, -1, 2, float('nan')):
            with self.assertRaises((TypeError, ValueError)):
                geometry.interpolate_position((0, 0, 0), (1, 1, 1), fraction)
        with self.assertRaises(ValueError):
            geometry.plane_direction((0, 0, 0), (1, 0, 0), (2, 0, 0))

    def test_large_plane_and_reverse_direction_are_stable(self):
        points = ((-1e308, 0, 0), (1e308, 0, 0), (0, 1e308, 0))
        normal = geometry.plane_direction(*points)
        self.assertEqual(normal, (0, 0, -1))
        self.assertEqual(geometry.plane_direction(*points, reverse=True), (0, 0, 1))


class JointPlanningTests(unittest.TestCase):
    def setUp(self):
        self.scene = Scene()
        self.source = DagNode('source_l_jnt', 'joint')
        self.target = DagNode('target_l_jnt', 'joint')
        self.source.position, self.target.position = [0, 0, 0], [9, 0, 0]
        self.scene.nodes.extend([self.source, self.target])
        def xform(node, **kwargs):
            if kwargs.get('q'):
                return node.position
            node.position = kwargs['t']
        def duplicate(node, **kwargs):
            copy = self.scene.create('joint', name=kwargs['n'])
            copy.position = list(node.position)
            return [copy]
        self.scene.pm.xform = Mock(side_effect=xform)
        self.scene.pm.duplicate = Mock(side_effect=duplicate)
        self.scene.pm.parent = Mock()
        owner_scope = definitions('python/DPyTools/scene/ownership.py',
                                  {'uuid': uuid, 'pm': self.scene.pm})
        self.scope = functions('python/DPyTools/rigging/jointActions.py',
                               {'pm': self.scene.pm, 'sys': sys, 'numbers': numbers,
                                'geometry': geometry, 'strings': strings,
                                'MAX_SPLIT_JOINTS': 10000,
                                'ownership': SimpleNamespace(SceneNodes=owner_scope['SceneNodes'])})
        self.axis = Mock()
        self.scope['correctJointAxis'] = self.axis

    def split(self, **kwargs):
        return self.scope['makeSplitJoint'](self.source, self.target, **kwargs)

    def test_split_end_joint_is_flat_and_positions_preserve_historical_fractions(self):
        result = self.split(numJoint=3, withEndJoint=True)
        self.assertEqual(len(result), 4)
        self.assertTrue(all(isinstance(node, DagNode) for node in result))
        self.assertEqual([node.position for node in result],
                         [(0, 0, 0), (3, 0, 0), (6, 0, 0), (9, 0, 0)])

    def test_split_preserves_constant_world_coordinates_at_every_fraction(self):
        self.source.position, self.target.position = [3, 0, 0.1], [3, 1, 0.1]
        result = self.split(numJoint=100)
        self.assertTrue(all(node.position[0] == 3.0 and node.position[2] == 0.1
                            for node in result))

    def test_bad_count_replacement_or_input_cannot_duplicate_valid_prefix(self):
        for kwargs in ({'numJoint': 0}, {'numJoint': True}, {'numJoint': 1.5},
                       {'numJoint': 10000, 'withEndJoint': True},
                       {'nameDic': {'': 'bad'}}, {'nameDic': {'jnt': '|bad'}}):
            with self.assertRaises((TypeError, ValueError)):
                self.split(**kwargs)
        self.scene.pm.xform.side_effect = RuntimeError('input query failed')
        with self.assertRaises(RuntimeError):
            self.split()
        self.scene.pm.duplicate.assert_not_called()

    def test_later_native_placement_failure_cleans_new_joints_only(self):
        original = self.scene.pm.xform.side_effect
        placements = [0]
        def fail(node, **kwargs):
            if not kwargs.get('q'):
                placements[0] += 1
                if placements[0] == 2:
                    raise RuntimeError('placement failed')
            return original(node, **kwargs)
        self.scene.pm.xform.side_effect = fail
        with self.assertRaises(RuntimeError):
            self.split()
        self.assertTrue(all(not node.exists for node in self.scene.created))
        self.assertTrue(self.source.exists and self.target.exists)

    def test_later_degenerate_finger_plane_is_rejected_before_any_axis_edit(self):
        third = DagNode('third', 'joint')
        pole = DagNode('pole', 'transform')
        self.source.position, self.target.position = [0, 1, 0], [1, 0, 0]
        third.position, pole.position = [2, 0, 0], [3, 0, 0]
        self.scene.nodes.extend([third, pole])
        with self.assertRaises(ValueError):
            self.scope['crossCorrectJointAxisForFingur']([self.source, self.target, third], pole)
        self.axis.assert_not_called()

    def test_valid_three_point_frames_are_admitted_before_both_edits(self):
        pole = DagNode('pole', 'transform')
        pole.position = [0, 1, 0]
        self.scene.nodes.append(pole)
        self.scope['crossCorrectJointAxis']([self.source, self.target, pole])
        self.assertEqual(self.axis.call_count, 2)

    def test_duplicate_or_instanced_batch_input_cannot_change_axes(self):
        with self.assertRaises(ValueError):
            self.scope['crossCorrectJointAxisForFingur']([self.source, self.source], self.target)
        self.target.instanced = True
        with self.assertRaises(ValueError):
            self.scope['crossCorrectJointAxisForFingur']([self.source, self.target], self.source)
        self.axis.assert_not_called()

    def test_parent_plan_cycle_is_refused_before_first_link(self):
        self.target.children.append(self.source)
        self.source.parent = self.target
        original = self.scene.pm.listRelatives
        self.scene.pm.listRelatives = lambda node, **kwargs: (
            [node.parent] if kwargs.get('parent') and node.parent else
            [] if kwargs.get('parent') else original(node, **kwargs))
        with self.assertRaises(ValueError):
            self.scope['parentJointFromlist']([self.source, self.target])
        self.scene.pm.parent.assert_not_called()

    def test_translated_adjacent_joint_planes_are_planned_before_both_axis_calls(self):
        third = DagNode('third', 'joint')
        self.scene.nodes.append(third)
        self.source.position, self.target.position, third.position = adjacent_plane()
        for reverse, normal in ((False, (0, 0, -1)), (True, (0, 0, 1))):
            self.axis.reset_mock()
            self.scope['crossCorrectJointAxis']([self.source, self.target, third], reverse=reverse)
            self.assertEqual(self.axis.call_count, 2)
            self.assertEqual([call.args[-1] for call in self.axis.call_args_list], [normal, normal])
            self.assertEqual([call.args[0] for call in self.axis.call_args_list], [self.source, self.target])

    def test_finger_adjacent_planes_keep_final_tip_to_first_reference(self):
        pole = DagNode('pole', 'transform')
        self.scene.nodes.append(pole)
        self.source.position, self.target.position, pole.position = adjacent_plane()
        for reverse, normals in ((False, [(0, 0, -1), (0, 0, 1)]),
                                 (True, [(0, 0, 1), (0, 0, -1)])):
            self.axis.reset_mock()
            self.scope['crossCorrectJointAxisForFingur']([self.source, self.target], pole, reverse=reverse)
            self.assertEqual([call.args[-1] for call in self.axis.call_args_list], normals)

    def test_translated_later_degenerate_finger_plane_cannot_apply_valid_prefix(self):
        third, pole = DagNode('third', 'joint'), DagNode('pole', 'transform')
        self.scene.nodes.extend([third, pole])
        origin, target, unused = adjacent_plane()
        delta = target[0] - origin[0]
        self.source.position = (origin[0], 1, origin[2])
        self.target.position = target
        third.position = (origin[0] + 2 * delta, 0, origin[2])
        pole.position = (origin[0] + 3 * delta, 0, origin[2])
        geometry.plane_direction(self.source.position, self.target.position, pole.position)
        with self.assertRaises(ValueError):
            self.scope['crossCorrectJointAxisForFingur']([self.source, self.target, third], pole)
        self.axis.assert_not_called()

    def test_wide_joint_triangle_retains_both_finite_unit_normals(self):
        third = DagNode('third', 'joint')
        self.scene.nodes.append(third)
        self.source.position, self.target.position, third.position = (
            (-1e308, 0, 0), (1e308, 0, 0), (0, 1e308, 0))
        self.scope['crossCorrectJointAxis']([self.source, self.target, third])
        self.assertEqual([call.args[-1] for call in self.axis.call_args_list], [(0, 0, -1), (0, 0, -1)])

    def test_bad_later_coordinates_or_query_failure_cannot_edit_axes(self):
        third = DagNode('third', 'joint')
        self.scene.nodes.append(third)
        for invalid in ((math.nan, 1, 0), (math.inf, 1, 0), (True, 1, 0)):
            third.position = invalid
            with self.assertRaises((TypeError, ValueError)):
                self.scope['crossCorrectJointAxis']([self.source, self.target, third])
            self.axis.assert_not_called()
        third.position = (0, 1, 0)
        for error in (RuntimeError('later query failed'), KeyboardInterrupt()):
            self.scene.pm.xform.side_effect = [self.source.position, self.target.position, error]
            with self.assertRaises(type(error)) as caught:
                self.scope['crossCorrectJointAxis']([self.source, self.target, third])
            self.assertIs(caught.exception, error)
            self.axis.assert_not_called()

    def test_mixed_scale_later_angular_degeneracy_still_refuses_all_axis_edits(self):
        third = DagNode('third', 'joint')
        self.scene.nodes.append(third)
        self.source.position, self.target.position, third.position = (
            (1e308, 0, 0), (-1e308, 0, 0), (1e308, 5e-324, 0))
        self.assertEqual(geometry.plane_direction(
            self.source.position, self.target.position, third.position), (0, 0, 1))
        with self.assertRaises(ValueError):
            self.scope['crossCorrectJointAxis']([self.source, self.target, third])
        self.axis.assert_not_called()


if __name__ == '__main__':
    unittest.main()
