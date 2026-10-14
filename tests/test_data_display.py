#!/usr/bin/env python3
"""Actual display bodies preserve unrelated source fanout and known ownership."""

import sys
import ast
import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_python_rig_safety import functions
from test_python_table_selection import _tree
from DPyTools.core import strings


class Port:
    def __init__(self, node, name, kind):
        self.owner, self.name, self.kind = node, name, kind
        self.incoming, self.outgoing = [], []
        self.locked = False
    def node(self):
        return self.owner
    def inputs(self, **kwargs):
        return list(self.incoming)


class Node:
    def __init__(self, kind='producer'):
        self.kind, self.ref, self.instance = kind, False, False
        self.ports = {}
    def nodeType(self):
        return self.kind
    def isReferenced(self):
        return self.ref
    def isInstanced(self):
        return self.instance
    def hasAttr(self, name):
        return name in self.ports
    def attr(self, name):
        return self.ports[name]


class DisplayTests(unittest.TestCase):
    def setUp(self):
        self.source, self.display = Node(), Node('DPointDisplay')
        self.source.ports['outDP'] = Port(self.source, 'outDP', 'DPData')
        self.display.ports['inDP'] = Port(self.display, 'inDP', 'DPData')
        self.created, self.cleaned = [], []
        test = self
        class Owner:
            def __init__(self):
                self.nodes = []
            def create(self, kind, **kwargs):
                node = Node(kind)
                if kind != 'transform':
                    port = dict(test.ports)[kind]
                    node.ports[port] = Port(node, port, port[2:] + 'Data')
                    if 'parent' not in kwargs:
                        raise AssertionError('Display needs an explicitly owned parent')
                self.nodes.append(node)
                test.created.append(node)
                return node
            def cleanup(self, **kwargs):
                test.cleaned.extend(reversed(self.nodes))
        self.pm = NS(PyNode=lambda node: node,
                     attributeQuery=Mock(return_value=True),
                     addAttr=Mock(side_effect=lambda port, **kw: [port.kind]),
                     getAttr=lambda port, **kw: port.kind if kw.get('type') else port.locked,
                     connectAttr=Mock(side_effect=self.connect), warning=Mock())
        self.ports = (('DPointDisplay', 'inDP'), ('DCurveDisplay', 'inDC'),
                      ('DCMarker', 'inDC'), ('DMeshDisplay', 'inDM'), ('DMMarker', 'inDM'))
        self.names = Mock(side_effect=lambda values: None if values and values[0] else (_ for _ in ()).throw(ValueError('empty name')))
        self.scope = functions('python/DPyTools/scene/data_display.py', {
            'pm': self.pm, 'sys': sys, 'DISPLAY_PORTS': self.ports,
            'strings': strings,
            'parameters': NS(attribute_names=self.names), 'ownership': NS(SceneNodes=Owner)})

    def connect(self, source, target, **kwargs):
        for old in target.incoming:
            if target in old.outgoing:
                old.outgoing.remove(target)
        target.incoming = [source]
        source.outgoing.append(target)

    def switch(self):
        return self.scope['switch_display'](self.source, self.display)

    def test_switch_changes_only_the_selected_display_input(self):
        output = self.source.attr('outDP')
        unrelated = [object(), object()]
        output.outgoing = list(unrelated)
        old = Port(Node(), 'old', 'DPData')
        old.outgoing = [self.display.attr('inDP')]
        self.display.attr('inDP').incoming = [old]
        self.assertIs(self.switch(), self.display)
        self.assertEqual(output.outgoing, unrelated + [self.display.attr('inDP')])
        self.assertEqual(old.outgoing, [])

    def test_failed_native_switch_does_not_pre_disconnect_any_fanout(self):
        output = self.source.attr('outDP')
        output.outgoing = [object(), object()]
        previous = list(output.outgoing)
        self.pm.connectAttr.side_effect = RuntimeError('native refused')
        self.assertFalse(self.switch())
        self.assertEqual(output.outgoing, previous)

    def test_locked_wrong_type_reference_instance_and_wrong_node_refuse_dispatch(self):
        for property_name in ('ref', 'instance'):
            setattr(self.display, property_name, True)
            self.assertFalse(self.switch())
            setattr(self.display, property_name, False)
        self.display.attr('inDP').locked = True
        self.assertFalse(self.switch())
        self.display.attr('inDP').locked = False
        self.display.attr('inDP').kind = 'DMData'
        self.assertFalse(self.switch())
        self.display.kind = 'utility'
        self.assertFalse(self.switch())
        self.pm.connectAttr.assert_not_called()

    def test_same_connection_is_idempotent_even_if_current_input_is_locked(self):
        self.display.attr('inDP').incoming = [self.source.attr('outDP')]
        self.display.attr('inDP').locked = True
        self.assertIs(self.switch(), self.display)
        self.pm.connectAttr.assert_not_called()

    def test_self_driving_display_is_rejected(self):
        self.display.ports['outDP'] = Port(self.display, 'outDP', 'DPData')
        self.assertFalse(self.scope['switch_display'](self.display, self.display))
        self.pm.connectAttr.assert_not_called()

    def test_bad_source_does_not_create_or_disconnect_nodes(self):
        self.assertFalse(self.scope['create_display']('DPointDisplay', self.source, 'missing'))
        self.assertEqual(self.created, [])
        self.pm.connectAttr.assert_not_called()

    def test_all_supported_display_shapes_receive_explicit_owned_parents(self):
        for kind, port in self.ports:
            output_name = 'out' + port[2:]
            self.source.ports[output_name] = Port(self.source, output_name, port[2:] + 'Data')
            display = self.scope['create_display'](kind, self.source, output_name)
            self.assertEqual(display.nodeType(), kind)
        self.assertEqual(len(self.created), 10)
        self.assertFalse(self.cleaned)

    def test_failed_creation_cleans_new_nodes_only(self):
        self.pm.connectAttr.side_effect = RuntimeError('failed input')
        self.assertFalse(self.scope['create_display']('DPointDisplay', self.source))
        self.assertEqual(self.cleaned, list(reversed(self.created)))
        self.assertNotIn(self.source, self.cleaned)

    def test_interruption_is_not_hidden_and_known_candidate_is_cleaned(self):
        self.pm.connectAttr.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            self.scope['create_display']('DPointDisplay', self.source)
        self.assertEqual(self.cleaned, list(reversed(self.created)))

    def test_compatibility_check_is_read_only_and_facade_delegates(self):
        self.assertIs(self.scope['check_source'](self.source), self.source.attr('outDP'))
        self.pm.connectAttr.assert_not_called()
        tree = _tree('python/DPyTools/dpoint/display/DPointDisplayOp.py')
        exports = [node for node in tree.body if isinstance(node, ast.ImportFrom)]
        self.assertEqual(exports[0].module, 'DPyTools.scene')
        self.assertEqual(exports[0].names[0].name, 'data_display')
        assigned = [node.targets[0].id for node in tree.body if isinstance(node, ast.Assign)]
        self.assertIn('createDPointDisplay', assigned)
        self.assertIs(self.scope['dp_dsiplay_connect_check'](self.source), self.source.attr('outDP'))

    def test_uninitialized_payload_does_not_replace_declared_type_admission(self):
        self.pm.getAttr = Mock(side_effect=lambda port, **kw: None if kw.get('type') else False)
        self.assertTrue(self.scope['create_display']('DPointDisplay', self.source))
        self.assertTrue(all(not call.kwargs.get('type') for call in self.pm.getAttr.call_args_list))


if __name__ == '__main__':
    unittest.main()
