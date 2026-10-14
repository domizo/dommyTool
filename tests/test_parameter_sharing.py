#!/usr/bin/env python3
"""Actual sharing admission/model/UI callbacks with typed graph doubles."""

import numbers
import sys
import unittest
from types import SimpleNamespace
from unittest.mock import Mock

from test_python_rig_safety import functions
from test_python_table_selection import _callback
from DPyTools.core import strings
from DPyTools.scene import attribute_values
from DPyTools.ui import table_selection


class Port:
    def __init__(self, node, name):
        self.node, self.name = node, name
        self.incoming = []
        self.locked = False
        self.minimum = self.maximum = None
        self.enums = {'off': 0, 'on': 1}
    def inputs(self, **kwargs):
        return list(self.incoming)
    def getChildren(self):
        if self.node.types.get(self.name, '').endswith(('2', '3')):
            return [self.node.attr(self.name + axis) for axis in 'XYZ'[:int(self.node.types[self.name][-1])]]
        return []
    def getMin(self): return self.minimum
    def getMax(self): return self.maximum
    def getEnums(self): return self.enums


class Node:
    def __init__(self, name, kind='utility'):
        self.label, self.kind = name, kind
        self.ref = False
        self.locked = False
        self.values = {'gain': 7.0, 'offset': (1, 2, 3), 'label': '担当者'}
        self.types = {'gain': 'double', 'offset': 'double3', 'label': 'string'}
        self.ports = {name: Port(self, name) for name in self.values}
    def name(self):
        return self.label
    def nodeType(self):
        return self.kind
    def isReferenced(self):
        return self.ref
    def hasAttr(self, name):
        return name in self.values
    def attr(self, name):
        if name not in self.ports and name[-1:] in ('X', 'Y', 'Z') and name[:-1] in self.values:
            self.ports[name] = Port(self, name)
        return self.ports[name]


class Graph:
    def __init__(self):
        self.source = Node('source')
        self.target = Node('target')
        self.nodes = [self.source, self.target]
        self.created = []
        self.cleaned = []
        self.connections = []
        self.members = []
        graph = self
        class Owner:
            def __init__(self):
                self.nodes = []
            def create(self, kind, **kwargs):
                node = Node(kwargs['name'], kind)
                node.values = {'gain': 0.0, 'offset': (0, 0, 0), 'label': ''}
                self.nodes.append(node)
                graph.created.append(node)
                graph.nodes.append(node)
                return node
            def cleanup(self, **kwargs):
                graph.cleaned.extend(self.nodes)

        self.owner_type = Owner
        self.pm = SimpleNamespace(
            PyNode=self.resolve, nt=SimpleNamespace(DagNode=type('Dag', (), {}),
                                                   Shape=type('Shape', (), {})),
            getAttr=self.get_attr, setAttr=Mock(side_effect=self.set_attr),
            lockNode=lambda node, **kwargs: [node.locked],
            attributeQuery=self.query, objExists=lambda name: any(node.label == name for node in self.nodes),
            connectAttr=Mock(side_effect=self.connect),
            sets=Mock(side_effect=self.sets), listAttr=lambda node, **kwargs: list(node.values))

    def resolve(self, value):
        if isinstance(value, Node):
            return value
        matches = [node for node in self.nodes if node.label == value]
        if len(matches) != 1:
            raise ValueError('missing or ambiguous node')
        return matches[0]

    def get_attr(self, port, **kwargs):
        if kwargs.get('type'):
            return port.node.types.get(port.name, 'double')
        if kwargs.get('lock'):
            return port.locked
        if port.name in port.node.values:
            return port.node.values[port.name]
        return port.node.values[port.name[:-1]]['XYZ'.index(port.name[-1])]

    def query(self, name, **kwargs):
        if kwargs.get('listParent'):
            return [name[:-1]] if name not in kwargs['node'].values and name[-1:] in ('X', 'Y', 'Z') else []
        if kwargs.get('listChildren'):
            return [port.name for port in kwargs['node'].attr(name).getChildren()]
        return True

    def set_attr(self, port, *values, **kwargs):
        port.node.values[port.name] = values[0] if len(values) == 1 else values

    def connect(self, source, target, **kwargs):
        if target.incoming and not kwargs.get('force'):
            raise RuntimeError('existing connection')
        target.incoming = [source]
        self.connections.append((source, target, kwargs))

    def sets(self, group, **kwargs):
        if 'isMember' in kwargs:
            return kwargs['isMember'] in self.members
        self.members.append(kwargs['addElement'])


class SharingTests(unittest.TestCase):
    def setUp(self):
        self.graph = Graph()
        self.scope = functions('python/DPyTools/scene/parameters.py', {
            'pm': self.graph.pm, 'sys': sys, 'numbers': numbers, 'strings': strings,
            'attribute_values': attribute_values,
            'ownership': SimpleNamespace(SceneNodes=self.graph.owner_type),
            'MAX_CONNECTIONS': 10000,
            'SUPPORTED_TYPES': attribute_values.SCALAR_TYPES + attribute_values.COMPOUND_TYPES + ('string',)})

    def share(self, mode=3, attrs=('gain',), overwrite=False):
        return self.scope['share_parameters']([self.graph.source, self.graph.target],
                                               attrs, mode, 'share', overwrite)

    def test_existing_connection_is_preserved_without_explicit_permission(self):
        old = Port(Node('animation'), 'output')
        self.graph.target.attr('gain').incoming = [old]
        with self.assertRaises(ValueError):
            self.share()
        self.assertEqual(self.graph.target.attr('gain').incoming, [old])
        self.graph.pm.connectAttr.assert_not_called()

    def test_explicit_permission_is_forwarded_and_same_connection_is_idempotent(self):
        self.graph.target.attr('gain').incoming = [Port(Node('animation'), 'out')]
        self.share(overwrite=True)
        self.assertTrue(self.graph.connections[0][2]['force'])
        self.graph.pm.connectAttr.reset_mock()
        self.share()
        self.graph.pm.connectAttr.assert_not_called()

    def test_later_locked_or_mismatched_port_cannot_connect_valid_prefix(self):
        extra = Node('extra')
        extra.attr('gain').locked = True
        with self.assertRaises(ValueError):
            self.scope['share_parameters']([self.graph.source, self.graph.target, extra], ['gain'], 3)
        self.graph.target.types['gain'] = 'float'
        with self.assertRaises(ValueError):
            self.share()
        self.graph.pm.connectAttr.assert_not_called()

    def test_source_animation_or_lock_is_not_treated_as_destination(self):
        self.graph.source.attr('gain').incoming = [Port(Node('animation'), 'out')]
        self.graph.source.attr('gain').locked = True
        self.assertIs(self.share(), self.graph.source)

    def test_enum_meaning_mismatch_is_refused_before_dispatch(self):
        for node in (self.graph.source, self.graph.target): node.types['gain'] = 'enum'
        self.graph.source.values['gain'], self.graph.target.values['gain'] = 1, 10
        self.graph.target.attr('gain').enums = {'off': 10, 'on': 11}
        with self.assertRaises(ValueError): self.share()
        self.graph.pm.connectAttr.assert_not_called()

    def test_unbounded_or_wider_driver_range_is_refused_even_with_valid_current_value(self):
        self.graph.source.values['gain'] = self.graph.target.values['gain'] = 1
        self.graph.target.attr('gain').minimum, self.graph.target.attr('gain').maximum = 0, 2
        for minimum, maximum in ((None, None), (-1, 2), (0, 3)):
            self.graph.source.attr('gain').minimum, self.graph.source.attr('gain').maximum = minimum, maximum
            with self.subTest(minimum=minimum, maximum=maximum), self.assertRaises(ValueError): self.share()
        self.graph.pm.connectAttr.assert_not_called()

    def test_contained_live_range_and_matching_enum_meaning_are_allowed(self):
        self.graph.source.values['gain'] = self.graph.target.values['gain'] = 1
        self.graph.source.attr('gain').minimum, self.graph.source.attr('gain').maximum = 0, 1
        self.graph.target.attr('gain').minimum, self.graph.target.attr('gain').maximum = -1, 2
        self.assertIs(self.share(), self.graph.source)
        self.graph.pm.connectAttr.reset_mock()
        for node in (self.graph.source, self.graph.target):
            node.types['gain'], node.values['gain'] = 'enum', 1
            node.attr('gain').minimum = node.attr('gain').maximum = None
        self.assertIs(self.share(), self.graph.source)

    def test_later_compound_child_range_rejects_all_connections(self):
        self.graph.source.values['offset'] = self.graph.target.values['offset'] = (0, 0, 0)
        self.graph.target.attr('offsetZ').maximum = 2
        with self.assertRaises(ValueError): self.share(attrs=('gain', 'offset'))
        self.graph.pm.connectAttr.assert_not_called()

    def test_node_lock_cannot_be_bypassed_by_overwrite_permission(self):
        self.graph.target.locked = True
        with self.assertRaises(ValueError): self.share(overwrite=True)
        self.graph.pm.connectAttr.assert_not_called()

    def test_new_driver_full_schema_is_checked_before_initial_prefix_write(self):
        base = self.graph.owner_type
        class Owner(base):
            def create(owner, kind, **kwargs):
                node = super().create(kind, **kwargs)
                if kind != 'objectSet':
                    node.attr('offsetZ').maximum = 2
                return node
        self.scope['ownership'] = SimpleNamespace(SceneNodes=Owner)
        with self.assertRaises(RuntimeError): self.share(mode=1, attrs=('gain', 'offset'))
        self.graph.pm.setAttr.assert_not_called()
        self.graph.pm.connectAttr.assert_not_called()
        self.assertEqual(self.graph.cleaned, self.graph.created)

    def test_intrinsic_boolean_integer_and_enum_domains_are_not_treated_as_unbounded(self):
        for kind, bounds, value in (('bool', (0, 1), True), ('byte', (0, 255), 1), ('enum', (0, 1), 1)):
            self.setUp()
            for node in (self.graph.source, self.graph.target):
                node.types['gain'], node.values['gain'] = kind, value
            self.graph.target.attr('gain').minimum, self.graph.target.attr('gain').maximum = bounds
            with self.subTest(kind=kind): self.assertIs(self.share(), self.graph.source)

    def test_new_controller_copies_complete_numeric_and_unicode_values(self):
        controller = self.share(mode=1, attrs=('gain', 'offset', 'label'))
        self.assertEqual(controller.values, self.graph.source.values)
        self.assertEqual(len(self.graph.connections), 6)
        self.assertTrue(all(not kwargs['force'] for _, _, kwargs in self.graph.connections))

    def test_name_collision_and_ambiguous_names_fail_before_creation(self):
        self.graph.nodes.append(Node(':share', 'objectSet'))
        with self.assertRaises(ValueError):
            self.share(mode=1)
        for name in ('', 'a*', 'a|b', 'a::b', ':', ' a ', 'a\n'):
            with self.assertRaises(ValueError):
                self.scope['control_name'](name)
        self.assertFalse(self.graph.created)

    def test_references_and_unsupported_values_are_refused_before_connection(self):
        self.graph.target.ref = True
        with self.assertRaises(ValueError):
            self.share()
        self.graph.target.ref = False
        self.graph.target.values['gain'] = float('nan')
        with self.assertRaises(ValueError):
            self.share()
        self.graph.pm.connectAttr.assert_not_called()

    def test_model_merge_uses_existing_type_even_with_one_node_and_skips_duplicates(self):
        self.assertEqual(self.scope['merge_nodes']([self.graph.source], [self.graph.source]),
                         [self.graph.source])
        with self.assertRaises(ValueError):
            self.scope['merge_nodes']([self.graph.source], [Node('other', 'mesh')])

    def test_duplicate_models_and_invalid_mode_permission_are_refused(self):
        with self.assertRaises(ValueError):
            self.scope['resolve_nodes']([self.graph.source, self.graph.source])
        for mode in (True, 1.0, 0, 4):
            with self.assertRaises(ValueError):
                self.share(mode=mode)
        with self.assertRaises(TypeError):
            self.share(overwrite=1)

    def test_failure_before_dispatch_cleans_only_new_candidates(self):
        self.graph.pm.setAttr.side_effect = RuntimeError('initial value write failed')
        with self.assertRaises(RuntimeError):
            self.share(mode=1)
        self.assertEqual(self.graph.cleaned, self.graph.created)
        self.graph.pm.connectAttr.assert_not_called()

    def test_native_connection_failure_reports_partial_edits_and_keeps_driver(self):
        actual = self.graph.pm.connectAttr.side_effect
        calls = [0]
        def fail(source, target, **kwargs):
            calls[0] += 1
            if calls[0] == 2:
                raise RuntimeError('native failure')
            actual(source, target, **kwargs)
        self.graph.pm.connectAttr.side_effect = fail
        with self.assertRaisesRegex(RuntimeError, '1 completed connections'):
            self.share(mode=1)
        self.assertFalse(self.graph.cleaned)
        self.assertEqual(len(self.graph.connections), 1)


class ShareCallbackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scope = {'table_selection': table_selection}
        path = 'python/DPyTools/operation/DShareParamOp.py'
        cls.add = staticmethod(_callback(path, 'openWin', 'addSelObjCB', cls.scope))
        cls.remove = staticmethod(_callback(path, 'openWin', 'minusSelObjCB', cls.scope))
        cls.run_callback = staticmethod(_callback(path, 'openWin', 'runShareCB', cls.scope))

    def setUp(self):
        self.model = ['source']
        self.render = Mock()
        self.operation = Mock()
        self.pm = SimpleNamespace(ls=lambda **kw: ['added'], warning=Mock(),
                                  textScrollList=Mock(return_value=[1]),
                                  optionMenu=Mock(return_value=3), textField=Mock(return_value=''),
                                  checkBox=Mock(return_value=False))
        self.parameters = SimpleNamespace(merge_nodes=Mock(return_value=['source', 'added']),
                                          share_parameters=self.operation)
        self.scope.update(pm=self.pm, parameters=self.parameters)
        self.ui = SimpleNamespace(nodeList=self.model, _set_node_list=self.render,
                                  objectListView='objects', AttrListView='attrs',
                                  modeOptMenu='mode', cntlrNameTextField='name', overwriteCheck='permission')

    def test_failed_merge_keeps_old_model_and_view(self):
        self.parameters.merge_nodes.side_effect = ValueError('type mismatch')
        self.assertFalse(self.add(self.ui))
        self.render.assert_not_called()
        self.assertEqual(self.model, ['source'])

    def test_removal_uses_checked_indices_not_label_or_node_name(self):
        self.assertTrue(self.remove(self.ui))
        self.render.assert_called_once_with([])
        self.pm.textScrollList.return_value = [2]
        self.assertFalse(self.remove(self.ui))

    def test_run_forwards_default_off_permission_and_reports_failure(self):
        self.pm.textScrollList.return_value = ['gain']
        self.assertTrue(self.run_callback(self.ui))
        self.operation.assert_called_once_with(['source'], ['gain'], 3, '', False)
        self.operation.side_effect = RuntimeError('partial native failure')
        self.assertFalse(self.run_callback(self.ui))


if __name__ == '__main__':
    unittest.main()
