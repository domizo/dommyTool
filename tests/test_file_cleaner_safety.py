#!/usr/bin/env python3
"""Real disposable preview identities and actual cleaner failure callbacks."""

import contextlib
import io
import os
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace as NS
from unittest.mock import Mock, patch

from test_python_table_selection import _callback
from DPyTools.core import file_cleanup, atomic_file


class CleanerFixture(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()
        self.a, self.b = self.root / 'a.pyc', self.root / 'b.pyc'
        self.a.write_bytes(b'a')
        self.b.write_bytes(b'b')
    def plan(self):
        return file_cleanup.build_deletion_plan(str(self.root))


class DeletionPlanTests(CleanerFixture):
    def test_success_deletes_only_the_exact_preview(self):
        plan = self.plan()
        other = self.root / 'after-preview.pyc'
        other.write_bytes(b'new')
        self.assertEqual(file_cleanup.execute_deletion_plan(plan), 2)
        self.assertTrue(other.exists())
    def test_replaced_late_candidate_prevents_every_initial_deletion(self):
        plan = self.plan()
        replacement = self.root / 'replacement.tmp'
        replacement.write_bytes(b'foreign')
        os.replace(str(replacement), str(self.b))
        with self.assertRaises(file_cleanup.FileCleanupError) as caught:
            file_cleanup.execute_deletion_plan(plan)
        self.assertEqual(caught.exception.deleted, 0)
        self.assertTrue(self.a.exists())
        self.assertEqual(self.b.read_bytes(), b'foreign')
    def test_inplace_content_or_mode_change_refuses_a_stale_preview(self):
        plan = self.plan()
        self.b.write_bytes(b'changed-length')
        with self.assertRaises(file_cleanup.FileCleanupError): file_cleanup.execute_deletion_plan(plan)
        self.assertTrue(self.a.exists())
        plan = self.plan()
        os.chmod(str(self.b), 0o600)
        with self.assertRaises(file_cleanup.FileCleanupError): file_cleanup.execute_deletion_plan(plan)
    def test_missing_candidate_or_new_symlink_preserves_other_files(self):
        plan = self.plan()
        self.b.unlink()
        with self.assertRaises(file_cleanup.FileCleanupError): file_cleanup.execute_deletion_plan(plan)
        self.assertTrue(self.a.exists())
        self.b.write_bytes(b'b')
        plan = self.plan()
        self.b.unlink()
        self.b.symlink_to(self.a)
        with self.assertRaises(file_cleanup.FileCleanupError): file_cleanup.execute_deletion_plan(plan)
        self.assertTrue(self.a.exists())
    def test_replaced_root_or_symlinked_parent_is_refused(self):
        nested = self.root / 'nested'
        nested.mkdir()
        (nested / 'c.pyc').write_bytes(b'c')
        plan = self.plan()
        old = self.root / 'old'
        nested.rename(old)
        nested.symlink_to(old, target_is_directory=True)
        with self.assertRaises(file_cleanup.FileCleanupError): file_cleanup.execute_deletion_plan(plan)
        self.assertTrue(self.a.exists())
        nested.unlink()
        old.rename(nested)
        plan = self.plan()
        moved = self.root.with_name(self.root.name + '-moved')
        self.root.rename(moved)
        self.addCleanup(lambda: moved.rename(self.root) if moved.exists() else None)
        self.root.mkdir()
        self.addCleanup(lambda: self.root.rmdir() if self.root.exists() else None)
        with self.assertRaises(file_cleanup.FileCleanupError): file_cleanup.execute_deletion_plan(plan)
    def test_late_replacement_reports_the_confirmed_removed_prefix(self):
        plan = self.plan()
        replacement = self.root / 'replacement.tmp'
        replacement.write_bytes(b'foreign')
        native = atomic_file.remove_owned_file
        def remove(filename, identity, **kw):
            result = native(filename, identity, **kw)
            if filename == str(self.a): os.replace(str(replacement), str(self.b))
            return result
        with patch.object(atomic_file, 'remove_owned_file', side_effect=remove):
            with self.assertRaises(file_cleanup.FileCleanupError) as caught:
                file_cleanup.execute_deletion_plan(plan)
        self.assertEqual(caught.exception.deleted, 1)
        self.assertFalse(self.a.exists())
        self.assertEqual(self.b.read_bytes(), b'foreign')
    def test_permission_failure_reports_partial_completion_without_rollback_claim(self):
        plan = self.plan()
        native = atomic_file.remove_owned_file
        def remove(filename, identity, **kw):
            if filename == str(self.b): raise PermissionError('unlink failed')
            return native(filename, identity, **kw)
        with patch.object(atomic_file, 'remove_owned_file', side_effect=remove):
            with self.assertRaises(file_cleanup.FileCleanupError) as caught:
                file_cleanup.execute_deletion_plan(plan)
        self.assertEqual(caught.exception.deleted, 1)
        self.assertTrue(self.b.exists())
    def test_interruption_is_not_wrapped_or_reported_as_success(self):
        with patch.object(atomic_file, 'remove_owned_file', side_effect=KeyboardInterrupt()):
            with self.assertRaises(KeyboardInterrupt): file_cleanup.execute_deletion_plan(self.plan())
        self.assertTrue(self.a.exists())
    def test_forged_duplicate_outside_or_wrong_extension_plans_are_refused(self):
        plan = self.plan()
        for entries in (plan.entries + plan.entries, ((str(self.root.parent / 'outside.pyc'), plan.entries[0][1]),),
                        ((str(self.root / 'a.txt'), plan.entries[0][1]),)):
            with self.assertRaises(file_cleanup.FileCleanupError):
                file_cleanup.execute_deletion_plan(plan._replace(entries=entries))
        self.assertTrue(self.a.exists())
    def test_unusable_inode_identity_disables_deletion(self):
        with patch.object(atomic_file, 'file_identity', return_value=(1, 0)):
            with self.assertRaisesRegex(ValueError, 'identity'): self.plan()
    def test_candidate_and_scan_limits_apply_before_confirmation(self):
        with patch.object(file_cleanup, 'MAX_CANDIDATES', 1):
            with self.assertRaises(ValueError): self.plan()
        with patch.object(file_cleanup, 'MAX_SCANNED_ENTRIES', 1):
            with self.assertRaises(ValueError): self.plan()
    def test_cleaner_never_changes_borrowed_file_permissions(self):
        identity = atomic_file.file_identity(os.lstat(str(self.a)))
        with patch.object(atomic_file.os, 'name', 'nt'), patch.object(atomic_file.os, 'chmod') as chmod:
            self.assertTrue(atomic_file.remove_owned_file(str(self.a), identity, make_writable=False))
            chmod.assert_not_called()


class CleanerCallbackTests(CleanerFixture):
    def setUp(self):
        super().setUp()
        self.confirm = Mock(return_value='Delete')
        self.pm = NS(textField=lambda control, **kw: str(self.root) if control == 'path' else self.extension,
                     confirmDialog=self.confirm, warning=Mock())
        self.extension = '.pyc'
        self.run = _callback('python/DPyTools/utility/DFileCleaner.py', 'DFileCleaner', 'runCleanCB',
                             {'pm': self.pm, 'file_cleanup': file_cleanup,
                              '_validate_root': file_cleanup.validate_root})
        self.ui = NS(projectPathText='path', extensionText='extension')
    def execute(self):
        with contextlib.redirect_stdout(io.StringIO()): return self.run(self.ui)
    def test_callback_success_or_cancel_reports_boolean_and_preserves_cancelled_files(self):
        self.confirm.return_value = 'Cancel'
        self.assertFalse(self.execute())
        self.assertTrue(self.a.exists())
        self.confirm.return_value = 'Delete'
        self.assertTrue(self.execute())
    def test_confirm_then_replace_refuses_to_delete_new_file(self):
        def confirm(**kw):
            replacement = self.root / 'replacement.tmp'
            replacement.write_bytes(b'foreign')
            os.replace(str(replacement), str(self.b))
            return 'Delete'
        self.confirm.side_effect = confirm
        self.assertFalse(self.execute())
        self.assertTrue(self.a.exists())
        self.assertEqual(self.b.read_bytes(), b'foreign')
        self.pm.warning.assert_called()
    def test_invalid_extension_is_reported_before_dialog(self):
        self.extension = None
        self.assertFalse(self.execute())
        self.confirm.assert_not_called()
    def test_interruption_propagates_even_if_warning_fails(self):
        self.pm.warning.side_effect = RuntimeError('warning failed')
        with patch.object(file_cleanup, 'execute_deletion_plan', side_effect=KeyboardInterrupt()):
            with self.assertRaises(KeyboardInterrupt): self.execute()


if __name__ == '__main__': unittest.main()
