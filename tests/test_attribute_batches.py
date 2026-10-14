#!/usr/bin/env python3
"""Actual batch attribute/factory bodies with typed, failure-capable doubles."""

import math
import numbers
import re
import sys
import unittest
import uuid
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_python_rig_safety import functions
from test_scene_node_ownership import definitions
from DPyTools.core import strings
from DPyTools.scene import attribute_values


class Port:
    def __init__(self, node, name, kind='double', value=0.0):
        self.owner, self.label, self.kind, self.value = node, name, kind, value
        self.parents, self.children, self.incoming = [], [], []
        self.locked = self.array = False
        self.writable = self.readable = self.connectable = True
        self.minimum = self.maximum = None
        self.enums = {'zero': 0, 'one': 1}
    def node(self):
        return self.owner
    def lastPlugAttr(self, **kwargs):
        return self.label.split('.')[-1].split('[')[0]
    def isArray(self):
        return self.array
    def isCompound(self):
        return bool(self.children)
    def getAllParents(self, **kwargs):
        return list(self.parents)
    def getChildren(self):
        return list(self.children)
    def getMin(self):
        return self.minimum
    def getMax(self):
        return self.maximum
    def getEnums(self):
        return self.enums
    def inputs(self, **kwargs):
        return list(self.incoming)
    def isReadable(self):
        return self.readable
    def isConnectable(self):
        return self.connectable
    def __getitem__(self, index):
        return self.owner.attr(self.label + '[{0}]'.format(index))


class Node:
    def __init__(self, name, kind='utility', parent=None):
        self.label, self.kind, self.parent = name, kind, parent
        self.children, self.ports = [], {}
        self.exists = True
        self.referenced = self.instanced = self.locked = False
        if parent is not None:
            parent.children.append(self)
        self.add_port('gain')
        self.add_port('other')
        self.add_port('label', 'string', '')
    def add_port(self, name, kind='double', value=0.0):
        port = Port(self, name, kind, value)
        self.ports[name] = port
        return port
    def attr(self, name):
        if name not in self.ports:
            raise ValueError('missing attribute: ' + name)
        return self.ports[name]
    def hasAttr(self, name):
        return name in self.ports
    def name(self):
        return self.label
    def nodeType(self):
        return self.kind
    def isReferenced(self):
        return self.referenced
    def isInstanced(self):
        return self.instanced
    def getParent(self):
        return self.parent


class DagNode(Node):
    pass


class Graph:
    def __init__(self):
        self.nodes, self.created, self.deleted, self.connections = [], [], [], []
        self.pm = NS(PyNode=self.resolve, nt=NS(DagNode=DagNode),
                     lockNode=lambda node, **kw: [node.locked],
                     getAttr=self.get_attr, attributeQuery=self.query,
                     setAttr=Mock(side_effect=self.set_attr),
                     addAttr=Mock(side_effect=self.add_attr),
                     createNode=Mock(side_effect=self.create),
                     objExists=self.exists, nodeType=lambda kind, **kw: ['shape'] if kind in ('mesh', 'nurbsCurve') else [],
                     listRelatives=self.relatives, delete=Mock(side_effect=self.delete),
                     connectAttr=Mock(side_effect=self.connect), warning=Mock())
        self.parameters = NS(**functions('python/DPyTools/scene/parameters.py', {
            'pm': self.pm, 'sys': sys, 're': re, 'numbers': numbers, 'strings': strings,
            'attribute_values': attribute_values, 'MAX_CONNECTIONS': 10000,
            'SUPPORTED_TYPES': attribute_values.SCALAR_TYPES + attribute_values.COMPOUND_TYPES + ('string',)}))
        self.ownership = NS(**definitions('python/DPyTools/scene/ownership.py', {'pm': self.pm, 'uuid': uuid}))
        self.attributes = NS(**functions('python/DPyTools/scene/attributes.py', {
            'pm': self.pm, 'parameters': self.parameters, 'attribute_values': attribute_values, 'string_utils': strings}))

    def resolve(self, value):
        if isinstance(value, (Node, Port)):
            return value
        if '.' in value:
            node, attr = value.split('.', 1)
            return self.resolve(node).attr(attr)
        matches = [node for node in self.nodes if node.exists and node.label.lstrip(':') == value.lstrip(':')]
        if len(matches) != 1:
            raise ValueError('missing or ambiguous node')
        return matches[0]
    def create(self, kind, **kwargs):
        cls = DagNode if kind in ('transform', 'joint', 'mesh', 'nurbsCurve', 'particle', 'nParticle') else Node
        node = cls(kwargs.get('name', kind + str(len(self.nodes))), kind, kwargs.get('parent'))
        self.nodes.append(node)
        self.created.append(node)
        return node
    def exists(self, value):
        if isinstance(value, Node):
            return value.exists
        return any(node.exists and node.label.lstrip(':') == value.lstrip(':') for node in self.nodes)
    def relatives(self, node, **kwargs):
        if kwargs.get('parent'):
            return [node.parent] if node.parent else []
        result, queue = [], list(node.children)
        while queue:
            child = queue.pop(0)
            if child.exists:
                result.append(child)
                queue.extend(child.children)
        return result
    def delete(self, node):
        for item in [node] + self.relatives(node):
            item.exists = False
        self.deleted.append(node)
    def get_attr(self, port, **kwargs):
        if kwargs.get('type'):
            return port.kind
        if kwargs.get('lock'):
            return port.locked
        return port.value
    def query(self, name, **kwargs):
        port = next(port for port in kwargs['node'].ports.values() if port.lastPlugAttr() == name)
        return port.writable
    def set_attr(self, port, *values, **kwargs):
        port.value = values[0] if len(values) == 1 else values
    def add_attr(self, node, **kwargs):
        node.add_port(kwargs['longName'], kwargs.get('attributeType', kwargs.get('dataType')), kwargs.get('defaultValue', 0))
    def connect(self, source, target, **kwargs):
        target.incoming = [source]
        self.connections.append((source, target, kwargs))


class AttributeBatchTests(unittest.TestCase):
    def setUp(self):
        self.graph = Graph()
        self.node = self.graph.create('utility', name='source')
        self.set_values = self.graph.attributes.setMultiAttributes
    def test_late_missing_attribute_does_not_write_valid_prefix(self):
        with self.assertRaises(ValueError):
            self.set_values(self.node, {'gain': 7, 'missing': 9})
        self.graph.pm.setAttr.assert_not_called()
    def test_late_locked_connected_unwritable_or_referenced_input_is_rejected(self):
        for state in ('lock', 'input', 'write', 'reference', 'nodeLock'):
            self.setUp()
            if state == 'lock': self.node.attr('other').locked = True
            if state == 'input': self.node.attr('other').incoming = [self.node.attr('label')]
            if state == 'write': self.node.attr('other').writable = False
            if state == 'reference': self.node.referenced = True
            if state == 'nodeLock': self.node.locked = True
            with self.subTest(state=state), self.assertRaises(ValueError):
                self.set_values(self.node, {'gain': 7, 'other': 9})
            self.graph.pm.setAttr.assert_not_called()
    def test_nonfinite_range_and_enum_errors_precede_all_writes(self):
        port = self.node.attr('other')
        for kind, value, maximum in [('double', math.inf, None), ('double', 10, 2), ('double', 1, math.nan), ('enum', 7, None)]:
            port.kind, port.maximum = kind, maximum
            with self.subTest(kind=kind, value=value), self.assertRaises(ValueError):
                self.set_values(self.node, {'gain': 7, 'other': value})
        self.graph.pm.setAttr.assert_not_called()
    def test_explicit_array_elements_are_preserved_and_whole_arrays_rejected(self):
        root = self.node.add_port('input1D')
        root.array = True
        element = self.node.add_port('input1D[0]')
        element.parents = [root]
        self.assertEqual(self.set_values(self.node, {'input1D[0]': 3}), 1)
        self.assertEqual(element.value, 3)
        with self.assertRaises(ValueError): self.set_values(self.node, {'input1D': 2})
    def test_alias_duplicate_and_parent_child_overlap_are_rejected(self):
        self.node.ports['alias'] = self.node.attr('gain')
        with self.assertRaises(ValueError): self.set_values(self.node, {'gain': 1, 'alias': 2})
        compound = self.node.add_port('offset', 'double3', (0, 0, 0))
        compound.children = [self.node.add_port('offset' + axis) for axis in 'XYZ']
        for child in compound.children: child.parents = [compound]
        with self.assertRaises(ValueError): self.set_values(self.node, {'offset': (1, 2, 3), 'offsetX': 9})
        self.graph.pm.setAttr.assert_not_called()
    def test_compound_child_and_parent_locks_and_connections_are_checked(self):
        compound = self.node.add_port('offset', 'double3', (0, 0, 0))
        compound.children = [self.node.add_port('offset' + axis) for axis in 'XYZ']
        for child in compound.children: child.parents = [compound]
        compound.children[-1].locked = True
        with self.assertRaises(ValueError): self.set_values(self.node, {'offset': (1, 2, 3)})
        compound.children[-1].locked = False
        compound.incoming = [self.node.attr('gain')]
        with self.assertRaises(ValueError): self.set_values(self.node, {'offsetX': 4})
        self.graph.pm.setAttr.assert_not_called()
    def test_string_and_vector_dispatch_keep_explicit_types_and_all_components(self):
        vector = self.node.add_port('offset', 'float3')
        vector.children = [self.node.add_port('offset' + axis, 'float') for axis in 'XYZ']
        self.assertEqual(self.set_values(self.node, {'label': '担当者', 'offset': (1, 2, 3)}), 2)
        self.graph.pm.setAttr.assert_any_call(self.node.attr('label'), '担当者', type='string')
        self.graph.pm.setAttr.assert_any_call(vector, 1.0, 2.0, 3.0, type='float3')
    def test_malformed_paths_mapping_and_huge_indices_fail(self):
        for value in ([], {'input[-1]': 2}, {'input[*]': 2}, {'gain\n': 2}, {'input[2147483648]': 2}):
            with self.subTest(value=value), self.assertRaises(ValueError): self.set_values(self.node, value)
        self.graph.pm.setAttr.assert_not_called()
    def test_native_failure_reports_confirmed_prefix_without_undo(self):
        self.graph.pm.setAttr.side_effect = [None, RuntimeError('refused')]
        with self.assertRaisesRegex(RuntimeError, 'after 1 writes'):
            self.set_values(self.node, {'gain': 1, 'other': 2})
    def test_bad_later_definition_and_name_collision_precede_additions(self):
        valid = {'ln': 'newGain', 'at': 'float', 'dv': 1.0}
        for invalid in ({'ln': 'bad', 'at': 'float', 'dv': math.nan}, {'ln': 'gain', 'at': 'float'}, dict(valid), {'ln': 'bad', 'at': 'float', 'query': True}):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                self.graph.attributes.createAttributeFromDic(self.node, [valid, invalid])
        self.graph.pm.addAttr.assert_not_called()
    def test_definitions_preserve_legacy_flags_and_explicit_enum_values(self):
        self.assertEqual(self.graph.attributes.createAttributeFromDic(self.node, [
            {'ln': 'separator', 'at': 'float', 'dv': False, 'h': False, 'min': 0, 'max': 1},
            {'ln': 'choice', 'at': 'enum', 'enumName': 'zero:ten=10:eleven', 'dv': 10}]), 2)
        self.assertEqual(self.node.attr('separator').value, 0.0)
    def test_invalid_enum_range_and_duplicate_flag_definitions_fail(self):
        for spec in ({'ln': 'x', 'at': 'enum', 'enumName': 'one=1'}, {'ln': 'x', 'at': 'enum', 'enumName': 'a:b=0'}, {'ln': 'x', 'at': 'float', 'min': 2, 'max': 1}, {'ln': 'x', 'longName': 'y', 'at': 'float'}, {'ln': 'x', 'dt': 'string', 'hnv': True}):
            with self.subTest(spec=spec), self.assertRaises(ValueError): self.graph.attributes.createAttributeFromDic(self.node, [spec])
        self.graph.pm.addAttr.assert_not_called()
    def test_native_add_failure_reports_partial_without_deleting_borrowed_attributes(self):
        self.graph.pm.addAttr.side_effect = [None, RuntimeError('refused')]
        with self.assertRaisesRegex(RuntimeError, 'after 1 additions'):
            self.graph.attributes.createAttributeFromDic(self.node, [{'ln': 'a', 'at': 'float'}, {'ln': 'b', 'at': 'float'}])
        self.graph.pm.delete.assert_not_called()
    def test_late_bad_connection_prevents_every_connection(self):
        target = self.graph.create('utility', name='target')
        target.attr('other').locked = True
        with self.assertRaises(ValueError): self.graph.attributes.connectAttrToList(self.node.attr('gain'), [target.attr('gain'), target.attr('other')])
        self.graph.pm.connectAttr.assert_not_called()
    def test_numeric_conversion_and_idempotent_connection_are_retained(self):
        target = self.graph.create('utility', name='target')
        target.attr('gain').kind = 'float'
        self.assertEqual(self.graph.attributes.connectAttrToList(self.node.attr('gain'), [target.attr('gain')]), 1)
        self.assertEqual(self.graph.attributes.connectAttrToList(self.node.attr('gain'), [target.attr('gain')]), 0)
        self.assertFalse(self.graph.connections[0][2].get('force', False))
    def test_idempotent_connection_still_checks_destination_locks(self):
        target = self.graph.create('utility', name='target')
        target.attr('gain').incoming = [self.node.attr('gain')]
        target.attr('gain').locked = True
        with self.assertRaises(ValueError):
            self.graph.attributes.connectAttrToList(self.node.attr('gain'), [target.attr('gain')])
        self.graph.pm.connectAttr.assert_not_called()
    def test_duplicate_destinations_and_self_links_fail(self):
        target = self.graph.create('utility', name='target')
        for values in ([target.attr('gain')] * 2, [self.node.attr('gain')]):
            with self.assertRaises(ValueError): self.graph.attributes.connectAttrToList(self.node.attr('gain'), values)
        self.graph.pm.connectAttr.assert_not_called()


class NodeFactoryTests(unittest.TestCase):
    def setUp(self):
        self.graph = Graph()
        self.scope = functions('python/DPyTools/scene/nodes.py', {'pm': self.graph.pm, 'sys': sys,
            'parameters': self.graph.parameters, 'ownership': self.graph.ownership, 'dattr': self.graph.attributes})
    def test_bad_later_value_cleans_only_known_created_node(self):
        borrowed = self.graph.create('utility', name='borrowed')
        with self.assertRaises(ValueError): self.scope['createNodeWithParam']('utility', 'new', {'gain': 1, 'missing': 2})
        self.assertTrue(borrowed.exists)
        self.assertFalse(self.graph.created[-1].exists)
        self.graph.pm.setAttr.assert_not_called()
    def test_success_returns_handle_and_uses_skip_select(self):
        node = self.scope['createNodeWithParam']('utility', 'new', {'gain': 2})
        self.assertEqual(node.attr('gain').value, 2)
        self.graph.pm.createNode.assert_called_once_with('utility', name=':new', skipSelect=True)
    def test_mapping_shape_or_name_collision_prevents_allocation(self):
        self.graph.create('utility', name='occupied')
        self.graph.pm.createNode.reset_mock()
        for kind, name, values in [('utility', 'new', []), ('mesh', 'new', {}), ('utility', 'occupied', {})]:
            with self.subTest(kind=kind, name=name), self.assertRaises(ValueError): self.scope['createNodeWithParam'](kind, name, values)
        self.graph.pm.createNode.assert_not_called()
    def test_interruption_cleans_known_node_and_propagates(self):
        self.graph.pm.setAttr.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt): self.scope['createNodeWithParam']('utility', 'new', {'gain': 2})
        self.assertFalse(self.graph.created[-1].exists)


if __name__ == '__main__':
    unittest.main()
