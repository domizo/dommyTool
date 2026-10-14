#!/usr/bin/env python3
"""Real controller/eye/emitter builders with native-result and failure doubles."""

import numbers
import sys
import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_attribute_batches import Node
from test_transform_helpers import TransformGraph
from test_python_rig_safety import functions
from test_python_table_selection import _callback


class BuilderGraph(TransformGraph):
    def __init__(self):
        super().__init__()
        self.selected = []
        self.pm.ls = self.ls
        self.pm.select = Mock(side_effect=self.select)
        self.pm.polyMergeVertex = Mock(side_effect=lambda shape, **kw: [self.create('polyMergeVert')])
        self.pm.polySoftEdge = Mock(side_effect=lambda shape, **kw: [self.create('polySoftEdge')])
        self.pm.sets = Mock()
        self.pm.particle = Mock(side_effect=lambda **kw: self.particle('particle'))
        self.pm.nParticle = Mock(side_effect=lambda **kw: self.particle('nParticle'))
        self.pm.connectDynamic = Mock()
        self.pm.addDynamic = Mock()
        self.selection = NS(**functions('python/DPyTools/scene/selection.py', {'pm': self.pm}))
        self.eye = NS(**functions('python/DPyTools/features/rigging/human_eye.py', {'pm': self.pm, 'sys': sys, 'ownership': self.ownership}))
        self.emitter = NS(**functions('python/DPyTools/features/particles.py', {
            'pm': self.pm, 'sys': sys, 'ownership': self.ownership,
            'parameters': self.parameters, 'selection': self.selection}))
        self.controller = NS(**functions('python/DPyTools/create/rig_controller.py', {
            'pm': self.pm, 'sys': sys, 'numbers': numbers,
            'ownership': self.ownership, 'parameters': self.parameters}))
    def create(self, kind, **kwargs):
        node = super().create(kind, **kwargs)
        if kind == 'DRigController':
            node.add_port('outputCurve', 'nurbsCurve')
            node.add_port('localScale', 'double', 1)
            division = node.add_port('division', 'long', 8)
            division.minimum, division.maximum = 3, 10000
            shape = node.add_port('shapeType', 'enum', 0)
            shape.enums = dict((str(index), index) for index in range(9))
        if kind == 'nurbsCurve':
            node.add_port('create', 'nurbsCurve')
            node.add_port('overrideEnabled', 'bool', False)
            node.add_port('overrideColor', 'long', 0)
        if kind == 'mesh': node.add_port('inMesh', 'mesh')
        if kind == 'DHEyeGen':
            node.add_port('outLenzMesh', 'mesh')
            node.add_port('outIrisMesh', 'mesh')
        if kind == 'DPEmitter': node.add_port('rate', 'double', 0)
        return node
    def ls(self, **kwargs):
        if kwargs.get('selection'): return list(self.selected)
        kinds = kwargs['type']
        kinds = (kinds,) if isinstance(kinds, str) else kinds
        return [node for node in self.nodes if node.exists and node.kind in kinds]
    def select(self, values=None, **kwargs):
        self.selected = [] if kwargs.get('clear') else (list(values) if isinstance(values, (list, tuple)) else [values])
    def particle(self, kind):
        transform = self.create('transform')
        shape = self.create(kind, parent=transform)
        if kind == 'nParticle': self.create('nucleus')
        self.selected = [transform]
        return [transform, shape]


class ControllerBuilderTests(unittest.TestCase):
    def setUp(self): self.graph = BuilderGraph()
    def test_success_returns_explicit_parent_shape_and_generator(self):
        transform, shape, generator = self.graph.controller.createRigCtrl({'division': 8, 'localScale': 3}, 2)
        self.assertIs(shape.parent, transform)
        self.assertEqual(generator.attr('localScale').value, 3)
        self.assertEqual(shape.attr('overrideColor').value, 6)
        self.assertEqual(self.graph.selected, [transform])
        self.assertTrue(all(call.kwargs['skipSelect'] for call in self.graph.pm.createNode.call_args_list))
        self.assertTrue(all(not options.get('force', False) for _, _, options in self.graph.connections))
    def test_bad_later_value_cleans_generator_before_output_allocation(self):
        for values in ({'division': 2}, {'shapeType': 9}, {'localScale': float('nan')}, {'gain': 1, 'missing': 2}):
            self.setUp()
            with self.subTest(values=values), self.assertRaises(ValueError): self.graph.controller.createRigCtrl(values)
            self.assertEqual([node.kind for node in self.graph.created], ['DRigController'])
            self.assertFalse(self.graph.created[0].exists)
            self.graph.pm.setAttr.assert_not_called()
    def test_connection_failure_cleans_complete_known_graph(self):
        self.graph.pm.connectAttr.side_effect = RuntimeError('failed')
        with self.assertRaises(RuntimeError): self.graph.controller.createRigCtrl()
        self.assertTrue(all(not node.exists for node in self.graph.created))
        self.graph.pm.select.assert_not_called()
    def test_side_and_mapping_admission_precede_allocation(self):
        for side in (True, -1, 3, 1.5):
            with self.assertRaises(ValueError): self.graph.controller.createRigCtrl(side=side)
        with self.assertRaises(ValueError): self.graph.controller.createRigCtrl(attributes=[])
        self.graph.pm.createNode.assert_not_called()
    def test_explicit_base_name_collision_preserves_existing_node(self):
        borrowed = self.graph.create('transform', name='occupied')
        self.graph.pm.createNode.reset_mock()
        with self.assertRaises(ValueError): self.graph.controller.createRigCtrlBase('occupied')
        self.assertTrue(borrowed.exists)
        self.graph.pm.createNode.assert_not_called()
    def test_selection_failure_does_not_delete_completed_controller(self):
        self.graph.pm.select.side_effect = RuntimeError('selection failed')
        result = self.graph.controller.createRigCtrl()
        self.assertTrue(all(node.exists for node in result))
        self.graph.pm.delete.assert_not_called()
        self.graph.pm.warning.assert_called_once()
    def test_interrupt_during_graph_dispatch_cleans_and_propagates(self):
        self.graph.pm.connectAttr.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt): self.graph.controller.createRigCtrl()
        self.assertTrue(all(not node.exists for node in self.graph.created))
    def test_ui_callback_reports_failure_and_returns_false(self):
        ui = NS(rigTypeMenu='type', scaleSlider=NS(getValue=lambda: 1), divSlider=NS(getValue=lambda: 8),
                ratioSlider=NS(getValue=lambda: 1), keepCheck='keep', dirTypeMenu='axis', taperAmtSlider=NS(getValue=lambda: 0),
                taperAxisTypeMenu='taper', bendAmtSlider=NS(getValue=lambda: 0), bendAxisTypeMenu='bend',
                arrowRevCheck='reverse', arrowRatioSlider=NS(getValue=lambda: .75), arrowWidthSlider=NS(getValue=lambda: .25), sideRadio='side')
        pm = NS(optionMenuGrp=lambda *a, **kw: 1, checkBoxGrp=lambda *a, **kw: False, radioButtonGrp=lambda *a, **kw: 2, warning=Mock())
        operation = Mock(side_effect=ValueError('invalid'))
        callback = _callback('python/DPyTools/create/rig_controller.py', 'openWin', 'createController', {'pm': pm, 'createRigCtrl': operation})
        self.assertFalse(callback(ui))
        pm.warning.assert_called_once()


class EyeBuilderTests(unittest.TestCase):
    def setUp(self): self.graph = BuilderGraph()
    def test_success_owns_explicit_mesh_parents_and_confirmed_history(self):
        group, generator, meshes, history = self.graph.eye.createHumanEye()
        self.assertEqual([node.kind for node in history], ['polyMergeVert', 'polySoftEdge', 'polySoftEdge'])
        self.assertTrue(all(mesh.parent.parent is group for mesh in meshes))
        self.assertEqual(len(self.graph.connections), 2)
        self.assertTrue(all(not options.get('force', False) for _, _, options in self.graph.connections))
        self.graph.pm.select.assert_not_called()
    def test_late_shading_failure_cleans_every_confirmed_node(self):
        self.graph.pm.sets.side_effect = RuntimeError('shading failed')
        self.assertFalse(self.graph.eye.createHumanEye())
        self.assertTrue(all(not node.exists for node in self.graph.created))
    def test_borrowed_history_return_is_never_adopted_or_deleted(self):
        borrowed = self.graph.create('polyMergeVert')
        self.graph.pm.polyMergeVertex.return_value = [borrowed]
        self.graph.pm.polyMergeVertex.side_effect = None
        self.assertFalse(self.graph.eye.createHumanEye())
        self.assertTrue(borrowed.exists)
        self.assertNotIn(borrowed, self.graph.deleted)
    def test_unconfirmed_or_wrong_history_result_fails_without_claiming_success(self):
        for result in (None, [], ['missing'], [Node('wrong', 'utility')]):
            self.setUp()
            self.graph.pm.polyMergeVertex.side_effect = None
            self.graph.pm.polyMergeVertex.return_value = result
            with self.subTest(result=result): self.assertFalse(self.graph.eye.createHumanEye())
            self.graph.pm.sets.assert_not_called()
    def test_unknown_native_child_is_not_deleted_by_cleanup(self):
        unknown = []
        def native(shape, **kwargs):
            unknown.append(self.graph.create('mesh', parent=shape.parent))
            raise RuntimeError('changed then failed')
        self.graph.pm.polyMergeVertex.side_effect = native
        self.assertFalse(self.graph.eye.createHumanEye())
        self.assertTrue(unknown[0].exists)
        self.assertTrue(unknown[0].parent.exists)
        self.assertGreaterEqual(self.graph.pm.warning.call_count, 2)
    def test_interrupt_in_native_history_preserves_primary_and_cleans_known_nodes(self):
        self.graph.pm.polySoftEdge.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt): self.graph.eye.createHumanEye()
        self.assertTrue(all(not node.exists for node in self.graph.created))


class ParticleBuilderTests(unittest.TestCase):
    def setUp(self):
        self.graph = BuilderGraph()
        self.borrowed = self.graph.create('transform', name='borrowed')
        self.graph.selected = [self.borrowed]
    def test_both_modes_return_handles_and_restore_previous_selection(self):
        for mode, kind in ((False, 'particle'), (True, 'nParticle')):
            self.setUp()
            emitter, transform, shape = self.graph.emitter.createDPEmitter(mode)
            self.assertEqual(shape.kind, kind)
            self.assertIs(shape.parent, transform)
            self.assertEqual(emitter.attr('rate').value, 1)
            self.assertEqual(self.graph.selected, [self.borrowed])
            command = self.graph.pm.nParticle if mode else self.graph.pm.particle
            command.assert_called_once_with()
    def test_return_order_is_classified_by_type_not_index(self):
        original = self.graph.pm.particle.side_effect
        self.graph.pm.particle.side_effect = lambda **kw: list(reversed(original(**kw)))
        self.assertEqual(self.graph.emitter.createDPEmitter()[2].kind, 'particle')
    def test_native_connection_failure_cleans_new_particles_but_keeps_nucleus(self):
        self.graph.pm.addDynamic.side_effect = RuntimeError('failed')
        self.assertFalse(self.graph.emitter.createDPEmitter(True))
        self.assertTrue(self.borrowed.exists)
        self.assertTrue(all(node.exists for node in self.graph.created if node.kind == 'nucleus'))
        self.assertTrue(all(not node.exists for node in self.graph.created if node is not self.borrowed and node.kind != 'nucleus'))
        self.assertEqual(self.graph.selected, [self.borrowed])
    def test_preexisting_returned_nodes_are_not_adopted(self):
        borrowed_shape = self.graph.create('particle', parent=self.borrowed)
        self.graph.pm.particle.side_effect = None
        self.graph.pm.particle.return_value = [self.borrowed, borrowed_shape]
        self.assertFalse(self.graph.emitter.createDPEmitter())
        self.assertTrue(self.borrowed.exists and borrowed_shape.exists)
        self.graph.pm.connectDynamic.assert_not_called()
    def test_result_count_type_duplicate_and_hierarchy_are_checked(self):
        for result in (None, [], [self.borrowed, self.borrowed], [Node('bad', 'utility')]):
            self.setUp()
            self.graph.pm.particle.side_effect = None
            self.graph.pm.particle.return_value = result
            with self.subTest(result=result): self.assertFalse(self.graph.emitter.createDPEmitter())
            self.graph.pm.connectDynamic.assert_not_called()
        self.setUp()
        def wrong_parent(**kwargs):
            transform = self.graph.create('transform')
            return [transform, self.graph.create('particle', parent=self.borrowed)]
        self.graph.pm.particle.side_effect = wrong_parent
        self.assertFalse(self.graph.emitter.createDPEmitter())
        self.assertTrue(self.borrowed.exists)
    def test_invalid_mode_precedes_allocation(self):
        self.graph.pm.createNode.reset_mock()
        with self.assertRaises(TypeError): self.graph.emitter.createDPEmitter(1)
        self.graph.pm.createNode.assert_not_called()
    def test_interrupt_restores_selection_and_cleans_known_emitter(self):
        self.graph.pm.particle.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt): self.graph.emitter.createDPEmitter()
        self.assertFalse(self.graph.created[-1].exists)
        self.assertEqual(self.graph.selected, [self.borrowed])
    def test_shared_selection_restoration_failure_does_not_mask_operation_error(self):
        self.graph.pm.select.side_effect = RuntimeError('selection refused')
        self.graph.pm.particle.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt): self.graph.emitter.createDPEmitter()
        self.graph.pm.warning.assert_called_once()
    def test_empty_and_deleted_selection_items_are_restored_explicitly(self):
        self.borrowed.exists = False
        self.assertTrue(self.graph.selection.restore_selection([self.borrowed]))
        self.graph.pm.select.assert_called_once_with(clear=True)
        self.assertEqual(self.graph.selected, [])


if __name__ == '__main__':
    unittest.main()
