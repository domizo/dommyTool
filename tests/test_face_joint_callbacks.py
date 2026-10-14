#!/usr/bin/env python3
"""Mirror orchestration and actual Face Joint callbacks without Maya."""

import unittest
from types import SimpleNamespace
from unittest.mock import Mock

from test_python_rig_safety import functions
from test_python_table_selection import _callback
from DPyTools.core import strings
from DPyTools.core.geometry import position3
from DPyTools.core.numeric import finite_number


class Node:
    def __init__(self, name, x=1, children=()):
        self._name, self.x = name, x
        self.children = list(children)
        self.parent = None
        for child in children:
            child.parent = self
    def name(self):
        return self._name
    def nodeType(self):
        return 'joint'
    def isInstanced(self):
        return False


class MirrorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scope = functions('python/DPyTools/features/rigging/mirror.py',
                              {'strings': strings, 'position3': position3})

    def setUp(self):
        self.root = Node('|group_l|char_l:jaw_l', children=[Node('char_l:upper_lip')])
        self.created = []
        self.existing = set()
        self.warnings = []

        def relatives(node, **kwargs):
            return node.children if kwargs.get('children') else (
                [node.parent] if node.parent else None)

        def mirror(root, **kwargs):
            queue = [(root, None)]
            created = []
            while queue:
                source, parent = queue.pop(0)
                node = Node('copy_' + strings.leaf_name(source.name()), -source.x)
                node.parent = parent
                if parent:
                    parent.children.append(node)
                created.append(node)
                queue.extend((child, node) for child in source.children)
            self.created.extend(created)
            return list(reversed(created))

        def rename(node, name):
            node._name = name.lstrip(':')
            self.existing.add(name)
            return node

        self.pm = SimpleNamespace(PyNode=lambda node: node, listRelatives=relatives,
                                  xform=Mock(side_effect=lambda node, **kw: [node.x, 0, 0]),
                                  objExists=Mock(side_effect=lambda name: name in self.existing),
                                  mirrorJoint=Mock(side_effect=mirror),
                                  rename=Mock(side_effect=rename))
        self.scope['pm'] = self.pm

    def test_checkbox_is_honored_without_mirrored_renaming(self):
        for enabled in (False, True):
            self.assertEqual(self.scope['mirror_joints'](
                [self.root], mirror_behavior=enabled, rename_mirrored=False), 1)
            self.pm.mirrorJoint.assert_called_with(
                self.root, mirrorYZ=True, mirrorBehavior=enabled)
        self.pm.rename.assert_not_called()

    def test_tree_matching_does_not_assume_native_return_order(self):
        self.assertEqual(self.scope['mirror_joints']([self.root]), 1)
        self.assertEqual([node.name() for node in self.created],
                         ['char_l:jaw_r', 'char_l:upper_lip_r'])
        self.assertTrue(all('searchReplace' not in call.kwargs
                            for call in self.pm.mirrorJoint.call_args_list))
        self.assertEqual(self.root.name(), '|group_l|char_l:jaw_l')

    def test_source_without_side_is_renamed_once_when_requested(self):
        self.root._name = 'jaw'
        self.scope['mirror_joints']([self.root], rename_mirrored=False)
        self.pm.rename.assert_called_once_with(self.root, ':jaw_l')

    def test_disabled_source_rename_keeps_original_name(self):
        self.root._name = 'jaw'
        self.scope['mirror_joints']([self.root], rename_sources=False)
        self.assertEqual(self.root.name(), 'jaw')

    def test_center_is_noop_and_does_not_dispatch_native_mirror(self):
        self.root.x = 0
        self.assertEqual(self.scope['mirror_joints']([self.root]), 0)
        self.pm.mirrorJoint.assert_not_called()

    def test_overlapping_selection_is_rejected_before_any_mirror(self):
        with self.assertRaises(ValueError):
            self.scope['mirror_joints']([self.root, self.root.children[0]])
        self.pm.mirrorJoint.assert_not_called()

    def test_later_ambiguous_name_or_collision_cannot_mirror_valid_prefix(self):
        for bad in (Node('bad_l_r'), Node('other_l')):
            self.existing = {':other_r'}
            with self.assertRaises(ValueError):
                self.scope['mirror_joints']([self.root, bad])
        self.pm.mirrorJoint.assert_not_called()

    def test_nonfinite_root_position_fails_before_native_mutation(self):
        self.root.x = float('nan')
        with self.assertRaises(ValueError):
            self.scope['mirror_joints']([self.root])
        self.pm.mirrorJoint.assert_not_called()

    def test_incomplete_native_result_reports_partial_state_and_never_renames(self):
        self.pm.mirrorJoint.side_effect = lambda *a, **kw: [Node('only_root')]
        with self.assertRaisesRegex(RuntimeError, '0 completed roots'):
            self.scope['mirror_joints']([self.root])
        self.pm.rename.assert_not_called()

    def test_interruption_is_not_masked_as_validation_error(self):
        self.pm.mirrorJoint.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            self.scope['mirror_joints']([self.root])


class FaceCallbackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scope = {'position3': position3, 'finite_number': finite_number}
        path = 'python/DPyTools/rigging/DFaceJointTool.py'
        cls.set_mesh = staticmethod(_callback(path, 'DFaceJointTool', 'setFitMeshCB', cls.scope))
        cls.mirror = staticmethod(_callback(path, 'DFaceJointTool', 'runMirrorJntCB', cls.scope))
        cls.rivet = staticmethod(_callback(path, 'DFaceJointTool', 'runCreateRivetCB', cls.scope))

    def setUp(self):
        self.mesh = SimpleNamespace(longName=lambda: '|root|mesh|shape')
        self.selection = [self.mesh]
        self.pm = SimpleNamespace(ls=lambda **kw: self.selection, warning=Mock(),
                                  textField=Mock(return_value='mesh'),
                                  checkBox=Mock(return_value=False),
                                  rename=Mock(side_effect=lambda node, name: node),
                                  parent=Mock(), createNode=Mock(return_value='joint'),
                                  xform=Mock())
        self.create_rivets = Mock(return_value=('follicle_group', 'joint_group'))
        self.vertices = Mock(return_value=([1, 2], [(101, 2, 3), (102, 2, 3)]))
        self.uvs = Mock(return_value=[[0.1, 0.5], [0.2, 0.5]])
        self.mirror_op = Mock(return_value=1)
        self.resolve = Mock(return_value=self.mesh)
        self.scope.update(pm=self.pm, resolve_shape=self.resolve,
                          selOp=SimpleNamespace(getJointlist=lambda: ['joint']),
                          mirror_joints=self.mirror_op,
                          meshOp=SimpleNamespace(getSelectedMeshIds=self.vertices),
                          uvOp=SimpleNamespace(getUVfromPoint=self.uvs),
                          rivet_tools=SimpleNamespace(createVertexRivets=self.create_rivets, MAX_RIVETS=10000))
        self.ui = SimpleNamespace(fitMeshNameText='field', mirrorOrientCheck='orient',
                                  mirroredRenameCheck='mirrored', mirrorSrcRenameCheck='source',
                                  rivetRotLink='rotation')

    def test_bad_mesh_selection_warns_and_never_updates_field(self):
        for selection in ([], [self.mesh, self.mesh]):
            self.selection = selection
            self.assertFalse(self.set_mesh(self.ui))
        self.selection = [self.mesh]
        self.resolve.side_effect = ValueError('not a mesh')
        self.assertFalse(self.set_mesh(self.ui))
        self.pm.textField.assert_not_called()

    def test_valid_mesh_field_uses_full_shape_path(self):
        self.assertTrue(self.set_mesh(self.ui))
        self.pm.textField.assert_called_once_with('field', e=1, text='|root|mesh|shape')

    def test_mirror_callback_forwards_every_option_and_failure_status(self):
        self.assertTrue(self.mirror(self.ui))
        self.mirror_op.assert_called_once_with(['joint'], mirror_behavior=False,
                                               rename_mirrored=False, rename_sources=False)
        self.mirror_op.side_effect = ValueError('collision')
        self.assertFalse(self.mirror(self.ui))

    def test_rivet_preflights_all_world_uvs_before_creating_groups(self):
        self.assertTrue(self.rivet(self.ui))
        self.vertices.assert_called_once_with(target_mesh=self.mesh)
        self.uvs.assert_called_once_with(meshObj=self.mesh,
                                       pt_list=[(101, 2, 3), (102, 2, 3)])
        self.create_rivets.assert_called_once_with(self.mesh,
            [(101.0, 2.0, 3.0), (102.0, 2.0, 3.0)], [(0.1, 0.5), (0.2, 0.5)], False)
        self.pm.createNode.assert_not_called()

    def test_later_bad_uv_or_native_query_failure_cannot_create_valid_prefix(self):
        for result in ([[0.1, 0.5]], [[0.1, 0.5], [float('nan'), 0.5]],
                       [[0.1, 0.5], [0.5]]):
            self.uvs.return_value = result
            self.assertFalse(self.rivet(self.ui))
        self.uvs.side_effect = RuntimeError('UV query failed')
        self.assertFalse(self.rivet(self.ui))
        self.create_rivets.assert_not_called()

    def test_mixed_mesh_selection_failure_cannot_create_rivets(self):
        self.vertices.side_effect = ValueError('wrong target mesh')
        self.assertFalse(self.rivet(self.ui))
        self.create_rivets.assert_not_called()

    def test_builder_false_does_not_claim_success(self):
        self.create_rivets.return_value = False
        self.assertFalse(self.rivet(self.ui))
        self.pm.createNode.assert_not_called()

    def test_rotation_link_and_world_driven_offsets(self):
        self.pm.checkBox.return_value = True
        self.assertTrue(self.rivet(self.ui))
        self.assertIs(self.create_rivets.call_args.args[-1], True)

    def test_over_limit_vertices_stop_before_uv_queries_or_creation(self):
        self.vertices.return_value = (list(range(10001)), [(0, 0, 0)] * 10001)
        self.assertFalse(self.rivet(self.ui))
        self.uvs.assert_not_called()
        self.create_rivets.assert_not_called()


if __name__ == '__main__':
    unittest.main()
