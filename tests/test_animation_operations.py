#!/usr/bin/env python3
"""Actual animation adapters with explicit-coordinate and failure doubles."""

import numbers
import sys
import unittest
import io
import contextlib
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_attribute_batches import Graph, Port
from test_python_rig_safety import functions
from DPyTools.features.animation import key_plan
from DPyTools.scene import attribute_values
from DPyTools.core import strings
from DPyTools.core.numeric import finite_number


class AnimationGraph(Graph):
    def __init__(self):
        super().__init__()
        self.driver = self.create('utility', name='driver')
        self.target = self.create('utility', name='target')
        self.driver.attr('gain').value, self.target.attr('gain').value = 5.0, 9.0
        self.selected = [self.driver]
        self.pm.general = NS(Attribute=Port)
        self.pm.ls = lambda **kw: list(self.selected)
        self.pm.select = Mock(side_effect=self.select)
        self.pm.setDrivenKeyframe = Mock(return_value=1)
        self.pm.attributeQuery = self.query
        self.animatable = {self.driver: None, self.target: [self.target.attr('gain')]}
        self.keys, self.port_curves = {}, {}
        self.pm.listAnimatable = lambda node: self.animatable.get(node)
        self.pm.keyframe = Mock(side_effect=self.keyframe)
        self.pm.cutKey = Mock(return_value=1)
        self.selection = NS(**functions('python/DPyTools/scene/selection.py', {'pm': self.pm}))
        self.operations = NS(**functions('python/DPyTools/features/animation/operations.py', {
            'pm': self.pm, 'numbers': numbers, 'key_plan': key_plan,
            'attribute_values': attribute_values, 'parameters': self.parameters,
            'selection': self.selection, 'sys': sys, 'strings': strings,
            'finite_number': finite_number}))
    def select(self, values=None, **kwargs):
        self.selected = [] if kwargs.get('clear') else list(values)
    def query(self, name, **kwargs):
        port = next(port for port in kwargs['node'].ports.values() if port.lastPlugAttr() == name)
        return all(getattr(port, field) for field in ('writable', 'readable', 'connectable') if kwargs.get(field))
    def driven(self, times=(0, 1), values=(20, 30)):
        return self.operations.createDrivenKeys(self.driver.attr('gain'), self.target.attr('gain'), times, values)
    def animate(self, port=None, kind='animCurveTU', keys=((0, 1), (1, 2))):
        port = self.target.attr('gain') if port is None else port
        curve = self.create(kind)
        self.keys[curve] = keys
        self.port_curves.setdefault(port, []).append(curve)
        return curve
    def keyframe(self, value, **kwargs):
        curves = self.port_curves.get(value, []) if isinstance(value, Port) else [value]
        pairs = [pair for curve in curves for pair in self.keys.get(curve, ())]
        if kwargs.get('keyframeCount'): return len(pairs)
        if kwargs.get('name'): return curves
        if kwargs.get('valueChange'): return [pair[1] for pair in pairs]
        return [pair[0] for pair in pairs]


class KeyPlanTests(unittest.TestCase):
    def test_descending_inputs_are_sorted_with_their_values(self):
        self.assertEqual(key_plan.key_pairs([1, -1, 0], [30, 10, 20]), ((-1.0, 10.0), (0.0, 20.0), (1.0, 30.0)))
    def test_invalid_lengths_types_and_limits_are_rejected(self):
        for times, values in ((None, None), ([], []), ([0], []), ('01', [1, 2]), (range(2), [1, 2]), ([0] * 10001, [0] * 10001)):
            with self.subTest(times=type(times).__name__), self.assertRaises(ValueError): key_plan.key_pairs(times, values)
    def test_nonfinite_boolean_string_and_unrepresentable_values_fail(self):
        for value in (True, '1', float('inf'), float('nan'), 10 ** 400):
            for times, values in (([0, value], [1, 2]), ([0, 1], [1, value])):
                with self.subTest(value=value), self.assertRaises((TypeError, ValueError)): key_plan.key_pairs(times, values)
    def test_duplicate_normalized_inputs_fail(self):
        for times in ((1, 1.0), (-0.0, 0.0)):
            with self.assertRaises(ValueError): key_plan.key_pairs(times, [1, 2])
    def test_the_admitted_snapshot_does_not_follow_later_list_changes(self):
        times, values = [0, 1], [2, 3]
        result = key_plan.key_pairs(times, values)
        times[1], values[1] = 10, 30
        self.assertEqual(result, ((0.0, 2.0), (1.0, 3.0)))


class DrivenKeyTests(unittest.TestCase):
    def setUp(self): self.graph = AnimationGraph()
    def test_explicit_coordinates_never_write_temporary_values(self):
        self.assertEqual(self.graph.driven(), 2)
        self.graph.pm.setAttr.assert_not_called()
        self.assertEqual((self.graph.driver.attr('gain').value, self.graph.target.attr('gain').value), (5, 9))
        for index, call in enumerate(self.graph.pm.setDrivenKeyframe.call_args_list):
            self.assertIs(call.args[0], self.graph.target.attr('gain'))
            self.assertEqual(call.kwargs['driverValue'], index)
            self.assertEqual(call.kwargs['value'], 20 + index * 10)
            self.assertIs(call.kwargs['currentDriver'], self.graph.driver.attr('gain'))
            self.assertFalse(call.kwargs['insertBlend'])
            self.assertEqual(call.kwargs['hierarchy'], 'none')
            self.assertFalse(call.kwargs['shape'])
    def test_native_failure_reports_partial_keys_without_temporary_state(self):
        self.graph.pm.setDrivenKeyframe.side_effect = [1, RuntimeError('second failed')]
        with self.assertRaisesRegex(RuntimeError, 'after 1 confirmed keys'): self.graph.driven()
        self.graph.pm.setAttr.assert_not_called()
        self.assertEqual(self.graph.selected, [self.graph.driver])
    def test_late_bad_pair_cannot_dispatch_a_valid_prefix(self):
        for times, values in (((0, float('nan')), (20, 30)), ((0, 1), (20, float('inf'))), ((0, 0), (20, 30))):
            with self.assertRaises((TypeError, ValueError)): self.graph.driven(times, values)
        self.graph.pm.setDrivenKeyframe.assert_not_called()
    def test_complete_driver_and_target_range_admission_precedes_keys(self):
        self.graph.target.attr('gain').maximum = 25
        with self.assertRaises(ValueError): self.graph.driven()
        self.graph.target.attr('gain').maximum = None
        self.graph.driver.attr('gain').minimum = 0
        with self.assertRaises(ValueError): self.graph.driven((-1, 0), (20, 30))
        self.graph.pm.setDrivenKeyframe.assert_not_called()
    def test_locked_referenced_instanced_and_parent_locked_targets_fail(self):
        for state in ('lock', 'reference', 'instance', 'parent'):
            self.setUp()
            if state == 'lock': self.graph.target.locked = True
            elif state == 'reference': self.graph.target.referenced = True
            elif state == 'instance':
                self.graph.target.__class__ = self.graph.pm.nt.DagNode
                self.graph.target.instanced = True
            else:
                parent = self.graph.target.add_port('parent')
                parent.locked = True
                self.graph.target.attr('gain').parents = [parent]
            with self.subTest(state=state), self.assertRaises(ValueError): self.graph.driven()
            self.graph.pm.setDrivenKeyframe.assert_not_called()
    def test_readonly_animated_driver_and_existing_target_input_are_not_forced(self):
        self.graph.driver.referenced = self.graph.driver.locked = True
        self.graph.driver.attr('gain').locked = True
        old = Port(self.graph.driver, 'old')
        self.graph.target.attr('gain').incoming = [old]
        self.assertEqual(self.graph.driven(), 2)
        self.assertEqual(self.graph.target.attr('gain').incoming, [old])
        self.graph.pm.connectAttr.assert_not_called()
        self.graph.pm.setAttr.assert_not_called()
    def test_self_driver_wrong_type_compound_whole_array_and_missing_ports_fail(self):
        target = self.graph.target.attr('gain')
        for value in (self.graph.driver, 'missing.gain', self.graph.driver.attr('gain')):
            with self.assertRaises(ValueError): self.graph.operations.createDrivenKeys(value, value, [0], [1])
        for kind in ('string', 'double3'):
            target.kind = kind
            with self.assertRaises(ValueError): self.graph.driven()
        target.kind, target.array = 'double', True
        with self.assertRaises(ValueError): self.graph.driven()
        self.graph.pm.setDrivenKeyframe.assert_not_called()
    def test_native_zero_or_unexpected_count_is_not_success(self):
        for result in (0, None, True, 2, 1.0):
            self.graph.pm.setDrivenKeyframe.return_value = result
            with self.subTest(result=result), self.assertRaisesRegex(RuntimeError, 'after 0 confirmed keys'): self.graph.driven()
    def test_interruption_preserves_selection_and_propagates(self):
        def interrupt(*args, **kwargs):
            self.graph.selected = [self.graph.target]
            raise KeyboardInterrupt()
        self.graph.pm.setDrivenKeyframe.side_effect = interrupt
        with self.assertRaises(KeyboardInterrupt): self.graph.driven()
        self.assertEqual(self.graph.selected, [self.graph.driver])
        self.graph.pm.setAttr.assert_not_called()
    def test_historical_face_module_imports_the_single_implementation(self):
        from test_python_table_selection import _tree
        import ast
        tree = _tree('python/DPyTools/rigging/DFaceRigOp.py')
        self.assertFalse(any(isinstance(node, ast.FunctionDef) and node.name == 'createDrivenKeys' for node in tree.body))
        self.assertTrue(any(isinstance(node, ast.ImportFrom) and node.module == 'DPyTools.features.animation.operations' and any(alias.name == 'createDrivenKeys' for alias in node.names) for node in tree.body))
    def test_historical_literal_face_key_batches_meet_the_shared_contract(self):
        import ast
        from test_python_table_selection import _tree
        checked = 0
        for node in ast.walk(_tree('python/DPyTools/rigging/DFaceRigOp.py')):
            if not isinstance(node, ast.Call) or not isinstance(node.func, ast.Name) or node.func.id != 'createDrivenKeys': continue
            fields = {item.arg: item.value for item in node.keywords}
            try: times, values = ast.literal_eval(fields['times']), ast.literal_eval(fields['values'])
            except (ValueError, KeyError): continue
            key_plan.key_pairs(times, values)
            checked += 1
        self.assertEqual(checked, 169)


class AnimationUtilityTests(unittest.TestCase):
    def setUp(self): self.graph = AnimationGraph()
    def test_none_or_empty_animatable_lists_return_false_and_noop_deletion(self):
        self.assertIs(self.graph.operations.isAnimationExist([self.graph.driver]), False)
        self.assertIs(self.graph.operations.isAnimationExist(), False)
        self.assertIs(self.graph.operations.isAnimationAttribute(), False)
        self.assertEqual(self.graph.operations.deleteAnimationKeys([self.graph.driver]), 0)
        self.graph.pm.cutKey.assert_not_called()
    def test_inspection_is_boolean_and_readonly_reference_animation_is_allowed(self):
        self.graph.animate()
        self.graph.target.referenced = self.graph.target.locked = True
        self.assertIs(self.graph.operations.isAnimationExist([self.graph.driver, self.graph.target]), True)
        self.assertIs(self.graph.operations.isAnimationAttribute(self.graph.target.attr('gain')), True)
        self.assertTrue(all(call.kwargs['animation'] == 'objects' for call in self.graph.pm.keyframe.call_args_list))
    def test_late_missing_locked_or_referenced_node_prevents_all_deletions(self):
        self.graph.animate()
        for state in ('missing', 'lock', 'reference'):
            self.graph.driver.locked = state == 'lock'
            self.graph.driver.referenced = state == 'reference'
            with self.assertRaises(ValueError): self.graph.operations.deleteAnimationKeys([self.graph.target, 'missing' if state == 'missing' else self.graph.driver])
        self.graph.pm.cutKey.assert_not_called()
    def test_late_locked_parent_or_referenced_curve_prevents_prefix_deletion(self):
        self.graph.animate()
        port = self.graph.target.attr('other')
        self.graph.animatable[self.graph.target].append(port)
        later = self.graph.animate(port)
        for state in ('parent', 'curve'):
            port.parents = [self.graph.target.add_port('parent')]
            port.parents[0].locked = state == 'parent'
            later.referenced = state == 'curve'
            with self.assertRaises(ValueError): self.graph.operations.deleteAnimationKeys([self.graph.target])
        self.graph.pm.cutKey.assert_not_called()
    def test_shared_curve_queries_are_deduplicated_and_cut_by_retained_identity(self):
        curve = self.graph.animate()
        other = self.graph.target.attr('other')
        self.graph.animatable[self.graph.target].append(other)
        self.graph.port_curves[other] = [curve]
        self.assertEqual(self.graph.operations.deleteAnimationKeys([self.graph.target]), 1)
        self.graph.pm.cutKey.assert_called_once_with(curve, clear=True, animation='objects', option='keys', hierarchy='none', shape=False)
    def test_short_or_qualified_attribute_names_resolve_to_same_curve(self):
        curve = self.graph.animate()
        for attribute in ('gain', 'target.gain', self.graph.target.attr('gain')):
            self.graph.pm.cutKey.reset_mock()
            self.assertEqual(self.graph.operations.deleteAnimationAttribute('target', attribute), 1)
            self.assertIs(self.graph.pm.cutKey.call_args.args[0], curve)
        with self.assertRaises(ValueError): self.graph.operations.deleteAnimationAttribute('driver', 'target.gain')
    def test_static_locked_attribute_does_not_block_other_animation(self):
        self.graph.animate()
        static = self.graph.target.attr('other')
        static.locked = True
        self.graph.animatable[self.graph.target].append(static)
        self.assertEqual(self.graph.operations.deleteAnimationKeys([self.graph.target]), 1)
    def test_wrong_native_attribute_or_curve_identity_cannot_delete(self):
        self.graph.animate()
        self.graph.animatable[self.graph.driver] = [self.graph.target.attr('gain')]
        with self.assertRaises(ValueError): self.graph.operations.deleteAnimationKeys([self.graph.target, self.graph.driver])
        self.graph.animatable[self.graph.driver] = None
        self.graph.port_curves[self.graph.target.attr('gain')] = [self.graph.driver]
        self.graph.keys[self.graph.driver] = [(0, 1)]
        with self.assertRaises(ValueError): self.graph.operations.deleteAnimationKeys([self.graph.target])
        self.graph.pm.cutKey.assert_not_called()
    def test_bad_key_count_and_unconfirmed_curve_query_fail_before_delete(self):
        for count in (-1, True, 1.0):
            self.graph.pm.keyframe.side_effect = None
            self.graph.pm.keyframe.return_value = count
            with self.assertRaises(ValueError): self.graph.operations.deleteAnimationKeys([self.graph.target])
        self.graph.pm.keyframe.side_effect = lambda *args, **kw: 1 if kw.get('keyframeCount') else None
        with self.assertRaises(ValueError): self.graph.operations.deleteAnimationKeys([self.graph.target])
        self.graph.pm.cutKey.assert_not_called()
    def test_native_deletion_failure_reports_confirmed_partial_curves(self):
        self.graph.animate()
        self.graph.animate()
        self.graph.pm.cutKey.side_effect = [1, RuntimeError('second failed')]
        with self.assertRaisesRegex(RuntimeError, 'after 1 confirmed curves'): self.graph.operations.deleteAnimationKeys([self.graph.target])
    def test_native_zero_deletion_is_not_success(self):
        self.graph.animate()
        for result in (0, None, True, 2):
            self.graph.pm.cutKey.return_value = result
            with self.assertRaisesRegex(RuntimeError, 'after 0 confirmed curves'): self.graph.operations.deleteAnimationKeys([self.graph.target])
    def test_empty_selection_report_is_noop(self):
        self.graph.selected = []
        self.assertEqual(self.graph.operations.echoAnimationData(), ())
    def test_explicit_node_report_ignores_unrelated_or_empty_selection(self):
        curve = self.graph.animate()
        self.graph.selected = []
        with contextlib.redirect_stdout(io.StringIO()): result = self.graph.operations.echoAnimationData('target')
        self.assertEqual(result, ((curve.name(), ((0.0, 1.0), (1.0, 2.0))),))
        array_calls = [call for call in self.graph.pm.keyframe.call_args_list if 'index' in call.kwargs]
        self.assertTrue(all(call.kwargs['index'] == (0, 1) for call in array_calls))
    def test_hierarchy_report_includes_descendant_animation(self):
        child = self.graph.create('transform', name='child', parent=self.graph.driver)
        self.graph.animatable[child] = [child.attr('gain')]
        curve = self.graph.animate(child.attr('gain'))
        with contextlib.redirect_stdout(io.StringIO()): result = self.graph.operations.echoAnimationData(self.graph.driver, True)
        self.assertEqual(result[0][0], curve.name())
        with self.assertRaises(TypeError): self.graph.operations.echoAnimationData(self.graph.driver, 1)
    def test_unitless_curve_report_queries_float_inputs(self):
        curve = self.graph.animate(kind='animCurveUU')
        with contextlib.redirect_stdout(io.StringIO()): self.graph.operations.echoAnimationData('target')
        self.assertTrue(any(call.args[0] is curve and call.kwargs.get('floatChange') for call in self.graph.pm.keyframe.call_args_list))
    def test_bad_report_arrays_and_nonfinite_values_are_rejected_before_printing(self):
        curve = self.graph.animate(keys=((0, 1), (1, float('nan'))))
        output = io.StringIO()
        with contextlib.redirect_stdout(output), self.assertRaises(ValueError): self.graph.operations.echoAnimationData('target')
        self.assertEqual(output.getvalue(), '')
        self.graph.keys[curve] = ((0, 1), (1, 2))
        native = self.graph.pm.keyframe.side_effect
        self.graph.pm.keyframe.side_effect = lambda value, **kw: [1] if kw.get('valueChange') else native(value, **kw)
        with self.assertRaises(ValueError): self.graph.operations.echoAnimationData('target')
    def test_report_and_attribute_limits_precede_native_array_or_cut_dispatch(self):
        self.graph.animatable[self.graph.target] = [self.graph.target.attr('gain')] * 10001
        with self.assertRaises(ValueError): self.graph.operations.deleteAnimationKeys([self.graph.target])
        self.graph.pm.cutKey.assert_not_called()
        self.graph.animatable[self.graph.target] = [self.graph.target.attr('gain')]
        self.graph.animate(keys=[(0, 1)] * 10001)
        with self.assertRaises(ValueError): self.graph.operations.echoAnimationData('target')
        self.assertFalse(any('index' in call.kwargs for call in self.graph.pm.keyframe.call_args_list))
    def test_transform_shape_animation_is_included_and_deletion_admits_the_shape(self):
        root = self.graph.create('transform', name='root')
        shape = self.graph.create('mesh', name='shape', parent=root)
        self.graph.animatable[root] = [shape.attr('gain')]
        self.graph.animate(shape.attr('gain'))
        self.assertTrue(self.graph.operations.isAnimationExist([root]))
        self.assertEqual(self.graph.operations.deleteAnimationKeys([root]), 1)
        shape.referenced = True
        self.graph.pm.cutKey.reset_mock()
        with self.assertRaises(ValueError): self.graph.operations.deleteAnimationKeys([root])
        self.graph.pm.cutKey.assert_not_called()
    def test_unrelated_native_shape_query_is_rejected_before_deletion(self):
        root = self.graph.create('transform', name='root')
        shape = self.graph.create('mesh', name='shape')
        self.graph.pm.listRelatives = lambda *args, **kwargs: [shape]
        self.graph.animatable[root] = [shape.attr('gain')]
        self.graph.animate(shape.attr('gain'))
        with self.assertRaises(ValueError): self.graph.operations.deleteAnimationKeys([root])
        self.graph.pm.cutKey.assert_not_called()


class UnitlessCurveTests(unittest.TestCase):
    def setUp(self):
        self.graph = AnimationGraph()
        self.created, self.deleted = [], []
        self.key_error = self.rename_error = self.cleanup_error = None
        self.name_suffix, self.null_result = '', False
        self.name_query_error, self.callback_name = None, None
        test = self
        class Object:
            def __init__(self): self.alive, self.label, self.keys = True, 'native_default', []
            def isNull(self): return test.null_result
        class Curve:
            kAnimCurveUU, kTangentLinear = 7, 2
            def __init__(self): self.object = None
            def create(self, kind):
                test.assertEqual(kind, 7)
                self.object = Object()
                test.created.append(self.object)
                return self.object
            def setName(self, name):
                if test.rename_error: raise test.rename_error
                self.object.label = name.lstrip(':') + test.name_suffix
            def name(self):
                if test.name_query_error and self.object.keys: raise test.name_query_error
                return self.object.label
            def addKey(self, time, value, incoming, outgoing):
                test.assertEqual((incoming, outgoing), (2, 2))
                if test.key_error and self.object.keys: raise test.key_error
                self.object.keys.append((time, value))
                if test.callback_name: self.object.label = test.callback_name
        class Modifier:
            def deleteNode(self, obj): self.object = obj
            def doIt(self):
                if test.cleanup_error: raise test.cleanup_error
                self.object.alive = False
                test.deleted.append(self.object)
        self.graph.operations.createAnimCurveUU.__globals__.update(oma=NS(MFnAnimCurve=Curve), om=NS(MDGModifier=Modifier))
    def create(self, name='requested', times=(1, 0), values=(3, 2)):
        return self.graph.operations.createAnimCurveUU(name, times, values)
    def test_requested_name_type_linear_keys_and_sorted_pairing_are_applied(self):
        self.assertEqual(self.create(), 'requested')
        self.assertEqual(self.created[0].keys, [(0.0, 2.0), (1.0, 3.0)])
        self.assertTrue(self.created[0].alive)
        self.assertEqual(self.deleted, [])
    def test_namespace_qualified_name_is_preserved(self): self.assertEqual(self.create('rig:curve'), 'rig:curve')
    def test_empty_bad_late_value_or_name_precedes_creation(self):
        for name, times, values in (('requested', (), ()), ('requested', (0, 1), (2, float('nan'))), ('bad*name', (0,), (1,)), ('requested', (0, 0), (1, 2))):
            with self.assertRaises((TypeError, ValueError)): self.create(name, times, values)
        self.assertEqual(self.created, [])
    def test_name_collision_preserves_existing_node_before_api_creation(self):
        borrowed = self.graph.create('animCurveUU', name='requested')
        with self.assertRaises(ValueError): self.create()
        self.assertTrue(borrowed.exists)
        self.assertEqual(self.created, [])
    def test_late_key_failure_deletes_only_the_returned_api_object(self):
        self.key_error = RuntimeError('key failed')
        with self.assertRaisesRegex(RuntimeError, 'key failed'): self.create()
        self.assertEqual(self.deleted, self.created)
        self.assertFalse(self.created[0].alive)
        self.assertTrue(all(node.exists for node in self.graph.nodes))
    def test_rename_failure_and_unexpected_native_suffix_clean_the_new_curve(self):
        self.rename_error = RuntimeError('rename failed')
        with self.assertRaises(RuntimeError): self.create()
        self.assertFalse(self.created[0].alive)
        self.rename_error, self.name_suffix = None, '1'
        with self.assertRaises(ValueError): self.create()
        self.assertFalse(self.created[-1].alive)
        self.name_suffix, self.callback_name = '', 'renamed_during_keys'
        with self.assertRaisesRegex(ValueError, 'name changed'): self.create()
        self.assertFalse(self.created[-1].alive)
        self.callback_name, self.name_query_error = None, RuntimeError('name query failed')
        with self.assertRaisesRegex(RuntimeError, 'name query failed'): self.create()
        self.assertFalse(self.created[-1].alive)
    def test_interruption_deletes_known_curve_and_propagates(self):
        self.key_error = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt): self.create()
        self.assertFalse(self.created[0].alive)
    def test_cleanup_failure_preserves_primary_and_warns(self):
        self.key_error, self.cleanup_error = ValueError('primary'), RuntimeError('cleanup failed')
        with self.assertRaisesRegex(ValueError, 'primary'): self.create()
        self.assertTrue(self.created[0].alive)
        self.graph.pm.warning.assert_called_once()
    def test_null_api_result_is_not_adopted_or_reported_as_success(self):
        self.null_result = True
        with self.assertRaisesRegex(RuntimeError, 'did not return an object'): self.create()
        self.assertEqual(self.deleted, [])
    def test_both_historical_factories_are_explicit_import_facades(self):
        import ast
        from test_python_table_selection import _tree
        for path in ('animOp.py', 'animCurveTools.py'):
            tree = _tree('python/DPyTools/anim/' + path)
            self.assertFalse(any(isinstance(node, ast.FunctionDef) for node in tree.body))
            self.assertTrue(any(isinstance(node, ast.ImportFrom) and node.module == 'DPyTools.features.animation' and any(alias.name == 'operations' for alias in node.names) for node in tree.body))


if __name__ == '__main__': unittest.main()
