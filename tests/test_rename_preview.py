#!/usr/bin/env python3
"""Pure preview plans and actual Renamer view callbacks without Maya."""

import contextlib
import functools
import unittest
from types import MethodType, SimpleNamespace as NS
from unittest.mock import Mock

from test_asset_browser import methods
from test_python_rig_safety import functions
from DPyTools.pipeline.assets import rename_plan


def model(targets=('one', 'two', 'three'), selected=None):
    return rename_plan.preview(tuple('|root|source{0}'.format(index) for index in range(len(targets))), targets, selected)


class RenamePreviewPlanTests(unittest.TestCase):
    def test_none_lists_and_selection_are_empty_not_negative_indices(self):
        self.assertEqual(rename_plan.preview(None, None, None), rename_plan.Preview((), (), ()))
        self.assertEqual(model(selected=None).selected, ())

    def test_model_and_coordinates_are_bounded_and_fully_admitted(self):
        for selected in ([0], [-1], [4], [True], [1.5], ['1'], [1, 1], '1', True):
            with self.assertRaises((TypeError, ValueError)): model(selected=selected)
        for sources, targets in ((['source'], []), ('source', ['one']),
                                 (['source'], ['']), (['bad\x00source'], ['one']),
                                 (['source'], ['|root|one']), (['source'], ['x' * 256]),
                                 (['s'] * 10001, ['t'] * 10001)):
            with self.assertRaises((TypeError, ValueError)): rename_plan.preview(sources, targets)

    def test_snapshot_is_independent_of_mutated_input_lists(self):
        sources, targets, rows = ['source'], ['target'], [1]
        snapshot = rename_plan.preview(sources, targets, rows)
        sources.clear()
        targets.clear()
        rows.clear()
        self.assertEqual(snapshot, rename_plan.Preview(('source',), ('target',), (1,)))

    def test_prefix_and_replace_keep_legacy_empty_selection_all_rows_behavior(self):
        before = model(('hero_a', 'hero_b'))
        self.assertEqual(rename_plan.prefix_preview(before, 'new').targets, ('new_hero_a', 'new_hero_b'))
        self.assertEqual(rename_plan.replace_preview(before, 'hero_', '').targets, ('a', 'b'))
        selected = model(('hero_a', 'hero_b'), [2])
        self.assertEqual(rename_plan.replace_preview(selected, 'hero', 'prop').targets, ('hero_a', 'prop_b'))

    def test_invalid_later_result_rejects_the_whole_plan(self):
        before = model(('short', 'x' * 255))
        with self.assertRaises(ValueError): rename_plan.prefix_preview(before, 'p')
        self.assertEqual(before.targets, ('short', 'x' * 255))
        with self.assertRaises(ValueError): rename_plan.replace_preview(model(('one', 'two')), 'two', '')

    def test_fragments_reject_types_controls_empty_search_and_oversize(self):
        before = model(selected=[1])
        for value in (None, True, [], '', 'bad\x00tag', 'x' * 256):
            for operation in (lambda: rename_plan.prefix_preview(before, value),
                              lambda: rename_plan.tag_preview(before, value),
                              lambda: rename_plan.replace_preview(before, value, 'replacement')):
                with self.assertRaises((TypeError, ValueError)): operation()

    def test_numbering_skips_collisions_and_preserves_selection_order(self):
        before = model(('node1', 'name', 'node3', 'name2'), [2, 4])
        self.assertEqual(rename_plan.numbered_preview(before, 'node').targets, ('node1', 'node2', 'node3', 'node4'))
        before = model(('node1', 'name', 'node3', 'name2'), [4, 2])
        self.assertEqual(rename_plan.numbered_preview(before, 'node').targets, ('node1', 'node4', 'node3', 'node2'))

    def test_numbering_repairs_duplicate_preview_targets_but_final_plan_stays_strict(self):
        before = model(('name', 'name', 'name'), [1, 2, 3])
        after = rename_plan.numbered_preview(before, 'node')
        self.assertEqual(after.targets, ('node1', 'node2', 'node3'))
        rename_plan.build_rename_plan(after.sources, after.targets)
        with self.assertRaises(ValueError): rename_plan.build_rename_plan(before.sources, before.targets)

    def test_numbering_is_bounded_at_the_complete_preview_size(self):
        before = model(tuple('node{0}'.format(index) for index in range(1, 10001)), list(range(1, 10001)))
        after = rename_plan.numbered_preview(before, 'node')
        self.assertEqual(after.targets, before.targets)

    def test_tags_use_literal_text_not_regular_expression_metacharacters(self):
        before = model(('hero_axb_a.b',), [1])
        self.assertEqual(rename_plan.tag_preview(before, '_a.b').targets, ('hero_axb',))
        self.assertEqual(rename_plan.tag_preview(model(('hero_[tag]',), [1]), '_[tag]').targets, ('hero',))

    def test_tag_switching_retains_first_match_and_custom_underscore_policy(self):
        before = model(('hero_l_c',), [1])
        self.assertEqual(rename_plan.tag_preview(before, '_r', ['_l', '_c']).targets, ('hero_c_r',))
        self.assertEqual(rename_plan.custom_tag_preview(model(('hero',), [1]), '_my_tag').targets, ('hero_mytag',))
        with self.assertRaises(ValueError): rename_plan.custom_tag_preview(before, '___')
        with self.assertRaises(ValueError): rename_plan.custom_tag_preview(before, '_' * 10000)

    def test_manual_edits_only_first_selected_row_and_delete_preserves_pairing(self):
        before = model(selected=[3, 1])
        self.assertEqual(rename_plan.manual_preview(before, '人物').targets, ('one', 'two', '人物'))
        deleted = rename_plan.delete_preview_rows(before)
        self.assertEqual(deleted.sources, (before.sources[1],))
        self.assertEqual(deleted.targets, ('two',))
        self.assertEqual(deleted.selected, ())

    def test_no_selected_number_tag_manual_or_delete_rows_are_noops(self):
        before = model()
        for result in (rename_plan.numbered_preview(before, 'node'),
                       rename_plan.tag_preview(before, '_ctrl'),
                       rename_plan.manual_preview(before, 'manual'),
                       rename_plan.delete_preview_rows(before)):
            self.assertEqual(result, before)


class RenamePreviewCallbackTests(unittest.TestCase):
    def setUp(self):
        self.sources = ['|root|one', '|root|two', '|root|three']
        self.targets = ['one', 'two', 'three']
        self.selected = None
        self.fields = {'prefix': 'pre', 'multi': 'node', 'search': 'o', 'replacement': 'O',
                       'ignore': 'old', 'custom': '_my_tag'}
        self.edits = []
        self.fail_edit = None
        self.interrupt = False
        self.reenter = None
        self.prompt_result = 'OK'
        self.prompt_text = 'manual'
        self.prompt_change = None

        def field(control, **kwargs):
            if kwargs.get('query') or kwargs.get('q'): return self.fields[control]
            self.fields[control] = kwargs['text']

        def scroll(control, **kwargs):
            values = self.sources if control == 'source' else self.targets
            if kwargs.get('query') or kwargs.get('q'):
                if kwargs.get('allItems'): return list(values) if values else None
                if kwargs.get('selectIndexedItem'): return self.selected
                raise AssertionError('Preview must not query selected text/native row counts')
            self.edits.append((control, kwargs))
            if self.fail_edit == len(self.edits):
                if self.interrupt: raise KeyboardInterrupt()
                raise RuntimeError('native view update failed')
            if kwargs.get('removeAll'): values.clear()
            if 'append' in kwargs: values.extend(kwargs['append'])
            if 'selectIndexedItem' in kwargs: self.selected = kwargs['selectIndexedItem']
            if self.reenter: self.reenter()

        def prompt(**kwargs):
            if kwargs.get('query') or kwargs.get('q'): return self.prompt_text
            if self.prompt_change: self.prompt_change()
            return self.prompt_result

        self.pm = NS(textField=Mock(side_effect=field), textScrollList=Mock(side_effect=scroll),
                     promptDialog=Mock(side_effect=prompt), warning=Mock(), rename=Mock(),
                     ls=Mock(return_value=None))
        browser_scope = functions('python/DPyTools/pipeline/assets/browser.py', {
            'pm': self.pm, 'functools': functools, 'contextmanager': contextlib.contextmanager})
        browser = NS(action=browser_scope['action'], updating=browser_scope['updating'])
        names = ('_preview_snapshot', '_write_preview', '_apply_preview', 'addPrefixTextCB', 'runRenameMultiCB',
                 'setIgoreTextCB', 'setSourceReplaceTextCB', 'runReplaceTextCB',
                 'manualNameChangeCB', 'deleteSelectObjectCB', 'addNameTag', 'addCustomTag')
        scope = methods('python/DPyTools/utility/DAssetRenamer.py', 'DAssetRenamer',
                        {'pm': self.pm, 'rename_plan': rename_plan, 'browser': browser}, names)
        self.owner = NS(srcTextScroll='source', targetTextScroll='target', prefixText='prefix',
                        RenameMultiText='multi', SourceRepText='search', ReplaceText='replacement',
                        ignoreText='ignore', CustomTagText='custom')
        for name in names: setattr(self.owner, name, MethodType(scope[name], self.owner))

    def test_none_selection_supports_prefix_and_replace_all_without_scene_rename(self):
        self.assertTrue(self.owner.addPrefixTextCB())
        self.assertEqual(self.targets, ['pre_one', 'pre_two', 'pre_three'])
        self.assertTrue(self.owner.runReplaceTextCB())
        self.assertEqual(self.targets, ['pre_One', 'pre_twO', 'pre_three'])
        self.assertEqual(self.sources, ['|root|one', '|root|two', '|root|three'])
        self.pm.rename.assert_not_called()

    def test_none_selection_is_noop_for_selected_only_actions_and_clears_pick_fields(self):
        for name in ('runRenameMultiCB', 'manualNameChangeCB', 'deleteSelectObjectCB', 'addNameTag', 'addCustomTag'):
            self.assertFalse(getattr(self.owner, name)())
        self.assertTrue(self.owner.setIgoreTextCB())
        self.assertTrue(self.owner.setSourceReplaceTextCB())
        self.assertEqual((self.fields['ignore'], self.fields['search']), ('', ''))
        self.assertEqual(self.edits, [])
        self.pm.promptDialog.assert_not_called()

    def test_empty_lists_do_not_crash_or_allocate_native_rows(self):
        self.sources.clear()
        self.targets.clear()
        self.assertFalse(self.owner.addPrefixTextCB())
        self.assertFalse(self.owner.runReplaceTextCB())
        self.assertEqual(self.edits, [])

    def test_invalid_coordinates_and_mismatched_lists_never_mutate_preview(self):
        for rows in ([0], [-1], [4], [True], [1.5], [1, 1], '1'):
            self.selected = rows
            self.assertFalse(self.owner.addPrefixTextCB())
        self.selected = None
        self.sources.pop()
        self.assertFalse(self.owner.runReplaceTextCB())
        self.assertEqual(self.edits, [])

    def test_bad_later_result_preserves_every_original_preview_row(self):
        self.targets[-1] = 'x' * 255
        previous = list(self.targets)
        self.assertFalse(self.owner.addPrefixTextCB())
        self.assertEqual(self.targets, previous)
        self.assertEqual(self.edits, [])

    def test_selected_tags_and_numbering_keep_other_rows_and_selection(self):
        self.selected = [2]
        self.assertTrue(self.owner.addNameTag(tagStr='_ctrl'))
        self.assertEqual(self.targets, ['one', 'two_ctrl', 'three'])
        self.assertEqual(self.selected, [2])
        self.assertTrue(self.owner.runRenameMultiCB())
        self.assertEqual(self.targets, ['one', 'node1', 'three'])

    def test_literal_tag_does_not_remove_a_regex_match_elsewhere(self):
        self.targets[1] = 'hero_axb_a.b'
        self.selected = [2]
        self.assertTrue(self.owner.addNameTag(tagStr='_a.b'))
        self.assertEqual(self.targets, ['one', 'hero_axb', 'three'])

    def test_custom_tag_uses_same_literal_planner_and_rejects_only_underscores(self):
        self.selected = [1]
        self.assertTrue(self.owner.addCustomTag())
        self.assertEqual(self.targets[0], 'one_mytag')
        self.edits.clear()
        self.fields['custom'] = '___'
        self.assertFalse(self.owner.addCustomTag())
        self.assertEqual(self.edits, [])

    def test_deletion_keeps_both_lists_aligned_and_is_finite(self):
        self.selected = [3, 1]
        self.assertTrue(self.owner.deleteSelectObjectCB())
        self.assertEqual((self.sources, self.targets), (['|root|two'], ['two']))
        self.assertEqual(len(self.edits), 4)
        self.pm.rename.assert_not_called()

    def test_manual_cancel_and_invalid_text_leave_view_unchanged(self):
        self.selected = [2]
        self.prompt_result = 'Cancel'
        self.assertFalse(self.owner.manualNameChangeCB())
        self.prompt_result = 'OK'
        self.prompt_text = 'bad\x00name'
        self.assertFalse(self.owner.manualNameChangeCB())
        self.assertEqual(self.edits, [])

    def test_manual_prompt_uses_retained_row_and_refuses_changed_context(self):
        self.selected = [2]
        self.assertTrue(self.owner.manualNameChangeCB())
        self.assertEqual(self.targets, ['one', 'manual', 'three'])
        self.edits.clear()
        self.prompt_change = lambda: self.targets.__setitem__(1, 'new-context')
        self.assertFalse(self.owner.manualNameChangeCB())
        self.assertEqual(self.targets[1], 'new-context')
        self.assertEqual(self.edits, [])

    def test_same_owner_reentry_is_suppressed_during_view_updates(self):
        results = []
        self.reenter = lambda: results.append(self.owner.addPrefixTextCB())
        self.assertTrue(self.owner.addPrefixTextCB())
        self.assertTrue(results)
        self.assertTrue(all(result is False for result in results))
        self.assertFalse(self.owner._updating_browser)
        self.assertEqual(self.targets[0], 'pre_one')

    def test_native_view_failure_and_interruption_release_guard_without_scene_mutation(self):
        for interrupt in (False, True):
            self.setUp()
            self.fail_edit = 2
            self.interrupt = interrupt
            if interrupt:
                with self.assertRaises(KeyboardInterrupt): self.owner.addPrefixTextCB()
            else: self.assertFalse(self.owner.addPrefixTextCB())
            self.assertFalse(self.owner._updating_browser)
            self.pm.rename.assert_not_called()
            if not interrupt: self.pm.warning.assert_called_once()

    def test_picking_fields_uses_admitted_row_not_truncated_selected_text(self):
        self.selected = [3, 1]
        self.assertTrue(self.owner.setIgoreTextCB())
        self.assertTrue(self.owner.setSourceReplaceTextCB())
        self.assertEqual((self.fields['ignore'], self.fields['search']), ('three', 'three'))


if __name__ == '__main__': unittest.main()
