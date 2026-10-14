#!/usr/bin/env python3
"""Known-node cleanup and actual measurement ownership with scene doubles."""

import ast
import sys
import unittest
import uuid
from types import SimpleNamespace
from unittest.mock import Mock

from test_python_table_selection import _tree
from test_python_rig_safety import functions
from DPyTools.core.numeric import finite_number


def definitions(relative, namespace):
    tree = _tree(relative)
    body = [node for node in tree.body
            if isinstance(node, (ast.Assign, ast.FunctionDef, ast.ClassDef))]
    module = ast.fix_missing_locations(ast.Module(body=body, type_ignores=[]))
    exec(compile(module, relative, 'exec'), namespace)
    return namespace


class Plug:
    def __init__(self, node, name):
        self.node, self.name = node, name
    def __getitem__(self, index):
        return Plug(self.node, self.name + '[{}]'.format(index))
    def get(self):
        return self.node.values[self.name]
    def set(self, value):
        self.node.values[self.name] = value
    def lock(self):
        pass


class DagNode:
    def __init__(self, name, kind='transform', parent=None):
        self.label, self.kind, self.parent = name, kind, parent
        self.children = []
        if parent is not None:
            parent.children.append(self)
        self.values = {}
        self.referenced = self.instanced = False
        self.exists = True
    def nodeType(self):
        return self.kind
    def name(self):
        return self.label
    def isReferenced(self):
        return self.referenced
    def isInstanced(self):
        return self.instanced
    def hasAttr(self, name):
        return name in self.values
    def addAttr(self, name, **kwargs):
        if name in self.values:
            raise ValueError('attribute exists')
        self.values[name] = None
    def attr(self, name):
        return Plug(self, name)
    def __getattr__(self, name):
        if name in ('worldPosition', 'startPoint', 'endPoint'):
            return self.attr(name)
        raise AttributeError(name)


class Scene:
    def __init__(self):
        self.nodes = []
        self.selection = []
        self.deleted = []
        self.created = []
        self.connections = []
        self.fail_delete = set()
        self.pm = SimpleNamespace(
            PyNode=self.resolve, createNode=Mock(side_effect=self.create),
            objExists=lambda value: value.exists if isinstance(value, DagNode)
            else any(node.exists and node.name() == value for node in self.nodes),
            nt=SimpleNamespace(DagNode=DagNode), listRelatives=self.relatives,
            delete=Mock(side_effect=self.delete), warning=Mock(),
            ls=lambda **kw: self.selection,
            xform=Mock(return_value=[0, 0, 0, 2, 3, 4]),
            connectAttr=Mock(side_effect=lambda source, target, **kw:
                             self.connections.append((source, target, kw))),
            rename=Mock(side_effect=self.rename))

    def resolve(self, value):
        if isinstance(value, DagNode):
            return value
        return next(node for node in self.nodes if node.exists and node.name() == value)

    def create(self, kind, **kwargs):
        node = DagNode(kwargs.get('name', kind + str(len(self.nodes))), kind,
                       kwargs.get('parent'))
        self.nodes.append(node)
        self.created.append(node)
        return node

    def relatives(self, node, **kwargs):
        result = []
        queue = list(node.children)
        while queue:
            child = queue.pop(0)
            if child.exists:
                result.append(child)
                queue.extend(child.children)
        return result

    def delete(self, node):
        if node in self.fail_delete:
            raise RuntimeError('deletion refused')
        for item in [node] + self.relatives(node):
            item.exists = False
        self.deleted.append(node)

    def rename(self, node, name):
        node.label = name
        return node


class OwnershipTests(unittest.TestCase):
    def setUp(self):
        self.scene = Scene()
        self.scope = definitions('python/DPyTools/scene/ownership.py',
                                 {'uuid': uuid, 'pm': self.scene.pm})
        self.owner = self.scope['SceneNodes']()

    def test_created_nodes_are_retained_and_cleaned_in_reverse_order(self):
        root = self.owner.create('transform', name='root')
        child = self.owner.create('mesh', parent=root)
        self.owner.cleanup()
        self.assertEqual(self.scene.deleted, [child, root])
        self.assertTrue(all(call.kwargs['skipSelect'] for call in self.scene.pm.createNode.call_args_list))

    def test_foreign_descendant_is_not_deleted_with_owned_parent(self):
        root = self.owner.create('transform', name='root')
        borrowed = self.scene.create('joint', parent=root)
        with self.assertRaises(ValueError):
            self.owner.cleanup()
        self.assertTrue(root.exists and borrowed.exists)
        self.scene.pm.delete.assert_not_called()

    def test_cleanup_failure_attempts_other_nodes_and_reports_primary_status(self):
        first = self.owner.create('transform')
        second = self.owner.create('transform')
        self.scene.fail_delete.add(second)
        self.owner.cleanup(active_error=True)
        self.assertFalse(first.exists)
        self.assertTrue(second.exists)
        self.scene.pm.warning.assert_called_once()

    def test_reference_and_instance_cleanup_is_refused(self):
        node = self.owner.create('transform')
        for name in ('referenced', 'instanced'):
            setattr(node, name, True)
            with self.assertRaises(ValueError):
                self.owner.cleanup()
            setattr(node, name, False)
        self.scene.pm.delete.assert_not_called()

    def test_complete_marked_hierarchy_is_admitted(self):
        root = self.owner.create('transform')
        self.owner.create('locator', parent=root)
        self.scope['mark_hierarchy'](root, self.owner.nodes, 'test')
        self.assertIs(self.scope['require_owned_hierarchy'](root, 'test'), root)

    def test_unmarked_foreign_and_protected_hierarchies_are_refused(self):
        root = self.owner.create('transform')
        with self.assertRaises(ValueError):
            self.scope['require_owned_hierarchy'](root, 'test')
        self.scope['mark_hierarchy'](root, self.owner.nodes, 'test')
        with self.assertRaises(ValueError):
            self.scope['require_owned_hierarchy'](root, 'other')
        with self.assertRaises(ValueError):
            self.scope['require_owned_hierarchy'](root, 'test', root)
        owner_attribute = self.scope['OWNER_ATTRIBUTE']
        valid_token = root.values[owner_attribute]
        for token in (None, '', 'invalid-uuid', 123, True, [valid_token]):
            root.values[owner_attribute] = token
            with self.subTest(token=token), self.assertRaises(ValueError):
                self.scope['require_owned_hierarchy'](root, 'test')
        root.values[owner_attribute] = valid_token
        self.scene.create('joint', parent=root)
        with self.assertRaises(ValueError):
            self.scope['require_owned_hierarchy'](root, 'test')


class MeasurementTests(unittest.TestCase):
    def setUp(self):
        self.scene = Scene()
        self.owner_scope = definitions('python/DPyTools/scene/ownership.py',
                                       {'uuid': uuid, 'pm': self.scene.pm})
        self.scope = functions('python/DPyTools/features/measurement.py',
                               {'pm': self.scene.pm, 'sys': sys,
                                'finite_number': finite_number,
                                'bboxMeasureName': 'bboxMeasure_grp',
                                'TOOL_NAME': 'DPyTools.BBoxMeasure.v1',
                                'ownership': SimpleNamespace(**self.owner_scope)})
        self.target = DagNode('target')
        self.scene.nodes.append(self.target)
        self.scene.selection = [self.target]

    def test_same_name_unmarked_group_and_selected_source_are_never_deleted(self):
        node = DagNode(':bboxMeasure_grp')
        self.scene.nodes.append(node)
        self.assertFalse(self.scope['deleteBBoxMeasure']())
        self.assertFalse(self.scope['createBBoxMeasure']())
        self.scene.selection = [node]
        self.assertFalse(self.scope['createBBoxMeasure']())
        self.scene.pm.delete.assert_not_called()
        self.scene.pm.createNode.assert_not_called()

    def test_generated_hierarchy_is_marked_and_can_be_explicitly_deleted(self):
        root = self.scope['createBBoxMeasure']()
        self.assertIsInstance(root, DagNode)
        self.assertEqual(len(self.scene.created), 19)
        self.assertTrue(self.scope['deleteBBoxMeasure']())
        self.assertTrue(self.target.exists)
        self.assertFalse(root.exists)
        self.assertTrue(all(not kwargs.get('force') for _, _, kwargs in self.scene.connections))

    def test_old_marked_result_is_not_removed_until_candidate_is_complete(self):
        old = self.scope['createBBoxMeasure']()
        self.scene.pm.connectAttr.side_effect = RuntimeError('connection failed')
        self.assertFalse(self.scope['createBBoxMeasure']())
        self.assertTrue(old.exists and self.target.exists)

    def test_marked_root_cannot_be_used_as_target_and_replaced(self):
        old = self.scope['createBBoxMeasure']()
        self.scene.selection = [old]
        self.scene.pm.createNode.reset_mock()
        self.assertFalse(self.scope['createBBoxMeasure']())
        self.assertTrue(old.exists)
        self.scene.pm.createNode.assert_not_called()

    def test_foreign_child_blocks_deletion(self):
        root = self.scope['createBBoxMeasure']()
        foreign = self.scene.create('joint', parent=root)
        self.assertFalse(self.scope['deleteBBoxMeasure']())
        self.assertTrue(root.exists and foreign.exists)

    def test_bad_bounds_fail_before_creation(self):
        for bounds in ([0, 1], [0, 0, 0, float('inf'), 1, 1], [2, 0, 0, 1, 1, 1]):
            self.scene.pm.xform.return_value = bounds
            self.assertFalse(self.scope['createBBoxMeasure']())
        self.scene.pm.createNode.assert_not_called()

    def test_bbox_info_retains_order_sizes_and_axis_tie_policy(self):
        self.assertEqual(self.scope['getBBoxInfo'](self.target),
                         [0, 0, 0, 2, 3, 4, 2, 3, 4, 'Z', 4])

    def test_archived_match_scale_entry_points_warn_without_opening_ui(self):
        scope = definitions('python/DPyTools/features/measurement.py', dict(self.scope))
        self.assertFalse(scope['openMatchScaleTool']())
        self.assertFalse(scope['matchScaleTool'].show_dialog())
        scope['matchScaleTool'](title='old signature', parent=None)
        self.assertEqual(self.scene.pm.warning.call_count, 3)


if __name__ == '__main__':
    unittest.main()
