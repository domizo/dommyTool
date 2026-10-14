#!/usr/bin/env python3
"""Actual custom-data conversion, input/fanout admission and group ownership."""

import sys
import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_attribute_batches import Graph, Port, DagNode
from test_python_rig_safety import functions
from DPyTools.core import strings


class Component:
    pass


class ConversionGraph(Graph):
    def __init__(self):
        super().__init__()
        self.selected = []
        self.pm.general = NS(Attribute=Port, Component=Component)
        self.pm.ls = lambda **kw: list(self.selected)
        self.pm.select, self.pm.curve, self.pm.polyPlane, self.pm.disconnectAttr = Mock(), Mock(), Mock(), Mock()
        self.pm.addAttr = Mock(side_effect=lambda port, **kw: [port.kind])
        self.pm.getAttr = Mock(side_effect=self.get_attr)
        self.display = NS(**functions('python/DPyTools/scene/data_display.py', {
            'pm': self.pm, 'sys': sys, 'strings': strings, 'parameters': self.parameters,
            'DISPLAY_PORTS': (('DCurveDisplay', 'inDC'), ('DCMarker', 'inDC'), ('DMeshDisplay', 'inDM'), ('DMMarker', 'inDM'))}))
        self.convert = NS(**functions('python/DPyTools/features/data_conversion.py', {
            'pm': self.pm, 'sys': sys, 'ownership': self.ownership, 'data_display': self.display}))
        self.producer = self.create('producer')
        self.producer.add_port('outDC', 'DCurveData')
        self.producer.add_port('outDM', 'DMeshData')
        self.selected = [self.producer]
    def create(self, kind, **kwargs):
        node = super().create('transform' if kind in ('curveVarGroup', 'meshVarGroup', 'DCurveDisplay', 'DMeshDisplay') else kind, **kwargs)
        node.kind = kind
        node.getShapes = lambda **kw: [child for child in node.children if child.kind not in ('transform', 'joint')]
        if kind in ('DCToCurves', 'DMToMesh'):
            first, second, schema, output = ('inDC', 'outCurves', 'DCurveData', 'nurbsCurve') if kind == 'DCToCurves' else ('inDM', 'outMesh', 'DMeshData', 'mesh')
            node.add_port(first, schema)
            node.add_port(second, output)
        if kind in ('curveVarGroup', 'meshVarGroup'):
            node.add_port('create', 'nurbsCurve' if kind == 'curveVarGroup' else 'mesh')
            node.add_port('local', 'nurbsCurve' if kind == 'curveVarGroup' else 'mesh')
        if kind in ('DCurveDisplay', 'DMeshDisplay'):
            node.add_port('inDC' if kind == 'DCurveDisplay' else 'inDM', 'DCurveData' if kind == 'DCurveDisplay' else 'DMeshData')
        return node


class ConversionTests(unittest.TestCase):
    def setUp(self): self.graph = ConversionGraph()
    def test_both_converters_connect_live_data_and_use_only_native_groups(self):
        for method, output, converter_kind, group_kind, input_name in (
                ('createDCConvertToCurve', 'outDC', 'DCToCurves', 'curveVarGroup', 'inDC'),
                ('createDMToMesh', 'outDM', 'DMToMesh', 'meshVarGroup', 'inDM')):
            self.setUp()
            converter, group = getattr(self.graph.convert, method)()
            self.assertEqual((converter.kind, group.kind), (converter_kind, group_kind))
            self.assertEqual(self.graph.connections[0][:2], (self.graph.producer.attr(output), converter.attr(input_name)))
            self.assertEqual(len(self.graph.connections), 2)
            self.assertTrue(all(not options.get('force', False) for _, _, options in self.graph.connections))
            self.assertTrue(all(call.kwargs['skipSelect'] for call in self.graph.pm.createNode.call_args_list))
            self.graph.pm.getAttr.assert_called_once_with(group.attr('local'), silent=True)
            self.graph.pm.curve.assert_not_called()
            self.graph.pm.polyPlane.assert_not_called()
            self.graph.pm.select.assert_not_called()
    def test_explicit_plug_and_alternate_output_name_are_supported(self):
        self.assertTrue(self.graph.convert.createDCConvertToCurve(self.graph.producer.attr('outDC')))
        self.graph.producer.add_port('alternate', 'DCurveData')
        self.assertTrue(self.graph.convert.createDCConvertToCurve(self.graph.producer, 'alternate'))
    def test_display_selection_follows_its_borrowed_driver(self):
        display = self.graph.create('DCurveDisplay')
        display.attr('inDC').incoming = [self.graph.producer.attr('outDC')]
        root = self.graph.create('transform')
        root.children = [display]
        self.graph.selected = [root]
        self.assertTrue(self.graph.convert.createDCConvertToCurve())
        self.assertEqual(self.graph.connections[0][0], self.graph.producer.attr('outDC'))
        self.assertTrue(display.exists)
    def test_empty_multiple_component_and_ambiguous_shape_selection_precede_creation(self):
        root = self.graph.create('transform')
        root.children = [self.graph.create('DCurveDisplay'), self.graph.create('DCurveDisplay')]
        self.graph.pm.createNode.reset_mock()
        for values in ([], [self.graph.producer, self.graph.producer], [root]):
            self.graph.selected = values
            with self.subTest(values=values): self.assertFalse(self.graph.convert.createDCConvertToCurve())
        original = self.graph.pm.PyNode
        self.graph.pm.PyNode = lambda value: value if isinstance(value, Component) else original(value)
        self.graph.selected = [Component()]
        self.assertFalse(self.graph.convert.createDCConvertToCurve())
        self.graph.pm.createNode.assert_not_called()
    def test_wrong_schema_unreadable_array_and_instanced_source_precede_creation(self):
        port = self.graph.producer.attr('outDC')
        for state in ('schema', 'read', 'connect', 'array'):
            port.kind, port.readable, port.connectable, port.array = 'DCurveData', True, True, False
            if state == 'schema': port.kind = 'DMeshData'
            if state == 'read': port.readable = False
            if state == 'connect': port.connectable = False
            if state == 'array': port.array = True
            with self.subTest(state=state): self.assertFalse(self.graph.convert.createDCConvertToCurve(port))
        self.graph.pm.createNode.assert_not_called()
        root = self.graph.create('transform')
        root.add_port('outDC', 'DCurveData')
        root.instanced = True
        self.graph.pm.createNode.reset_mock()
        self.assertFalse(self.graph.convert.createDCConvertToCurve(root.attr('outDC')))
        self.graph.pm.createNode.assert_not_called()
    def test_unconnected_or_multiply_connected_display_is_refused(self):
        display = self.graph.create('DMeshDisplay')
        self.graph.pm.createNode.reset_mock()
        for inputs in ([], [self.graph.producer.attr('outDM')] * 2):
            display.attr('inDM').incoming = inputs
            self.assertFalse(self.graph.convert.createDMToMesh(display))
        self.graph.pm.createNode.assert_not_called()
    def test_source_fanout_is_never_disconnected(self):
        self.graph.producer.attr('outDC').outgoing = [object(), object()]
        before = list(self.graph.producer.attr('outDC').outgoing)
        self.assertTrue(self.graph.convert.createDCConvertToCurve())
        self.assertEqual(self.graph.producer.attr('outDC').outgoing, before)
        self.graph.pm.disconnectAttr.assert_not_called()
    def test_native_connection_failure_cleans_known_nodes_not_input(self):
        self.graph.pm.connectAttr.side_effect = RuntimeError('failed')
        self.assertFalse(self.graph.convert.createDCConvertToCurve())
        self.assertTrue(self.graph.producer.exists)
        self.assertTrue(all(not node.exists for node in self.graph.created if node is not self.graph.producer))
    def test_native_evaluation_unknown_descendant_is_not_blindly_deleted(self):
        unknown = []
        def evaluate(port, **kwargs):
            unknown.append(self.graph.create('mesh', parent=port.node()))
            raise RuntimeError('native evaluation failed')
        self.graph.pm.getAttr.side_effect = evaluate
        self.assertFalse(self.graph.convert.createDMToMesh())
        self.assertTrue(unknown[0].exists and unknown[0].parent.exists)
        self.assertGreaterEqual(self.graph.pm.warning.call_count, 2)
    def test_loaded_converter_schema_mismatch_cleans_candidate_before_connecting(self):
        original = self.graph.pm.addAttr.side_effect
        self.graph.pm.addAttr.side_effect = lambda port, **kw: ['DMeshData'] if port.label == 'inDC' else original(port, **kw)
        self.assertFalse(self.graph.convert.createDCConvertToCurve())
        self.graph.pm.connectAttr.assert_not_called()
        self.assertFalse(self.graph.created[-1].exists)
    def test_interrupt_cleans_known_graph_and_propagates(self):
        self.graph.pm.connectAttr.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt): self.graph.convert.createDMToMesh()
        self.assertTrue(self.graph.producer.exists)
        self.assertTrue(all(not node.exists for node in self.graph.created if node is not self.graph.producer))
    def test_uninitialized_payload_keeps_declared_schema(self):
        self.graph.producer.attr('outDC').value = None
        self.assertTrue(self.graph.convert.createDCConvertToCurve())


if __name__ == '__main__': unittest.main()
