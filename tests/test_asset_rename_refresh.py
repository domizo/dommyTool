#!/usr/bin/env python3
"""Complete Renamer refresh preparation and guarded native view boundaries."""

import contextlib
import functools
import logging
import unittest
from types import MethodType, SimpleNamespace as NS
from unittest.mock import Mock, patch

from test_asset_browser import methods
from test_python_rig_safety import functions
from DPyTools.core import components
from DPyTools.pipeline.assets import rename_plan


class Node:
    def __init__(self, name, parent=None):
        self.label, self.parent = name, parent
        self.name = Mock(return_value=name)
        self.listRelatives = Mock(return_value=None)

    def getParent(self):
        return self.parent


class RenameRefreshTests(unittest.TestCase):
    def setUp(self):
        self.first, self.second = Node('|root|one'), Node('|root|two')
        self.selection = [self.first]
        self.candidates = [self.first, self.second]
        self.filter_type = 1
        self.fields = {'filter': '', 'ignore': ''}
        self.models = {'source': ['old-source'], 'target': ['old-target']}
        self.edits = []
        self.fail_edit = None
        self.interrupt_edit = False
        self.reenter = None
        self.classified = []
        self.types = ['controlled-type']

        def scroll(control, **kwargs):
            if kwargs.get('query') or kwargs.get('q'):
                raise AssertionError('Refresh must not depend on the old, possibly partial view')
            self.edits.append((control, kwargs))
            if len(self.edits) == self.fail_edit:
                if self.interrupt_edit:
                    raise KeyboardInterrupt()
                raise RuntimeError('controlled view failure')
            if kwargs.get('removeAll'):
                self.models[control].clear()
            if 'append' in kwargs:
                self.models[control].extend(kwargs['append'])
            if self.reenter:
                self.reenter()

        def ls(**kwargs):
            return self.selection if kwargs.get('sl') else self.classified

        self.pm = NS(textScrollList=Mock(side_effect=scroll),
                     textField=Mock(side_effect=lambda control, **kwargs: self.fields[control]),
                     optionMenu=Mock(side_effect=lambda *args, **kwargs: self.filter_type),
                     ls=Mock(side_effect=ls), listNodeTypes=Mock(side_effect=lambda *args: self.types),
                     warning=Mock(), rename=Mock())
        query_names = ('getAssetTransformNoShapeFromHierarchy', 'getAssetTransformFromHierarchy',
                       'getAssetGeometryFromHierarchy', 'getAssetTypeNodeFromHierarchy',
                       'getAssetShapeParentFromHierarchy')
        self.api = NS(**{name: Mock(side_effect=lambda *args, **kwargs: self.candidates)
                         for name in query_names})
        self.api.assetAutoRenameGen = Mock(side_effect=lambda obj, ignore='': obj.label.rsplit('|', 1)[-1])
        browser_scope = functions('python/DPyTools/pipeline/assets/browser.py', {
            'pm': self.pm, 'functools': functools, 'contextmanager': contextlib.contextmanager})
        browser = NS(action=browser_scope['action'], updating=browser_scope['updating'])
        names = ('_selection_preview', '_apply_preview', 'changeSelectObject')
        scope = methods('python/DPyTools/utility/DAssetRenamer.py', 'DAssetRenamer', {
            'pm': self.pm, 'dasset': self.api, 'rename_plan': rename_plan, 'browser': browser,
            'components': components, '_LOGGER': logging.getLogger('test-renamer')}, names)
        self.owner = NS(srcTextScroll='source', targetTextScroll='target',
                        filterMenu='menu', filterTextField='filter', ignoreText='ignore')
        for name in names:
            setattr(self.owner, name, MethodType(scope[name], self.owner))

    def assert_unchanged(self):
        self.assertEqual(self.models, {'source': ['old-source'], 'target': ['old-target']})
        self.assertEqual(self.edits, [])
        self.pm.rename.assert_not_called()
        self.assertFalse(self.owner._updating_browser)

    def test_hierarchy_query_failure_preserves_both_previous_lists(self):
        self.api.getAssetTransformNoShapeFromHierarchy.side_effect = RuntimeError('query failed')
        self.assertFalse(self.owner.changeSelectObject())
        self.assert_unchanged()
        self.pm.warning.assert_called_once()

    def test_later_name_generation_failure_preserves_both_previous_lists(self):
        self.api.assetAutoRenameGen.side_effect = ['new-one', ValueError('later name failed')]
        self.assertFalse(self.owner.changeSelectObject())
        self.assert_unchanged()
        self.assertEqual(self.api.assetAutoRenameGen.call_count, 2)

    def test_later_source_name_failure_preserves_previous_lists(self):
        self.second.name.side_effect = RuntimeError('deleted node')
        self.assertFalse(self.owner.changeSelectObject())
        self.assert_unchanged()

    def test_generated_none_controls_hierarchy_and_overlength_fail_before_view_changes(self):
        for value in (None, '', 'bad\x00name', '|root|name', 'x' * 256):
            with self.subTest(value=value):
                self.api.assetAutoRenameGen.side_effect = ['valid', value]
                self.assertFalse(self.owner.changeSelectObject())
                self.assert_unchanged()
        self.assertNotIn('Error', self.models['target'])

    def test_complete_plan_exists_before_first_view_mutation(self):
        admitted = []
        original = rename_plan.preview
        def preview(*args, **kwargs):
            result = original(*args, **kwargs)
            admitted.append(result)
            return result
        def assert_prepared():
            self.assertEqual(self.api.assetAutoRenameGen.call_count, 2)
            self.assertTrue(admitted)
            self.assertEqual(admitted[0].sources, ('|root|one', '|root|two'))
            self.assertEqual(admitted[0].targets, ('one', 'two'))
        self.reenter = assert_prepared
        with patch.object(rename_plan, 'preview', side_effect=preview):
            self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(self.models, {'source': ['|root|one', '|root|two'], 'target': ['one', 'two']})
        self.pm.rename.assert_not_called()

    def test_none_selection_clears_both_lists_without_collection_or_empty_append(self):
        self.selection = None
        self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(self.models, {'source': [], 'target': []})
        self.assertEqual(len(self.edits), 2)
        self.assertTrue(all('append' not in kwargs for _, kwargs in self.edits))
        self.api.assetAutoRenameGen.assert_not_called()
        self.pm.textField.assert_not_called()

    def test_none_hierarchy_result_produces_a_paired_empty_view(self):
        self.candidates = None
        self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(self.models, {'source': [], 'target': []})
        self.api.assetAutoRenameGen.assert_not_called()

    def test_overlapping_selected_hierarchies_deduplicate_identities_in_first_seen_order(self):
        self.selection = [self.first, self.second]
        self.candidates = [self.second, self.first, self.second]
        self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(self.models['source'], ['|root|two', '|root|one'])
        self.assertEqual(self.models['target'], ['two', 'one'])
        self.assertEqual(self.api.assetAutoRenameGen.call_count, 2)

    def test_text_filter_retains_unicode_order_and_passes_ignore_only_to_name_generator(self):
        self.first.label = '|root|人物One'
        self.first.name.return_value = self.first.label
        self.fields.update(filter='人物one', ignore='literal.*')
        self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(self.models, {'source': [self.first.label], 'target': ['人物One']})
        self.api.assetAutoRenameGen.assert_called_once_with(obj=self.first, ignore='literal.*')

    def test_filtered_out_nodes_are_not_auto_named(self):
        self.fields['filter'] = 'not-present'
        self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(self.models, {'source': [], 'target': []})
        self.api.assetAutoRenameGen.assert_not_called()

    def test_invalid_menu_choices_do_not_fall_back_to_all_nodes(self):
        for value in (0, 13, True, 1.5, '1', None):
            with self.subTest(value=value):
                self.filter_type = value
                self.assertFalse(self.owner.changeSelectObject())
                self.assert_unchanged()
        self.pm.ls.assert_not_called()

    def test_nontext_filter_fields_fail_before_queries_or_view_changes(self):
        for key in ('filter', 'ignore'):
            self.fields[key] = None
            self.assertFalse(self.owner.changeSelectObject())
            self.assert_unchanged()
            self.fields[key] = ''
        self.api.getAssetTransformNoShapeFromHierarchy.assert_not_called()

    def test_selection_and_unique_hierarchy_row_budgets_apply_before_view_mutation(self):
        with patch.object(rename_plan, 'MAX_RENAME_ITEMS', 2):
            self.selection = [self.first] * 3
            self.assertFalse(self.owner.changeSelectObject())
            self.assert_unchanged()
            self.api.getAssetTransformNoShapeFromHierarchy.assert_not_called()
            self.selection = [self.first]
            self.candidates = [self.first, self.second, Node('third')]
            self.assertFalse(self.owner.changeSelectObject())
            self.assert_unchanged()
            self.api.assetAutoRenameGen.assert_not_called()

    def test_exact_row_budget_accepts_duplicates_without_consuming_extra_rows(self):
        self.candidates = [self.first, self.second, self.first, self.second]
        with patch.object(rename_plan, 'MAX_RENAME_ITEMS', 2):
            self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(self.models['target'], ['one', 'two'])

    def test_geometry_menu_dispatch_preserves_supported_shape_types(self):
        for mode, kind in ((3, None), (4, 'mesh'), (5, 'nurbsCurve'), (6, 'nurbsSurface')):
            self.filter_type = mode
            self.api.getAssetGeometryFromHierarchy.reset_mock()
            self.assertTrue(self.owner.changeSelectObject())
            if kind is None:
                self.api.getAssetGeometryFromHierarchy.assert_called_once_with(self.first)
            else:
                self.api.getAssetGeometryFromHierarchy.assert_called_once_with(self.first, geotypes=[kind])

    def test_actual_geometry_helper_and_refresh_include_selected_mesh_itself(self):
        shape = Node('meshShape')
        shape.nodeType = Mock(return_value='mesh')
        self.first.nodeType = Mock(return_value='transform')
        self.pm.listRelatives = Mock(side_effect=lambda node, **kwargs: [shape] if node is self.first else None)
        scope = functions('python/DPyTools/pipeline/assets/operations.py', {'pm': self.pm})
        scope['assetAutoRenameGen'] = self.api.assetAutoRenameGen
        self.api.getAssetGeometryFromHierarchy = scope['getAssetGeometryFromHierarchy']
        self.filter_type = 4
        self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(self.models, {'source': ['|root|one'], 'target': ['one']})
        self.pm.listRelatives.assert_any_call(self.first, shapes=True)

    def test_transform_joint_camera_and_set_menu_dispatch_remains_explicit(self):
        for mode in (2, 7, 9, 10):
            self.filter_type = mode
            for value in vars(self.api).values():
                value.reset_mock()
            self.assertTrue(self.owner.changeSelectObject())
            if mode == 2:
                self.api.getAssetTransformFromHierarchy.assert_called_once_with(self.first)
            elif mode == 9:
                self.api.getAssetShapeParentFromHierarchy.assert_called_once_with(self.first, shapetypes=['camera'])
            else:
                self.api.getAssetTypeNodeFromHierarchy.assert_called_once_with(
                    self.first, typeStr='joint' if mode == 7 else 'objectSet')

    def test_light_and_material_classification_uses_node_identity_and_historical_order(self):
        self.filter_type = 8
        self.classified = [Node('lightShape', self.second), Node('orphanShape')]
        self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(self.models['source'], ['|root|two'])
        self.pm.listNodeTypes.assert_called_with('light')
        self.filter_type = 12
        self.classified = [self.first]
        self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(self.models['source'], ['|root|one'])
        self.pm.listNodeTypes.assert_called_with('shader')

    def test_missing_classification_never_issues_an_empty_type_all_scene_query(self):
        for mode in (8, 12):
            for types in (None, []):
                self.filter_type, self.types = mode, types
                self.pm.ls.reset_mock()
                self.assertTrue(self.owner.changeSelectObject())
                self.assertEqual(self.models, {'source': [], 'target': []})
                self.pm.ls.assert_called_once_with(sl=True)

    def test_none_classified_nodes_and_constraints_are_normalized(self):
        self.filter_type, self.classified = 8, None
        self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(self.models['source'], [])
        self.filter_type = 11
        self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(self.models['source'], [])
        self.first.listRelatives.assert_called_once_with(c=True, ad=True, type='constraint')
        self.first.listRelatives.return_value = [self.second]
        self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(self.models['target'], ['two'])

    def test_preparation_and_commit_suppress_same_owner_reentry(self):
        results = []
        def collect(*args, **kwargs):
            results.append(self.owner.changeSelectObject())
            return self.candidates
        self.api.getAssetTransformNoShapeFromHierarchy.side_effect = collect
        self.reenter = lambda: results.append(self.owner.changeSelectObject())
        self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(results, [False] * 5)
        self.assertFalse(self.owner._updating_browser)
        self.pm.warning.assert_not_called()

    def test_refresh_repairs_an_old_mismatched_partial_view_without_reading_it(self):
        self.models['source'].append('orphan-row')
        self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(self.models, {'source': ['|root|one', '|root|two'], 'target': ['one', 'two']})

    def test_partial_native_view_failure_is_reported_and_next_refresh_can_repair_it(self):
        self.fail_edit = 2
        self.assertFalse(self.owner.changeSelectObject())
        self.assertFalse(self.owner._updating_browser)
        self.pm.warning.assert_called_once()
        self.assertEqual(self.models, {'source': [], 'target': ['old-target']})
        self.fail_edit = None
        self.assertTrue(self.owner.changeSelectObject())
        self.assertEqual(self.models, {'source': ['|root|one', '|root|two'], 'target': ['one', 'two']})
        self.pm.rename.assert_not_called()

    def test_preparation_interruption_propagates_and_preserves_old_rows(self):
        self.api.assetAutoRenameGen.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            self.owner.changeSelectObject()
        self.assert_unchanged()
        self.pm.warning.assert_not_called()

    def test_native_view_interruption_propagates_and_releases_nested_guards(self):
        self.fail_edit, self.interrupt_edit = 2, True
        with self.assertRaises(KeyboardInterrupt):
            self.owner.changeSelectObject()
        self.assertFalse(self.owner._updating_browser)
        self.pm.warning.assert_not_called()
        self.pm.rename.assert_not_called()


if __name__ == '__main__':
    unittest.main()
