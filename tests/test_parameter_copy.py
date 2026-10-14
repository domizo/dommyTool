#!/usr/bin/env python3
"""Complete typed copy admission and retained-identity UI callbacks."""

import numbers
import sys
import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_parameter_sharing import Graph, Node, Port
from test_python_rig_safety import functions
from test_python_table_selection import _callback
from DPyTools.core import strings
from DPyTools.scene import attribute_values


class ParameterCopyTests(unittest.TestCase):
    def setUp(self):
        self.graph = Graph()
        self.scope = functions('python/DPyTools/scene/parameters.py', {
            'pm': self.graph.pm, 'sys': sys, 'numbers': numbers,
            'strings': strings, 'attribute_values': attribute_values,
            'MAX_CONNECTIONS': 10000,
            'SUPPORTED_TYPES': attribute_values.SCALAR_TYPES + attribute_values.COMPOUND_TYPES + ('string',)})
        self.copy = self.scope['copy_parameters']

    def test_complete_vectors_unicode_and_heterogeneous_nodes_are_supported(self):
        self.graph.target.kind = 'differentUtility'
        self.graph.target.values['offset'] = (0, 0, 0)
        self.assertEqual(self.copy(self.graph.source, [self.graph.target]), 3)
        self.assertEqual(self.graph.target.values, self.graph.source.values)
        self.graph.pm.connectAttr.assert_not_called()

    def test_empty_duplicate_source_and_missing_later_target_cannot_write(self):
        for targets in ([], None, [self.graph.target, self.graph.target],
                        [self.graph.source], [self.graph.target, 'missing']):
            with self.subTest(targets=targets), self.assertRaises((TypeError, ValueError)):
                self.copy(self.graph.source, targets)
        self.graph.pm.setAttr.assert_not_called()

    def test_later_locked_reference_mismatch_or_bad_value_cannot_write_prefix(self):
        for state in ('lock', 'reference', 'type', 'value', 'connection'):
            later = Node('later')
            if state == 'lock':
                later.attr('gain').locked = True
            elif state == 'reference':
                later.ref = True
            elif state == 'type':
                later.types['gain'] = 'float'
            elif state == 'value':
                later.values['gain'] = float('nan')
            else:
                later.attr('gain').incoming = [Port(Node('animation'), 'out')]
            with self.subTest(state=state), self.assertRaises(ValueError):
                self.copy(self.graph.source, [self.graph.target, later], ['gain'])
        self.graph.pm.setAttr.assert_not_called()

    def test_source_animation_lock_and_reference_do_not_prevent_snapshot_reading(self):
        self.graph.source.ref = True
        self.graph.source.attr('gain').locked = True
        self.graph.source.attr('gain').incoming = [Port(Node('driver'), 'out')]
        self.assertEqual(self.copy(self.graph.source, [self.graph.target], ['gain']), 1)

    def test_unsupported_attributes_are_excluded_from_default_but_explicit_requests_fail(self):
        for node in (self.graph.source, self.graph.target):
            node.types['array'] = 'doubleArray'
            node.values['array'] = [1, 2]
            node.ports['array'] = Port(node, 'array')
        self.assertEqual(self.copy(self.graph.source, [self.graph.target]), 3)
        self.graph.pm.setAttr.reset_mock()
        with self.assertRaises(ValueError):
            self.copy(self.graph.source, [self.graph.target], ['array'])
        self.graph.pm.setAttr.assert_not_called()

    def test_compound_parent_lock_and_input_block_child_writes(self):
        original = self.graph.pm.attributeQuery
        def query(name, **kwargs):
            if kwargs.get('listParent'):
                return ['offset'] if name == 'offsetX' else []
            return original(name, **kwargs)
        self.graph.pm.attributeQuery = query
        for node in (self.graph.source, self.graph.target):
            node.types['offsetX'] = 'double'
            node.values['offsetX'] = 1.0
            node.ports['offsetX'] = Port(node, 'offsetX')
        self.graph.target.attr('offset').locked = True
        with self.assertRaises(ValueError):
            self.copy(self.graph.source, [self.graph.target], ['offsetX'])
        self.graph.target.attr('offset').locked = False
        self.graph.target.attr('offset').incoming = [Port(Node('driver'), 'out')]
        with self.assertRaises(ValueError):
            self.copy(self.graph.source, [self.graph.target], ['offsetX'])
        self.graph.pm.setAttr.assert_not_called()

    def test_all_source_values_are_captured_before_first_write(self):
        original = self.graph.pm.setAttr.side_effect
        def write(port, *values, **kwargs):
            original(port, *values, **kwargs)
            self.graph.source.values['label'] = 'changed during dispatch'
        self.graph.pm.setAttr.side_effect = write
        self.copy(self.graph.source, [self.graph.target], ['gain', 'label'])
        self.assertEqual(self.graph.target.values['label'], '担当者')

    def test_native_failure_reports_confirmed_partial_writes(self):
        self.graph.pm.setAttr.side_effect = [None, RuntimeError('refused')]
        with self.assertRaisesRegex(RuntimeError, 'after 1 writes'):
            self.copy(self.graph.source, [self.graph.target], ['gain', 'label'])

    def test_later_destination_range_violation_precedes_every_write(self):
        later = Node('later')
        later.values['gain'] = 0
        later.attr('gain').maximum = 2
        with self.assertRaises(ValueError):
            self.copy(self.graph.source, [self.graph.target, later], ['gain'])
        self.graph.pm.setAttr.assert_not_called()

    def test_snapshot_copy_does_not_require_containing_future_source_range(self):
        self.graph.source.values['gain'] = 1
        self.graph.target.values['gain'] = 0
        self.graph.target.attr('gain').minimum, self.graph.target.attr('gain').maximum = 0, 2
        self.assertEqual(self.copy(self.graph.source, [self.graph.target], ['gain']), 1)
        self.assertEqual(self.graph.target.values['gain'], 1)

    def test_enum_numeric_and_label_schema_mismatches_are_refused(self):
        for fields in ({'off': 10, 'on': 11}, {'left': 0, 'right': 1}):
            self.setUp()
            for node in (self.graph.source, self.graph.target): node.types['gain'] = 'enum'
            self.graph.source.values['gain'] = 1
            self.graph.target.values['gain'] = min(fields.values())
            self.graph.target.attr('gain').enums = fields
            with self.subTest(fields=fields), self.assertRaises(ValueError):
                self.copy(self.graph.source, [self.graph.target], ['gain'])
            self.graph.pm.setAttr.assert_not_called()

    def test_identical_enum_schemas_copy_without_remapping(self):
        for node in (self.graph.source, self.graph.target):
            node.types['gain'], node.values['gain'] = 'enum', 0
        self.graph.source.values['gain'] = 1
        self.assertEqual(self.copy(self.graph.source, [self.graph.target], ['gain']), 1)
        self.assertEqual(self.graph.target.values['gain'], 1)

    def test_compound_last_child_limit_prevents_scalar_prefix(self):
        self.graph.target.attr('offsetZ').maximum = 2
        self.graph.target.values['offset'] = (0, 0, 0)
        with self.assertRaises(ValueError):
            self.copy(self.graph.source, [self.graph.target], ['gain', 'offset'])
        self.graph.pm.setAttr.assert_not_called()

    def test_locked_destination_node_precedes_writes(self):
        self.graph.target.locked = True
        with self.assertRaises(ValueError): self.copy(self.graph.source, [self.graph.target], ['gain'])
        self.graph.pm.setAttr.assert_not_called()


class CopyCallbackTests(unittest.TestCase):
    def setUp(self):
        self.source = Node('source')
        self.pm = NS(textField=Mock(return_value='source'), textScrollList=Mock(return_value=None),
                     PyNode=Mock(), ls=Mock(return_value=[]), warning=Mock())
        self.operation = Mock(side_effect=ValueError('At least one node is required'))
        self.parameters = NS(copy_parameters=self.operation, resolve_nodes=Mock(), node_label=Mock())
        self.scope = {'pm': self.pm, 'parameters': self.parameters}
        path = 'python/DPyTools/operation/DCopyParamOp.py'
        self.run_callback = _callback(path, 'openWin', 'runCopyCB', self.scope)
        self.set_targets = _callback(path, 'openWin', 'setTargetCB', self.scope)
        self.set_source = _callback(path, 'openWin', 'setSourceCB', self.scope)
        self.ui = NS(SrcObjTextField='sourceField', objectListView='targets',
                     sourceNode=self.source, sourceLabel='source', targetNodes=[], targetLabels=[])

    def test_empty_none_view_warns_and_returns_false_without_typeerror(self):
        self.assertFalse(self.run_callback(self.ui))
        self.operation.assert_called_once_with(self.source, [])

    def test_retained_source_is_used_after_native_rename_and_no_name_relookup(self):
        self.source.label = 'renamed'
        self.ui.targetNodes, self.ui.targetLabels = ['targetHandle'], ['oldTargetLabel']
        self.pm.textScrollList.return_value = ['oldTargetLabel']
        self.operation.side_effect = None
        self.operation.return_value = 1
        self.assertTrue(self.run_callback(self.ui))
        self.operation.assert_called_once_with(self.source, ['targetHandle'])
        self.pm.PyNode.assert_not_called()

    def test_stale_target_view_refuses_operation(self):
        self.ui.targetLabels = ['target']
        self.assertFalse(self.run_callback(self.ui))
        self.operation.assert_not_called()

    def test_invalid_selection_preserves_source_and_target_models(self):
        self.parameters.resolve_nodes.side_effect = ValueError('invalid later item')
        self.assertFalse(self.set_targets(self.ui))
        self.assertEqual(self.ui.targetNodes, [])
        self.pm.textScrollList.assert_not_called()
        self.pm.ls.return_value = ['first', 'second']
        self.assertFalse(self.set_source(self.ui))
        self.assertIs(self.ui.sourceNode, self.source)
        self.pm.textField.assert_not_called()


if __name__ == '__main__':
    unittest.main()
