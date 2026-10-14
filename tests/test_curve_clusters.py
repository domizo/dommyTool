#!/usr/bin/env python3
"""Actual editable-CV plans, native cluster results and known-node cleanup."""

import sys
import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_small_generators import BuilderGraph
from test_python_rig_safety import functions
from DPyTools.core import components, geometry


class CV:
    def __init__(self, owner, index):
        self.owner, self.index, self.position = owner, index, (index, 0, 0)


class ClusterGraph(BuilderGraph):
    def __init__(self):
        super().__init__()
        self.pm.cluster = Mock(side_effect=self.cluster)
        self.curve = self.create('nurbsCurve', name='inputCurve')
        self.curve.cv = [CV(self.curve, index) for index in range(3)]
        self.curve.numCVs = Mock(return_value=3)
        self.selected = [self.curve]
        self.selection = NS(**functions('python/DPyTools/scene/selection.py', {'pm': self.pm, 'components': components}))
        self.operation = functions('python/DPyTools/deformer/cluster.py', {
            'pm': self.pm, 'sys': sys, 'selection': self.selection, 'parameters': self.parameters,
            'components': components, 'geometry': geometry, 'ownership': self.ownership, 'MAX_CLUSTER_CVS': 1000})['createClusterFromCurve']
    def create(self, kind, **kwargs):
        node = super().create('mesh' if kind == 'clusterHandle' else kind, **kwargs)
        node.kind = kind
        node.add_port('intermediateObject', 'bool', False)
        node.getAttr = lambda name: node.attr(name).value
        node.getShapes = lambda **kw: [child for child in node.children if child.kind not in ('transform', 'joint') and not child.attr('intermediateObject').value]
        return node
    def cluster(self, cv, **kwargs):
        deformer = self.create('cluster')
        handle = self.create('transform')
        self.create('clusterHandle', parent=handle)
        self.selected = [handle]
        return [deformer, handle]


class ClusterTests(unittest.TestCase):
    def setUp(self): self.graph = ClusterGraph()
    def test_all_editable_cvs_are_planned_and_success_returns_handles(self):
        handles = self.graph.operation(self.graph.curve)
        self.assertEqual(len(handles), 3)
        self.assertTrue(all(node.kind == 'transform' for node in handles))
        self.graph.curve.numCVs.assert_called_once_with(editableOnly=True)
        self.assertEqual(self.graph.selected, [self.graph.curve])
        self.assertEqual([call.args[0] for call in self.graph.pm.cluster.call_args_list], self.graph.curve.cv)
        self.assertTrue(all(call.kwargs == {'relative': True, 'envelope': 1} for call in self.graph.pm.cluster.call_args_list))
    def test_periodic_editable_count_is_used_instead_of_degree_plus_spans(self):
        self.graph.curve.numCVs.return_value = 2
        self.graph.curve.mathematical_cv_count = 5
        self.assertEqual(len(self.graph.operation(self.graph.curve)), 2)
        self.assertEqual(self.graph.pm.cluster.call_count, 2)
    def test_invalid_zero_and_over_limit_counts_precede_native_creation(self):
        for count in (0, -1, True, 1.0, 1001):
            self.graph.curve.numCVs.return_value = count
            with self.subTest(count=count): self.assertFalse(self.graph.operation(self.graph.curve))
        self.graph.pm.cluster.assert_not_called()
    def test_late_nonfinite_cv_prevents_valid_prefix(self):
        self.graph.curve.cv[-1].position = (float('nan'), 0, 0)
        self.assertFalse(self.graph.operation(self.graph.curve))
        self.graph.pm.cluster.assert_not_called()
    def test_reference_instance_shape_and_input_lock_are_preflighted(self):
        for state in ('reference', 'instance', 'wrong', 'lock'):
            self.setUp()
            if state == 'reference': self.graph.curve.referenced = True
            if state == 'instance': self.graph.curve.instanced = True
            if state == 'wrong': self.graph.curve.kind = 'mesh'
            if state == 'lock': self.graph.curve.attr('create').locked = True
            with self.subTest(state=state): self.assertFalse(self.graph.operation(self.graph.curve))
            self.graph.pm.cluster.assert_not_called()
    def test_late_native_failure_cleans_confirmed_prefix_and_keeps_input(self):
        native = self.graph.pm.cluster.side_effect
        count = [0]
        def dispatch(cv, **kwargs):
            count[0] += 1
            if count[0] == 2: raise RuntimeError('second failed')
            return native(cv, **kwargs)
        self.graph.pm.cluster.side_effect = dispatch
        self.assertFalse(self.graph.operation(self.graph.curve))
        self.assertTrue(self.graph.curve.exists)
        self.assertTrue(all(not node.exists for node in self.graph.created if node is not self.graph.curve))
        self.assertIn('after 1 confirmed results', self.graph.pm.warning.call_args_list[0].args[0])
        self.assertEqual(self.graph.selected, [self.graph.curve])
    def test_return_order_is_classified_by_type(self):
        native = self.graph.pm.cluster.side_effect
        self.graph.pm.cluster.side_effect = lambda cv, **kw: list(reversed(native(cv, **kw)))
        self.assertEqual(len(self.graph.operation(self.graph.curve)), 3)
    def test_borrowed_or_malformed_native_result_is_never_deleted(self):
        borrowed_cluster = self.graph.create('cluster')
        borrowed_handle = self.graph.create('transform')
        self.graph.pm.cluster.side_effect = None
        for result in ([borrowed_cluster, borrowed_handle], None, [], [borrowed_handle] * 2):
            self.graph.pm.cluster.return_value = result
            with self.subTest(result=result): self.assertFalse(self.graph.operation(self.graph.curve))
        self.assertTrue(borrowed_cluster.exists and borrowed_handle.exists and self.graph.curve.exists)
        self.graph.pm.delete.assert_not_called()
    def test_unknown_foreign_handle_child_prevents_parent_deletion(self):
        unknown = []
        def dispatch(cv, **kwargs):
            deformer = self.graph.create('cluster')
            handle = self.graph.create('transform')
            unknown.append(self.graph.create('mesh', parent=handle))
            return [deformer, handle]
        self.graph.pm.cluster.side_effect = dispatch
        self.assertFalse(self.graph.operation(self.graph.curve))
        self.assertTrue(unknown[0].exists and unknown[0].parent.exists)
        self.assertTrue(self.graph.curve.exists)
        self.assertGreaterEqual(self.graph.pm.warning.call_count, 2)
    def test_unknown_native_sets_are_not_swept_into_cleanup(self):
        native = self.graph.pm.cluster.side_effect
        auxiliary = []
        def dispatch(cv, **kwargs):
            result = native(cv, **kwargs)
            auxiliary.append(self.graph.create('objectSet'))
            if len(auxiliary) == 2: raise RuntimeError('failed before native return')
            return result
        self.graph.pm.cluster.side_effect = dispatch
        self.assertFalse(self.graph.operation(self.graph.curve))
        self.assertTrue(all(node.exists for node in auxiliary))
        self.assertTrue(self.graph.curve.exists)
    def test_interrupt_attempts_cleanup_and_restores_selection(self):
        self.graph.pm.cluster.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt): self.graph.operation(self.graph.curve)
        self.assertEqual(self.graph.selected, [self.graph.curve])
    def test_cleanup_failure_does_not_delete_borrowed_curve_or_hide_status(self):
        native = self.graph.pm.cluster.side_effect
        count = [0]
        def dispatch(cv, **kwargs):
            count[0] += 1
            if count[0] == 2: raise RuntimeError('primary')
            return native(cv, **kwargs)
        self.graph.pm.cluster.side_effect = dispatch
        self.graph.pm.delete.side_effect = RuntimeError('cleanup refused')
        self.assertFalse(self.graph.operation(self.graph.curve))
        self.assertTrue(self.graph.curve.exists)
        self.assertEqual(self.graph.selected, [self.graph.curve])
        self.assertGreaterEqual(self.graph.pm.warning.call_count, 2)


if __name__ == '__main__': unittest.main()
