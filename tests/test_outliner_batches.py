#!/usr/bin/env python3
"""Actual Outliner operations/UI preserve full-batch admission and status."""

import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_attribute_batches import Graph
from test_python_rig_safety import functions
from test_python_table_selection import _callback
from DPyTools.core import geometry


class OutlinerGraph(Graph):
    def __init__(self):
        super().__init__()
        self.selected = [self.create('transform', name='first'), self.create('mesh', name='second')]
        for node in self.selected:
            node.add_port('useOutlinerColor', 'bool', False)
            color = node.add_port('outlinerColor', 'float3', (0, 0, 0))
            color.children = [node.add_port('outlinerColor' + axis, 'float') for axis in 'RGB']
            for child in color.children:
                child.parents = [color]
                child.minimum, child.maximum = 0, 1
        self.pm.ls = lambda **kw: self.selected
        self.outliner = NS(**functions('python/DPyTools/scene/outliner.py', {'pm': self.pm, 'geometry': geometry, 'parameters': self.parameters}))


class OutlinerTests(unittest.TestCase):
    def setUp(self): self.graph = OutlinerGraph()
    def test_heterogeneous_targets_receive_complete_typed_color_and_boolean_values(self):
        self.assertEqual(self.graph.outliner.set_color_enabled(enabled=True), 2)
        self.assertEqual(self.graph.outliner.set_color(color=(.2, .4, .6)), 2)
        for node in self.graph.selected:
            self.assertTrue(node.attr('useOutlinerColor').value)
            self.assertEqual(node.attr('outlinerColor').value, (.2, .4, .6))
            self.graph.pm.setAttr.assert_any_call(node.attr('outlinerColor'), .2, .4, .6, type='float3')
    def test_later_lock_reference_instance_or_missing_attribute_prevents_all_writes(self):
        for state in ('lock', 'reference', 'instance', 'missing', 'nodeLock'):
            self.setUp()
            node = self.graph.selected[-1]
            if state == 'lock': node.attr('useOutlinerColor').locked = True
            if state == 'reference': node.referenced = True
            if state == 'instance': node.instanced = True
            if state == 'missing': del node.ports['useOutlinerColor']
            if state == 'nodeLock': node.locked = True
            with self.subTest(state=state), self.assertRaises(ValueError): self.graph.outliner.set_color_enabled()
            self.graph.pm.setAttr.assert_not_called()
    def test_later_color_child_lock_or_input_prevents_every_target_write(self):
        child = self.graph.selected[-1].attr('outlinerColor').children[-1]
        for state in ('lock', 'input'):
            child.locked = state == 'lock'
            child.incoming = [self.graph.selected[0].attr('gain')] if state == 'input' else []
            with self.assertRaises(ValueError): self.graph.outliner.set_color(color=(1, 0, 0))
        self.graph.pm.setAttr.assert_not_called()
    def test_bad_color_shape_nonfinite_boolean_and_range_precede_all_writes(self):
        for color in ((1, 2), (float('nan'), 0, 0), (True, 0, 0), (-.1, 0, 0), (1.1, 0, 0)):
            with self.subTest(color=color), self.assertRaises((TypeError, ValueError)): self.graph.outliner.set_color(color=color)
        self.graph.pm.setAttr.assert_not_called()
    def test_non_boolean_enablement_is_refused(self):
        for flag in (1, 0, 'yes', None):
            with self.assertRaises(TypeError): self.graph.outliner.set_color_enabled(enabled=flag)
        self.graph.pm.setAttr.assert_not_called()
    def test_empty_none_native_selection_is_a_noop(self):
        for selection in ([], None):
            self.graph.selected = selection
            self.assertEqual(self.graph.outliner.set_color_enabled(), 0)
            self.assertEqual(self.graph.outliner.set_color(), 0)
        self.graph.pm.setAttr.assert_not_called()
    def test_duplicate_or_oversized_target_plan_precedes_writes(self):
        node = self.graph.selected[0]
        for nodes in ([node, node], [node] * 10001):
            with self.assertRaises(ValueError): self.graph.outliner.set_color_enabled(nodes)
        self.graph.pm.setAttr.assert_not_called()
    def test_explicit_target_list_does_not_re_read_selection(self):
        first = self.graph.selected[0]
        self.graph.pm.ls = Mock(side_effect=AssertionError('unexpected selection query'))
        self.assertEqual(self.graph.outliner.set_color_enabled([first], False), 1)
        self.graph.pm.ls.assert_not_called()
    def test_native_failure_reports_partial_write_without_rollback(self):
        native = self.graph.pm.setAttr.side_effect
        count = [0]
        def dispatch(port, *values, **kwargs):
            count[0] += 1
            if count[0] == 2: raise RuntimeError('second failed')
            return native(port, *values, **kwargs)
        self.graph.pm.setAttr.side_effect = dispatch
        with self.assertRaisesRegex(RuntimeError, 'after 1 writes'): self.graph.outliner.set_color_enabled()
        self.assertTrue(self.graph.selected[0].attr('useOutlinerColor').value)
        self.assertFalse(self.graph.selected[1].attr('useOutlinerColor').value)
        self.graph.pm.delete.assert_not_called()


class OutlinerCallbackTests(unittest.TestCase):
    def setUp(self):
        self.pm = NS(warning=Mock(), colorSliderGrp=Mock(return_value=(.2, .4, .6)))
        self.outliner = NS(set_color_enabled=Mock(return_value=2), set_color=Mock(return_value=2))
        self.scope = {'pm': self.pm, 'outliner': self.outliner}
        self.enable = _callback('python/DPyTools/ui/DOutlinerUI.py', 'openWin', 'activeVisCB', self.scope)
        self.color = _callback('python/DPyTools/ui/DOutlinerUI.py', 'openWin', 'changeColorCB', self.scope)
        self.ui = NS(colSlider='slider')
    def test_callbacks_return_true_only_after_confirmed_changes(self):
        self.assertTrue(self.enable(self.ui, False))
        self.outliner.set_color_enabled.assert_called_once_with(enabled=False)
        self.assertTrue(self.color(self.ui))
        self.outliner.set_color.assert_called_once_with(color=(.2, .4, .6))
    def test_empty_selection_callbacks_return_false_without_success_claim(self):
        self.outliner.set_color_enabled.return_value = self.outliner.set_color.return_value = 0
        self.assertFalse(self.enable(self.ui))
        self.assertFalse(self.color(self.ui))
    def test_expected_operation_failure_warns_and_returns_false(self):
        self.outliner.set_color_enabled.side_effect = ValueError('locked later')
        self.outliner.set_color.side_effect = RuntimeError('partial write')
        self.assertFalse(self.enable(self.ui))
        self.assertFalse(self.color(self.ui))
        self.assertEqual(self.pm.warning.call_count, 2)
    def test_ui_query_failure_does_not_dispatch_color_edit(self):
        self.pm.colorSliderGrp.side_effect = RuntimeError('query failed')
        self.assertFalse(self.color(self.ui))
        self.outliner.set_color.assert_not_called()


if __name__ == '__main__': unittest.main()
