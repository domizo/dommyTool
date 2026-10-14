#!/usr/bin/env python3
"""Actual Joint fit admission/dispatch and UI outcomes with existing doubles."""

import math
import unittest
from contextlib import redirect_stdout
from io import StringIO
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_transform_helpers import TransformGraph
from test_python_rig_safety import functions
from test_python_table_selection import _callback
from DPyTools.core import geometry, strings


class JointFittingTests(unittest.TestCase):
    def setUp(self):
        self.graph = TransformGraph()
        self.first = self.graph.create('joint', name='first')
        self.second = self.graph.create('joint', name='second')
        self.first.position, self.second.position = (101, 2, 3), (102, 4, 5)
        self.events = []
        def xform(node, **kwargs):
            if kwargs.get('q') or kwargs.get('query'):
                self.events.append(('query', node))
                return node.position
            self.events.append(('write', node))
            node.position = kwargs.get('t', kwargs.get('translation'))
        self.graph.pm.xform = Mock(side_effect=xform)
        def closest(**kwargs):
            self.events.append(('closest', kwargs))
            return [(201, 2, 3), (202, 4, 5)]
        self.closest = Mock(side_effect=closest)
        mesh_tools = NS(getClosestPointFromPoint=self.closest)
        self.scope = functions('python/DPyTools/joint/JointOp.py', {
            'pm': self.graph.pm, 'geometry': geometry, 'parameters': self.graph.parameters,
            'mesh_tools': mesh_tools})
        self.fit = self.scope['fitJointToMesh']

    def writes(self):
        return [node for kind, node in self.events if kind == 'write']

    def test_valid_batch_keeps_world_space_order_and_returns_completed_count(self):
        self.assertEqual(self.fit('mesh', [self.first, self.second]), 2)
        self.assertEqual([kind for kind, value in self.events],
                         ['query', 'query', 'closest', 'write', 'write'])
        self.closest.assert_called_once_with(m_mesh='mesh', pt_list=[(101.0, 2.0, 3.0), (102.0, 4.0, 5.0)])
        self.assertEqual(self.first.position, (201.0, 2.0, 3.0))
        self.assertEqual(self.second.position, (202.0, 4.0, 5.0))
        self.graph.pm.xform.assert_called_with(self.second, ws=True, t=(202.0, 4.0, 5.0))
        self.graph.pm.setAttr.assert_not_called()

    def test_none_and_empty_are_zero_without_queries_or_writes(self):
        for values in (None, [], ()):
            self.assertEqual(self.fit(joint_list=values), 0)
        self.graph.pm.xform.assert_not_called()
        self.closest.assert_not_called()

    def test_duplicate_wrong_type_unbounded_or_nonsequence_input_cannot_write(self):
        wrong = self.graph.create('transform', name='wrong')
        for values in ([self.first, self.first], [self.first, wrong], [wrong],
                       [self.first] * 10001, 'first', iter([self.first])):
            with self.subTest(values=type(values).__name__), self.assertRaises((RuntimeError, TypeError, ValueError)):
                self.fit('mesh', values)
        self.assertEqual(self.writes(), [])
        self.closest.assert_not_called()

    def test_later_locked_referenced_or_instanced_joint_prevents_every_write(self):
        for flag in ('locked', 'referenced', 'instanced'):
            setattr(self.second, flag, True)
            with self.subTest(flag=flag), self.assertRaises((RuntimeError, ValueError)):
                self.fit('mesh', [self.first, self.second])
            setattr(self.second, flag, False)
        self.assertEqual(self.writes(), [])
        self.closest.assert_not_called()

    def test_later_translate_or_child_lock_connection_and_writability_prevent_every_write(self):
        compound = self.second.attr('translate')
        for port in (compound, compound.children[-1]):
            for flag in ('locked', 'incoming'):
                setattr(port, flag, True if flag == 'locked' else [self.first.attr('gain')])
                with self.subTest(port=port.label, flag=flag), self.assertRaises((RuntimeError, ValueError)):
                    self.fit('mesh', [self.first, self.second])
                setattr(port, flag, False if flag == 'locked' else [])
        compound.writable = False
        with self.assertRaises((RuntimeError, ValueError)):
            self.fit('mesh', [self.first, self.second])
        self.assertEqual(self.writes(), [])

    def test_complete_source_positions_are_finite_before_mesh_query(self):
        self.second.position = (math.nan, 0, 0)
        with self.assertRaises((RuntimeError, ValueError)):
            self.fit('mesh', [self.first, self.second])
        self.closest.assert_not_called()
        self.assertEqual(self.writes(), [])

    def test_complete_closest_point_count_shape_and_coordinates_precede_writes(self):
        self.closest.side_effect = None
        for result in (None, [], [(1, 2, 3)], [(1, 2, 3), (math.inf, 0, 0)],
                       [(1, 2, 3), (1, 2)], [(1, 2, 3), (True, 0, 0)]):
            self.closest.return_value = result
            with self.subTest(result=result), self.assertRaises((RuntimeError, TypeError, ValueError)):
                self.fit('mesh', [self.first, self.second])
        self.assertEqual(self.writes(), [])

    def test_mesh_query_failure_reports_zero_without_writing(self):
        self.closest.side_effect = ValueError('invalid mesh')
        with self.assertRaisesRegex(RuntimeError, 'after 0 completed writes'):
            self.fit('mesh', [self.first, self.second])
        self.assertEqual(self.writes(), [])

    def test_later_native_failure_reports_completed_prefix_without_undo_or_selection_changes(self):
        original = self.graph.pm.xform.side_effect
        def fail(node, **kwargs):
            if not kwargs.get('q') and node is self.second:
                raise RuntimeError('native placement failed')
            return original(node, **kwargs)
        self.graph.pm.xform.side_effect = fail
        with self.assertRaisesRegex(RuntimeError, 'after 1 completed writes.*native edits may remain'):
            self.fit('mesh', [self.first, self.second])
        self.assertEqual(self.first.position, (201.0, 2.0, 3.0))
        self.assertEqual(self.second.position, (102, 4, 5))
        self.graph.pm.delete.assert_not_called()

    def test_changed_later_destination_is_rechecked_after_previous_native_write(self):
        original = self.graph.pm.xform.side_effect
        def change(node, **kwargs):
            result = original(node, **kwargs)
            if not kwargs.get('q') and node is self.first:
                self.second.attr('translate').children[0].locked = True
            return result
        self.graph.pm.xform.side_effect = change
        with self.assertRaisesRegex(RuntimeError, 'after 1 completed writes'):
            self.fit('mesh', [self.first, self.second])
        self.assertEqual(self.writes(), [self.first])

    def test_change_then_throw_is_not_counted_as_completed_or_rolled_back(self):
        original = self.graph.pm.xform.side_effect
        def fail(node, **kwargs):
            result = original(node, **kwargs)
            if not kwargs.get('q'):
                raise RuntimeError('changed then failed')
            return result
        self.graph.pm.xform.side_effect = fail
        with self.assertRaisesRegex(RuntimeError, 'after 0 completed writes.*native edits may remain'):
            self.fit('mesh', [self.first, self.second])
        self.assertEqual(self.first.position, (201.0, 2.0, 3.0))
        self.assertEqual(self.second.position, (102, 4, 5))
        self.graph.pm.delete.assert_not_called()

    def test_interruption_propagates_with_completed_edits_retained(self):
        original = self.graph.pm.xform.side_effect
        error = KeyboardInterrupt()
        def fail(node, **kwargs):
            if not kwargs.get('q') and node is self.second:
                raise error
            return original(node, **kwargs)
        self.graph.pm.xform.side_effect = fail
        with self.assertRaises(KeyboardInterrupt) as caught:
            self.fit('mesh', [self.first, self.second])
        self.assertIs(caught.exception, error)
        self.assertEqual(self.first.position, (201.0, 2.0, 3.0))


class JointFitCallbackTests(unittest.TestCase):
    def setUp(self):
        self.pm = NS(textField=Mock(return_value='|root|meshShape'), warning=Mock())
        self.selected = Mock(return_value=['joint'])
        self.fit = Mock(return_value=1)
        self.callback = _callback('python/DPyTools/rigging/DFaceJointTool.py',
            'DFaceJointTool', 'runFitJntToMeshCB',
            {'pm': self.pm, 'strings': strings,
             'selOp': NS(getJointlist=self.selected), 'jntOp': NS(fitJointToMesh=self.fit)})
        self.ui = NS(fitMeshNameText='field')

    def test_success_preserves_mesh_and_joint_arguments_and_returns_true(self):
        self.assertIs(self.callback(self.ui), True)
        self.fit.assert_called_once_with(target_mesh='|root|meshShape', joint_list=['joint'])
        self.pm.warning.assert_not_called()

    def test_empty_none_or_nontext_mesh_stops_before_joint_query(self):
        for mesh in ('', None, 5):
            self.pm.textField.return_value = mesh
            self.assertIs(self.callback(self.ui), False)
        self.selected.assert_not_called()
        self.fit.assert_not_called()

    def test_none_or_empty_selection_stops_without_dispatch(self):
        for selection in (None, [], ()):
            self.selected.return_value = selection
            self.assertIs(self.callback(self.ui), False)
        self.fit.assert_not_called()

    def test_false_none_or_zero_outcome_never_prints_success(self):
        for result in (False, None, 0):
            self.fit.return_value = result
            output = StringIO()
            with redirect_stdout(output):
                self.assertIs(self.callback(self.ui), False)
            self.assertEqual(output.getvalue(), '')

    def test_expected_failure_reports_completed_count_and_returns_false(self):
        self.fit.side_effect = RuntimeError('Joint fitting stopped after 1 completed writes; native edits may remain')
        self.assertIs(self.callback(self.ui), False)
        self.assertIn('after 1 completed writes', self.pm.warning.call_args.args[0])

    def test_interruption_is_not_converted_to_false_success(self):
        self.fit.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            self.callback(self.ui)
        self.pm.warning.assert_not_called()


if __name__ == '__main__':
    unittest.main()
