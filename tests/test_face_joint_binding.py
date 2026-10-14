#!/usr/bin/env python3
"""Actual Face Joint Bind/ReSelect bodies and shared admission without Maya."""

import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_attribute_batches import Graph
from test_python_rig_safety import functions
from test_scene_node_ownership import definitions
from DPyTools.core import components, strings


class FaceJointBindingTests(unittest.TestCase):
    def setUp(self):
        self.graph = Graph()
        self.mesh = self.graph.create('mesh', name='|root|mesh|shape')
        self.mesh.add_port('intermediateObject', 'bool', False)
        self.mesh.getAttr = lambda name: self.mesh.attr(name).value
        self.first = self.graph.create('joint', name='|root|char:jaw')
        self.second = self.graph.create('joint', name='|root|char:lip')
        self.extra = self.graph.create('transform', name='|root|extra')
        self.original = [self.first, self.extra, self.second]
        self.selected = list(self.original)
        self.joints = [self.first, self.second]
        self.events = []
        self.pm = self.graph.pm
        self.pm.PyNode = Mock(side_effect=self.resolve)
        self.pm.ls = Mock(side_effect=self.query_selection)
        self.pm.select = Mock(side_effect=self.select)
        self.pm.textField = Mock(return_value=self.mesh.name())
        self.pm.skinCluster = Mock(side_effect=self.bind)
        self.pm.undo = Mock()
        self.selection = NS(**functions('python/DPyTools/scene/selection.py', {
            'pm': self.pm, 'components': components}))
        self.scope = definitions('python/DPyTools/rigging/DFaceJointTool.py', {
            'pm': self.pm, 'strings': strings, 'parameters': self.graph.parameters,
            'selOp': self.selection, 'scene_selection': self.selection,
            'resolve_shape': self.selection.resolve_shape})
        self.ui = self.scope['DFaceJointTool']()
        self.ui.fitMeshNameText = 'meshField'

    def resolve(self, value):
        self.events.append(('resolve', value))
        return self.graph.resolve(value)

    def query_selection(self, **kwargs):
        return self.joints if kwargs.get('type') == 'joint' else self.selected

    def select(self, values=None, **kwargs):
        self.events.append(('select', values, kwargs))
        if kwargs.get('clear') or kwargs.get('cl'):
            self.selected = []
        else:
            rows = list(values) if isinstance(values, (list, tuple)) else [values]
            if kwargs.get('replace'):
                self.selected = rows
            else:
                self.selected.extend(rows)

    def bind(self, *args, **kwargs):
        self.events.append(('bind', args, kwargs))
        return 'skinCluster'

    def callbacks(self):
        return (self.ui.runBindCB, self.ui.runReSelectCB)

    def assert_no_mutation(self):
        self.assertEqual(self.selected, self.original)
        self.pm.select.assert_not_called()
        self.pm.skinCluster.assert_not_called()
        self.pm.delete.assert_not_called()
        self.pm.undo.assert_not_called()

    def test_bind_success_prepares_every_identity_before_one_replace_and_keeps_flags(self):
        self.assertIs(self.ui.runBindCB(), True)
        self.assertEqual(self.selected, [self.mesh, self.first, self.second])
        self.pm.select.assert_called_once_with([self.mesh, self.first, self.second], replace=True)
        self.pm.skinCluster.assert_called_once_with([self.first, self.second], self.mesh,
            skinMethod=1, toSelectedBones=True, maximumInfluences=3,
            normalizeWeights=1, dropoffRate=3.0)
        self.assertEqual([event[0] for event in self.events],
                         ['resolve', 'resolve', 'resolve', 'select', 'bind'])
        self.pm.warning.assert_not_called()

    def test_reselect_success_keeps_joint_order_and_never_binds_or_restores(self):
        self.joints.reverse()
        self.assertIs(self.ui.runReSelectCB(), True)
        self.pm.select.assert_called_once_with([self.mesh, self.second, self.first], replace=True)
        self.assertEqual(self.selected, [self.mesh, self.second, self.first])
        self.pm.skinCluster.assert_not_called()
        self.pm.warning.assert_not_called()

    def test_empty_none_or_nontext_mesh_stops_before_joint_query(self):
        for callback in self.callbacks():
            for value in ('', None, 3, True, [], {}):
                with self.subTest(callback=callback.__name__, value=value):
                    self.pm.textField.return_value = value
                    self.assertIs(callback(), False)
        self.pm.ls.assert_not_called()
        self.pm.PyNode.assert_not_called()
        self.assert_no_mutation()

    def test_missing_or_ambiguous_mesh_keeps_original_selection(self):
        self.graph.create('mesh', name=self.mesh.name())
        for callback in self.callbacks():
            for name in ('missingMesh', self.mesh.name()):
                with self.subTest(callback=callback.__name__, name=name):
                    self.pm.textField.return_value = name
                    self.assertIs(callback(), False)
        self.assert_no_mutation()

    def test_wrong_intermediate_or_instanced_geometry_cannot_mutate_selection(self):
        for callback in self.callbacks():
            for state in ('wrong_type', 'intermediate', 'instance'):
                with self.subTest(callback=callback.__name__, state=state):
                    self.pm.textField.return_value = self.first.name() if state == 'wrong_type' else self.mesh.name()
                    self.mesh.attr('intermediateObject').value = state == 'intermediate'
                    self.mesh.instanced = state == 'instance'
                    self.assertIs(callback(), False)
        self.assert_no_mutation()

    def test_transform_resolves_one_supported_shape_before_dispatch(self):
        target = self.graph.create('transform', name='meshTransform')
        target.getShapes = Mock(return_value=[self.mesh])
        self.pm.textField.return_value = target.name()
        self.assertIs(self.ui.runBindCB(), True)
        self.assertIs(self.pm.skinCluster.call_args.args[1], self.mesh)
        target.getShapes.assert_called_once_with(noIntermediate=True)

    def test_empty_or_multiple_shape_transform_is_rejected_before_selection(self):
        target = self.graph.create('transform', name='meshTransform')
        target.getShapes = Mock()
        self.pm.textField.return_value = target.name()
        for callback in self.callbacks():
            for shapes in ([], [self.mesh, self.mesh]):
                target.getShapes.return_value = shapes
                self.assertIs(callback(), False)
        self.assert_no_mutation()

    def test_none_or_empty_joint_query_is_a_reported_noop(self):
        for callback in self.callbacks():
            for value in (None, [], ()):
                self.joints = value
                self.assertIs(callback(), False)
        self.assert_no_mutation()

    def test_malformed_duplicate_or_over_limit_joint_batch_is_rejected(self):
        for callback in self.callbacks():
            for value in ('joint', {'joint': self.first}, iter([self.first]),
                          [self.first, self.first], [self.first] * 10001):
                with self.subTest(callback=callback.__name__, kind=type(value).__name__):
                    self.joints = value
                    self.assertIs(callback(), False)
        self.assert_no_mutation()

    def test_later_wrong_type_or_missing_joint_prevents_every_selection_write(self):
        for callback in self.callbacks():
            for value in (self.extra, 'missingJoint'):
                self.joints = [self.first, value]
                self.assertIs(callback(), False)
        self.assert_no_mutation()

    def test_joint_query_failure_is_reported_without_mutation(self):
        self.pm.ls.side_effect = RuntimeError('joint query failed')
        for callback in self.callbacks():
            self.assertIs(callback(), False)
        self.assert_no_mutation()

    def test_preflight_interruption_propagates_and_allows_later_retry(self):
        for callback in self.callbacks():
            error = KeyboardInterrupt('query interrupted')
            self.pm.PyNode.side_effect = error
            with self.assertRaises(KeyboardInterrupt) as caught:
                callback()
            self.assertIs(caught.exception, error)
            self.assert_no_mutation()
        self.pm.warning.assert_not_called()
        self.pm.PyNode.side_effect = self.resolve
        self.assertIs(self.ui.runReSelectCB(), True)

    def test_previous_selection_must_be_captured_successfully_before_dispatch(self):
        def query(**kwargs):
            if kwargs.get('type') == 'joint':
                return self.joints
            raise RuntimeError('selection query failed')
        self.pm.ls.side_effect = query
        for callback in self.callbacks():
            self.assertIs(callback(), False)
        self.assert_no_mutation()

    def test_malformed_previous_selection_prevents_dispatch(self):
        self.pm.ls.side_effect = lambda **kwargs: self.joints if kwargs.get('type') else 'invalid'
        for callback in self.callbacks():
            self.assertIs(callback(), False)
        self.assert_no_mutation()

    def test_none_previous_selection_restores_empty_after_binding_failure(self):
        self.selected = None
        self.pm.skinCluster.side_effect = RuntimeError('bind failed')
        self.assertIs(self.ui.runBindCB(), False)
        self.assertEqual(self.selected, [])
        self.pm.select.assert_called_with(clear=True)

    def test_selection_change_then_throw_restores_original_before_returning_false(self):
        for callback in self.callbacks():
            self.pm.select.reset_mock()
            def fail(values=None, **kwargs):
                self.select(values, **kwargs)
                if values and values[0] is self.mesh:
                    raise RuntimeError('selection changed then failed')
            self.pm.select.side_effect = fail
            self.assertIs(callback(), False)
            self.assertEqual(self.selected, self.original)
            self.assertEqual(self.pm.select.call_count, 2)
        self.pm.skinCluster.assert_not_called()

    def test_selection_failure_before_change_still_attempts_best_effort_restoration(self):
        def fail(values=None, **kwargs):
            if values and values[0] is self.mesh:
                raise ValueError('selection refused')
            self.select(values, **kwargs)
        self.pm.select.side_effect = fail
        self.assertIs(self.ui.runBindCB(), False)
        self.assertEqual(self.selected, self.original)
        self.pm.select.assert_called_with(self.original, replace=True)
        self.pm.skinCluster.assert_not_called()

    def test_binding_failure_restores_selection_without_deleting_native_partial_edits(self):
        def fail(*args, **kwargs):
            self.selected = [self.mesh]
            raise RuntimeError('skin/history was partly changed')
        self.pm.skinCluster.side_effect = fail
        self.assertIs(self.ui.runBindCB(), False)
        self.assertEqual(self.selected, self.original)
        self.assertIn('native skin/history edits may remain', self.pm.warning.call_args.args[0])
        self.pm.undo.assert_not_called()
        self.pm.delete.assert_not_called()

    def test_binding_interruption_restores_and_propagates_the_original_error(self):
        error = KeyboardInterrupt('bind interrupted')
        self.pm.skinCluster.side_effect = error
        with self.assertRaises(KeyboardInterrupt) as caught:
            self.ui.runBindCB()
        self.assertIs(caught.exception, error)
        self.assertEqual(self.selected, self.original)
        self.pm.warning.assert_not_called()
        self.pm.skinCluster.side_effect = self.bind
        self.assertIs(self.ui.runBindCB(), True)

    def test_reselection_interruption_after_change_restores_and_propagates(self):
        error = KeyboardInterrupt('selection interrupted')
        def fail(values=None, **kwargs):
            self.select(values, **kwargs)
            if values and values[0] is self.mesh:
                raise error
        self.pm.select.side_effect = fail
        with self.assertRaises(KeyboardInterrupt) as caught:
            self.ui.runReSelectCB()
        self.assertIs(caught.exception, error)
        self.assertEqual(self.selected, self.original)
        self.pm.warning.assert_not_called()

    def test_restoration_retains_identities_and_does_not_select_reused_names(self):
        replacement = None
        def fail(*args, **kwargs):
            nonlocal replacement
            self.extra.exists = False
            replacement = self.graph.create('transform', name=self.extra.name())
            raise RuntimeError('deleted old selection')
        self.pm.skinCluster.side_effect = fail
        self.assertIs(self.ui.runBindCB(), False)
        self.assertEqual(self.selected, [self.first, self.second])
        self.assertNotIn(replacement, self.selected)

    def test_expected_restore_failure_does_not_hide_binding_error_or_claim_success(self):
        self.pm.skinCluster.side_effect = RuntimeError('primary bind failure')
        def fail(values=None, **kwargs):
            if values == self.original:
                raise RuntimeError('restore refused')
            self.select(values, **kwargs)
        self.pm.select.side_effect = fail
        self.assertIs(self.ui.runBindCB(), False)
        warnings = [call.args[0] for call in self.pm.warning.call_args_list]
        self.assertTrue(any('primary bind failure' in warning for warning in warnings))
        self.assertTrue(any('restore refused' in warning for warning in warnings))
        self.pm.select.side_effect = self.select
        self.pm.skinCluster.side_effect = self.bind
        self.assertIs(self.ui.runBindCB(), True)

    def test_same_owner_reentry_during_native_selection_is_suppressed(self):
        results = []
        def select(values=None, **kwargs):
            self.select(values, **kwargs)
            results.extend(callback() for callback in self.callbacks())
        self.pm.select.side_effect = select
        self.assertIs(self.ui.runBindCB(), True)
        self.assertEqual(results, [False, False])
        self.assertEqual(self.pm.select.call_count, 1)
        self.assertEqual(self.pm.skinCluster.call_count, 1)

    def test_reentry_guard_covers_restoration_and_releases_after_restore_interruption(self):
        self.pm.skinCluster.side_effect = RuntimeError('bind failed')
        results = []
        error = KeyboardInterrupt('restore interrupted')
        def select(values=None, **kwargs):
            self.select(values, **kwargs)
            if values == self.original:
                results.extend(callback() for callback in self.callbacks())
                raise error
        self.pm.select.side_effect = select
        with self.assertRaises(KeyboardInterrupt) as caught:
            self.ui.runBindCB()
        self.assertIs(caught.exception, error)
        self.assertEqual(results, [False, False])
        self.pm.select.side_effect = self.select
        self.pm.skinCluster.side_effect = self.bind
        self.assertIs(self.ui.runReSelectCB(), True)


if __name__ == '__main__':
    unittest.main()
