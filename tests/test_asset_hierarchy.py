#!/usr/bin/env python3
"""Actual asset hierarchy queries with controlled, read-only Maya doubles."""

import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_python_rig_safety import functions


SHAPE_TYPES = ('mesh', 'nurbsCurve', 'nurbsSurface', 'camera', 'locator', 'pointLight')


class Node:
    def __init__(self, name, kind='transform', children=()):
        self.label, self.kind, self.children = name, kind, tuple(children)

    def nodeType(self):
        return self.kind

    def numChildren(self):
        return len(self.children)

    def getChildren(self):
        raise AssertionError('Shape admission must use the shared actual-shape query')


class AssetHierarchyTests(unittest.TestCase):
    def setUp(self):
        def descendants(node):
            result = []
            for child in node.children:
                result.extend(descendants(child))
                result.append(child)
            return result

        def relatives(node, **kwargs):
            if kwargs.get('shapes'):
                values = [child for child in node.children if child.kind in SHAPE_TYPES]
            else:
                values = descendants(node)
                if kwargs.get('type'):
                    values = [child for child in values if child.kind == kwargs['type']]
            return values or None

        self.pm = NS(listRelatives=Mock(side_effect=relatives))
        transform_scope = functions('python/DPyTools/scene/transforms.py', {'pm': self.pm})
        self.transforms = NS(hasShape=Mock(side_effect=transform_scope['hasShape']))
        self.scope = functions('python/DPyTools/pipeline/assets/operations.py',
                               {'pm': self.pm, 'scene_transforms': self.transforms})

    def test_shape_facade_reuses_actual_shared_query(self):
        node = Node('mesh', children=(Node('shape', 'mesh'),))
        self.assertTrue(self.scope['isHaveShape'](node))
        self.transforms.hasShape.assert_called_once_with(node)
        self.pm.listRelatives.assert_called_once_with(node, shapes=True)

    def test_joint_child_is_not_a_shape(self):
        group = Node('group', children=(Node('child', 'joint'),))
        self.assertFalse(self.scope['isHaveShape'](group))
        self.assertEqual(self.scope['getAssetTransformFromHierarchy'](group), [group])

    def test_constraint_child_is_not_a_shape(self):
        group = Node('group', children=(Node('constraint', 'parentConstraint'),))
        self.assertFalse(self.scope['isHaveShape'](group))
        self.assertEqual(self.scope['getAssetTransformFromHierarchy'](group), [group])

    def test_transform_filter_includes_empty_groups_not_geometry_or_joints(self):
        empty, joint = Node('empty'), Node('joint', 'joint')
        geometry = Node('geometry', children=(Node('shape', 'mesh'),))
        root = Node('root', children=(empty, geometry, joint))
        self.assertEqual(self.scope['getAssetTransformFromHierarchy'](root), [root, empty])

    def test_geometry_includes_selected_transform_for_each_supported_shape(self):
        for kind in ('mesh', 'nurbsCurve', 'nurbsSurface'):
            with self.subTest(kind=kind):
                node = Node('selected', children=(Node('shape', kind),))
                self.assertEqual(self.scope['getAssetGeometryFromHierarchy'](node), [node])
                self.assertEqual(self.scope['getAssetGeometryFromHierarchy'](node, [kind]), [node])

    def test_geometry_reuses_shape_parent_query(self):
        delegate = Mock(return_value=['retained-result'])
        self.scope['getAssetShapeParentFromHierarchy'] = delegate
        node = Node('selected')
        self.assertEqual(self.scope['getAssetGeometryFromHierarchy'](node, ['mesh']), ['retained-result'])
        delegate.assert_called_once_with(node, shapetypes=['mesh'])

    def test_geometry_retains_descendant_order_and_appends_matching_root_once(self):
        child = Node('child', children=(Node('childShape', 'mesh'),))
        root = Node('root', children=(child, Node('rootShape', 'mesh')))
        self.assertEqual(self.scope['getAssetGeometryFromHierarchy'](root, ['mesh']), [child, root])

    def test_multiple_matching_shapes_do_not_duplicate_their_transform(self):
        node = Node('selected', children=(Node('a', 'mesh'), Node('b', 'mesh')))
        self.assertEqual(self.scope['getAssetGeometryFromHierarchy'](node), [node])

    def test_shape_filter_is_specific_and_does_not_classify_nonshape_children(self):
        node = Node('selected', children=(Node('joint', 'joint'), Node('shape', 'nurbsCurve')))
        self.assertFalse(self.scope['checkShapeType'](node, ['joint']))
        self.assertFalse(self.scope['checkShapeType'](node, ['mesh']))
        self.assertTrue(self.scope['checkShapeType'](node, ['nurbsCurve']))
        self.assertFalse(self.scope['checkShapeType'](node))

    def test_camera_and_geometry_share_selected_root_admission(self):
        node = Node('camera', children=(Node('shape', 'camera'),))
        self.assertEqual(self.scope['getAssetShapeParentFromHierarchy'](node, ['camera']), [node])
        self.assertEqual(self.scope['getAssetGeometryFromHierarchy'](node), [])

    def test_none_inputs_are_empty_without_native_queries(self):
        for name in ('getAssetTypeNodeFromHierarchy', 'getAssetTransformNoShapeFromHierarchy',
                     'getAssetTransformFromHierarchy', 'getAssetGeometryFromHierarchy',
                     'getAssetShapeParentFromHierarchy'):
            self.assertEqual(self.scope[name](None), [])
        self.assertFalse(self.scope['isHaveShape'](None))
        self.assertFalse(self.scope['checkShapeType'](None, ['mesh']))
        self.pm.listRelatives.assert_not_called()

    def test_empty_native_descendants_normalize_without_losing_selected_root(self):
        node = Node('leaf')
        self.assertEqual(self.scope['getAssetTypeNodeFromHierarchy'](node), [node])
        self.assertEqual(self.scope['getAssetTypeNodeFromHierarchy'](node, 'transform'), [node])
        self.assertEqual(self.scope['getAssetTypeNodeFromHierarchy'](node, 'joint'), [])
        self.assertEqual(self.scope['getAssetTransformNoShapeFromHierarchy'](node), [node])
        self.assertEqual(self.scope['getAssetTransformFromHierarchy'](node), [node])
        self.assertEqual(self.scope['getAssetGeometryFromHierarchy'](node), [])

    def test_root_and_descendants_type_filter_retains_legacy_order(self):
        first, second = Node('first', 'joint'), Node('second', 'joint')
        root = Node('root', 'joint', (first, Node('empty'), second))
        self.assertEqual(self.scope['getAssetTypeNodeFromHierarchy'](root, 'joint'), [root, first, second])

    def test_returned_native_lists_are_not_mutated_by_root_insertion(self):
        child, root = Node('child'), Node('root')
        values = [child]
        self.pm.listRelatives.side_effect = None
        self.pm.listRelatives.return_value = values
        for name in ('getAssetTypeNodeFromHierarchy', 'getAssetTransformNoShapeFromHierarchy'):
            self.assertEqual(self.scope[name](root), [root, child])
            self.assertEqual(values, [child])


if __name__ == '__main__':
    unittest.main()
