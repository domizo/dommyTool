#!/usr/bin/env python3
"""Actual confirmed Unknown-node plans and retained identities without Maya."""

import sys
import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock, call

from test_attribute_batches import Graph, Node
from test_scene_node_ownership import definitions
from DPyTools.core import strings


class UnknownNodeDeletionTests(unittest.TestCase):
    def setUp(self):
        self.graph = Graph()
        self.first = self.graph.create('unknown', name='firstUnknown')
        self.second = self.graph.create('unknown', name='secondUnknown')
        self.names = [self.first.name(), self.second.name()]
        self.choice = 'Yes'
        self.dialog_action = lambda: None
        self.pm = self.graph.pm
        self.pm.undo = Mock()
        self.pm.select = Mock()
        self.cmds = NS(ls=Mock(side_effect=lambda **kwargs: self.names),
                       confirmDialog=Mock(side_effect=self.confirm),
                       delete=Mock(side_effect=lambda value: self.graph.delete(self.graph.resolve(value))))
        self.scope = definitions('python/DPyTools/scene/nodes.py', {
            'pm': self.pm, 'cmds': self.cmds, 'sys': sys, 'string_utils': strings,
            'parameters': self.graph.parameters})
        self.run = self.scope['deleteUnknown']

    def confirm(self, **kwargs):
        self.dialog_action()
        return self.choice

    def assert_no_delete(self):
        self.pm.delete.assert_not_called()
        self.cmds.delete.assert_not_called()
        self.pm.undo.assert_not_called()
        self.pm.select.assert_not_called()

    def test_success_deletes_retained_nodes_in_order_and_reports_returned_calls(self):
        self.assertEqual(self.run(), 2)
        self.assertEqual(self.pm.delete.call_args_list, [call(self.first), call(self.second)])
        self.assertEqual(self.graph.deleted, [self.first, self.second])
        self.cmds.delete.assert_not_called()
        self.pm.warning.assert_not_called()
        self.pm.undo.assert_not_called()
        self.pm.select.assert_not_called()

    def test_none_or_empty_query_does_not_open_confirmation(self):
        for values in (None, [], ()):
            self.names = values
            self.assertEqual(self.run(), 0)
        self.cmds.confirmDialog.assert_not_called()
        self.assert_no_delete()

    def test_cancel_or_unexpected_response_is_a_zero_count_noop_and_default_is_no(self):
        for choice in ('No', 'Cancel', None, '', True):
            self.choice = choice
            self.assertEqual(self.run(), 0)
        options = self.cmds.confirmDialog.call_args.kwargs
        self.assertEqual(options['defaultButton'], 'No')
        self.assertEqual(options['cancelButton'], 'No')
        self.assertEqual(options['dismissString'], 'No')
        self.assert_no_delete()

    def test_malformed_duplicate_missing_or_over_limit_input_precedes_dialog(self):
        for names in ('firstUnknown', {}, [self.first, self.first],
                      [self.first, 'missingUnknown'], [self.first] * 10001):
            with self.subTest(kind=type(names).__name__):
                self.names = names
                self.assertIs(self.run(), False)
        self.cmds.confirmDialog.assert_not_called()
        self.assert_no_delete()

    def test_wrong_type_locked_or_referenced_later_node_prevents_dialog_and_all_deletes(self):
        for state in ('kind', 'locked', 'referenced'):
            original = getattr(self.second, state)
            setattr(self.second, state, 'transform' if state == 'kind' else True)
            self.assertIs(self.run(), False)
            setattr(self.second, state, original)
        self.cmds.confirmDialog.assert_not_called()
        self.assert_no_delete()

    def test_exact_bound_is_admitted_as_a_complete_readonly_plan(self):
        rows = [Node('unknown{0}'.format(index), 'unknown') for index in range(10000)]
        self.assertEqual(self.scope['_unknown_node_plan'](rows), tuple(rows))
        self.assert_no_delete()

    def test_name_reuse_after_confirmation_never_deletes_replacement(self):
        replacements = []
        def replace():
            self.second.exists = False
            replacements.append(self.graph.create('transform', name=self.second.name()))
        self.dialog_action = replace
        self.assertIs(self.run(), False)
        self.assertTrue(self.first.exists and replacements[0].exists)
        self.assert_no_delete()

    def test_replacement_unknown_is_not_adopted_into_the_confirmed_plan(self):
        def replace():
            self.second.exists = False
            self.graph.create('unknown', name=self.second.name())
        self.dialog_action = replace
        self.assertIs(self.run(), False)
        self.assert_no_delete()

    def test_renamed_retained_unknown_is_deleted_without_requerying_its_old_name(self):
        self.dialog_action = lambda: setattr(self.second, 'label', 'renamedUnknown')
        self.assertEqual(self.run(), 2)
        self.assertEqual(self.graph.deleted, [self.first, self.second])

    def test_complete_post_dialog_recheck_prevents_a_valid_prefix(self):
        for state in ('kind', 'locked', 'referenced'):
            original = getattr(self.second, state)
            self.dialog_action = lambda state=state: setattr(
                self.second, state, 'transform' if state == 'kind' else True)
            self.assertIs(self.run(), False)
            setattr(self.second, state, original)
        self.assert_no_delete()

    def test_each_destination_is_rechecked_after_previous_native_deletion(self):
        def delete(node):
            self.graph.delete(node)
            self.second.locked = True
        self.pm.delete.side_effect = delete
        self.assertIs(self.run(), False)
        self.assertEqual(self.graph.deleted, [self.first])
        self.assertTrue(self.second.exists)
        self.assertIn('after 1 completed delete calls', self.pm.warning.call_args.args[0])
        self.pm.undo.assert_not_called()

    def test_later_native_failure_reports_completed_prefix_without_undo(self):
        def delete(node):
            if node is self.second:
                raise RuntimeError('native deletion failed')
            self.graph.delete(node)
        self.pm.delete.side_effect = delete
        self.assertIs(self.run(), False)
        self.assertEqual(self.graph.deleted, [self.first])
        self.assertIn('after 1 completed delete calls', self.pm.warning.call_args.args[0])
        self.pm.undo.assert_not_called()

    def test_change_then_throw_is_not_counted_as_returned_or_rolled_back(self):
        def delete(node):
            self.graph.delete(node)
            raise RuntimeError('changed then failed')
        self.pm.delete.side_effect = delete
        self.assertIs(self.run(), False)
        self.assertFalse(self.first.exists)
        self.assertTrue(self.second.exists)
        self.assertIn('after 0 completed delete calls', self.pm.warning.call_args.args[0])
        self.assertIn('native edits may remain', self.pm.warning.call_args.args[0])
        self.pm.undo.assert_not_called()

    def test_query_failure_does_not_open_dialog_or_delete(self):
        self.cmds.ls.side_effect = RuntimeError('unknown query failed')
        self.assertIs(self.run(), False)
        self.cmds.confirmDialog.assert_not_called()
        self.assert_no_delete()

    def test_dialog_interruption_propagates_and_releases_guard_for_retry(self):
        error = KeyboardInterrupt('dialog interrupted')
        self.cmds.confirmDialog.side_effect = error
        with self.assertRaises(KeyboardInterrupt) as caught:
            self.run()
        self.assertIs(caught.exception, error)
        self.assert_no_delete()
        self.cmds.confirmDialog.side_effect = self.confirm
        self.assertEqual(self.run(), 2)

    def test_native_interruption_preserves_partial_edits_and_original_error(self):
        error = KeyboardInterrupt('delete interrupted')
        def delete(node):
            if node is self.second:
                raise error
            self.graph.delete(node)
        self.pm.delete.side_effect = delete
        with self.assertRaises(KeyboardInterrupt) as caught:
            self.run()
        self.assertIs(caught.exception, error)
        self.assertEqual(self.graph.deleted, [self.first])
        self.pm.undo.assert_not_called()
        self.choice = 'No'
        self.names = [self.second]
        self.assertEqual(self.run(), 0)

    def test_same_owner_reentry_during_dialog_and_native_deletion_is_refused(self):
        results = []
        self.dialog_action = lambda: results.append(self.run())
        def delete(node):
            results.append(self.run())
            self.graph.delete(node)
        self.pm.delete.side_effect = delete
        self.assertEqual(self.run(), 2)
        self.assertEqual(results, [False, False, False])
        self.cmds.ls.assert_called_once()
        self.cmds.confirmDialog.assert_called_once()


if __name__ == '__main__':
    unittest.main()
