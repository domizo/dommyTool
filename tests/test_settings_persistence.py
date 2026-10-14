#!/usr/bin/env python3
"""Strict settings writes and safe display fallback, without Maya/profile I/O."""

import errno
import os
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch

from test_python_rig_safety import functions

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'python'))
from DPyTools.core import json_store
from DPyTools.pipeline.assets import repository, settings


INVALID_DOCUMENTS = (
    b'{"schema_version":2,"username":"original_artist"}',
    b'{"schema_version":1,"unknown":"preserve"}',
    b'{"username":"original","username":"duplicate"}',
    b'{"username": "\xff"}',
    b'{"notif_mail_opt_in":"true"}',
    b'{"current_project":"../escape"}',
    b'[]', b'null', b'{"unfinished":',
)


class SettingsFixture(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='dommy-settings-')
        self.addCleanup(self.temporary.cleanup)
        self.filename = Path(self.temporary.name) / '.dasset'
        self.defaults = settings.default_settings(self.temporary.name, 'default_artist')

    def write_valid(self):
        data = dict(self.defaults, username='original_artist',
                    current_project='original_project', notif_mail_opt_in=True)
        json_store.dump(str(self.filename), data)
        return data


class SettingsPersistenceTests(SettingsFixture):
    def test_missing_read_returns_defaults_without_creating_a_file(self):
        result = repository.load_settings(str(self.filename), self.defaults)
        self.assertEqual(result, self.defaults)
        self.assertIsNot(result, self.defaults)
        self.assertFalse(self.filename.exists())

    def test_partial_update_preserves_original_values_and_defaults(self):
        original = self.write_valid()
        repository.update_settings(str(self.filename), self.defaults, {'current_project': ''})
        self.assertEqual(json_store.load(str(self.filename)), dict(original, current_project=''))
        self.assertEqual(self.defaults['username'], 'default_artist')

    def test_legacy_partial_document_gets_defaults_only_after_validation(self):
        self.filename.write_bytes(b'{"username":"legacy_artist"}')
        result = repository.load_settings(str(self.filename), self.defaults)
        self.assertEqual(result, dict(self.defaults, username='legacy_artist'))
        repository.update_settings(str(self.filename), self.defaults, {'current_project': 'hero'})
        self.assertEqual(json_store.load(str(self.filename)), dict(result, current_project='hero'))

    def test_invalid_original_refuses_read_merge_and_replacement_without_changes(self):
        for payload in INVALID_DOCUMENTS:
            with self.subTest(payload=payload):
                self.filename.write_bytes(payload)
                for action in (
                        lambda: repository.load_settings(str(self.filename), self.defaults),
                        lambda: repository.update_settings(str(self.filename), self.defaults, {}),
                        lambda: repository.update_settings(str(self.filename), self.defaults,
                                                           self.defaults, replace=True)):
                    with self.assertRaises((TypeError, ValueError)):
                        action()
                    self.assertEqual(self.filename.read_bytes(), payload)
                self.assertEqual(list(self.filename.parent.iterdir()), [self.filename])

    def test_valid_full_replacement_retains_historical_replacement_semantics(self):
        self.write_valid()
        replacement = settings.validate_settings({'username': 'replacement'})
        repository.update_settings(str(self.filename), self.defaults, replacement, replace=True)
        self.assertEqual(json_store.load(str(self.filename)), replacement)

    def test_missing_update_creates_valid_settings_with_defaults(self):
        repository.update_settings(str(self.filename), self.defaults, {'current_project': 'new'})
        self.assertEqual(json_store.load(str(self.filename)), dict(self.defaults, current_project='new'))

    def test_invalid_update_is_rejected_before_inspecting_original(self):
        with patch.object(repository.os, 'lstat') as inspect:
            for update in (None, [], {'schema_version': 2}, {'username': ''}):
                with self.assertRaises((TypeError, ValueError)):
                    repository.update_settings(str(self.filename), self.defaults, update)
        inspect.assert_not_called()
        self.assertFalse(self.filename.exists())

    def test_replacement_policy_must_be_boolean_before_io(self):
        with patch.object(repository.os, 'lstat') as inspect:
            for value in (0, 1, None, 'false'):
                with self.assertRaises(TypeError):
                    repository.update_settings(str(self.filename), self.defaults, {}, replace=value)
        inspect.assert_not_called()

    def test_lstat_errors_other_than_missing_are_not_defaults(self):
        error = OSError(errno.EACCES, 'access denied')
        with patch.object(repository.os, 'lstat', side_effect=error), \
                patch.object(json_store, 'dump') as writer:
            with self.assertRaises(OSError) as caught:
                repository.update_settings(str(self.filename), self.defaults, {})
        self.assertIs(caught.exception, error)
        writer.assert_not_called()

    def test_read_error_preserves_original_and_never_calls_writer(self):
        self.write_valid()
        payload = self.filename.read_bytes()
        error = IOError('unreadable settings')
        with patch.object(json_store, 'load', side_effect=error), \
                patch.object(json_store, 'dump') as writer:
            with self.assertRaises(IOError) as caught:
                repository.update_settings(str(self.filename), self.defaults, {})
        self.assertIs(caught.exception, error)
        writer.assert_not_called()
        self.assertEqual(self.filename.read_bytes(), payload)

    def test_removed_original_after_lstat_is_not_reinitialized(self):
        self.write_valid()
        original_load = json_store.load

        def removed(filename, **kwargs):
            self.filename.unlink()
            self.assertNotIn('default', kwargs)
            return original_load(filename, **kwargs)

        with patch.object(json_store, 'load', side_effect=removed), \
                patch.object(json_store, 'dump') as writer:
            with self.assertRaises(IOError):
                repository.update_settings(str(self.filename), self.defaults, {})
        writer.assert_not_called()
        self.assertFalse(self.filename.exists())

    def test_present_directory_and_fifo_fail_without_opening_or_writing(self):
        self.filename.mkdir()
        with patch('builtins.open') as reader:
            with self.assertRaises(IOError):
                repository.update_settings(str(self.filename), self.defaults, {})
        reader.assert_not_called()
        self.filename.rmdir()
        if hasattr(os, 'mkfifo'):
            os.mkfifo(str(self.filename))
            with patch('builtins.open') as reader:
                with self.assertRaises(IOError):
                    repository.update_settings(str(self.filename), self.defaults, {})
            reader.assert_not_called()

    def test_symlink_target_is_readable_but_never_overwritten(self):
        target = self.filename.parent / 'real-settings'
        json_store.dump(str(target), self.defaults)
        self.filename.symlink_to(target)
        self.assertEqual(repository.load_settings(str(self.filename), self.defaults), self.defaults)
        with self.assertRaises(OSError):
            repository.update_settings(str(self.filename), self.defaults, {'username': 'new'})
        self.assertTrue(self.filename.is_symlink())
        self.assertEqual(json_store.load(str(target)), self.defaults)

    def test_dangling_symlink_is_existing_invalid_settings_not_a_new_path(self):
        target = self.filename.parent / 'missing-target'
        self.filename.symlink_to(target)
        with patch.object(json_store, 'dump') as writer:
            with self.assertRaises(IOError):
                repository.update_settings(str(self.filename), self.defaults, {})
        writer.assert_not_called()
        self.assertTrue(self.filename.is_symlink())
        self.assertFalse(target.exists())

    def test_initialization_is_exclusive_for_valid_and_invalid_existing_files(self):
        for payload in (b'{"username":"original"}', b'broken settings'):
            self.filename.write_bytes(payload)
            with self.assertRaises(OSError):
                repository.create_settings(str(self.filename), self.defaults)
            self.assertEqual(self.filename.read_bytes(), payload)
            self.assertEqual(list(self.filename.parent.iterdir()), [self.filename])
        self.filename.unlink()
        self.assertEqual(repository.create_settings(str(self.filename), self.defaults), str(self.filename))
        self.assertEqual(json_store.load(str(self.filename)), self.defaults)

    def test_file_appearing_during_new_update_is_not_overwritten(self):
        chunks = json_store._output_chunks
        payload = b'newer external settings'

        def concurrent(encoder, data):
            self.filename.write_bytes(payload)
            return chunks(encoder, data)

        with patch.object(json_store, '_output_chunks', side_effect=concurrent):
            with self.assertRaises(OSError):
                repository.update_settings(str(self.filename), self.defaults, {})
        self.assertEqual(self.filename.read_bytes(), payload)
        self.assertEqual(list(self.filename.parent.iterdir()), [self.filename])

    def test_write_failure_and_interruption_preserve_original(self):
        self.write_valid()
        payload = self.filename.read_bytes()
        for error in (IOError('publish failed'), KeyboardInterrupt()):
            with patch.object(json_store.StagedFile, 'publish', side_effect=error):
                with self.assertRaises(type(error)) as caught:
                    repository.update_settings(str(self.filename), self.defaults, {'username': 'new'})
            self.assertIs(caught.exception, error)
            self.assertEqual(self.filename.read_bytes(), payload)
            self.assertEqual(list(self.filename.parent.iterdir()), [self.filename])


class SettingsAdapterTests(SettingsFixture):
    def setUp(self):
        super().setUp()
        self.pm = SimpleNamespace(warning=Mock())
        self.scope = functions('python/DPyTools/pipeline/assets/operations.py', {
            'asset_repository': repository, 'pm': self.pm})
        self.scope['_defaultAssetUserData'] = lambda: dict(self.defaults)
        self.scope['getAssetUserFileName'] = lambda: str(self.filename)

    def test_display_fallback_warns_and_never_writes_invalid_original(self):
        for payload in INVALID_DOCUMENTS:
            self.filename.write_bytes(payload)
            self.assertEqual(self.scope['readUserAssetData'](), self.defaults)
            self.assertEqual(self.filename.read_bytes(), payload)
        self.assertEqual(self.pm.warning.call_count, len(INVALID_DOCUMENTS))

    def test_partial_update_reports_false_and_preserves_invalid_original(self):
        for payload in INVALID_DOCUMENTS:
            self.filename.write_bytes(payload)
            self.assertFalse(self.scope['setAssetUserDataDic']({'current_project': ''}))
            self.assertEqual(self.filename.read_bytes(), payload)
        self.assertEqual(self.pm.warning.call_count, len(INVALID_DOCUMENTS))

    def test_direct_replacement_propagates_error_and_preserves_invalid_original(self):
        self.filename.write_bytes(INVALID_DOCUMENTS[0])
        with self.assertRaises(ValueError):
            self.scope['updateUserAssetData'](self.defaults)
        self.assertEqual(self.filename.read_bytes(), INVALID_DOCUMENTS[0])

    def test_initialization_refuses_existing_and_succeeds_when_absent(self):
        self.filename.write_bytes(b'preserve')
        with self.assertRaises(OSError):
            self.scope['createDefaultAssetUserData']()
        self.assertEqual(self.filename.read_bytes(), b'preserve')
        self.filename.unlink()
        self.scope['createDefaultAssetUserData']()
        self.assertEqual(json_store.load(str(self.filename)), self.defaults)

    def test_valid_partial_adapter_update_preserves_user_values(self):
        original = self.write_valid()
        self.assertTrue(self.scope['setAssetUserDataDic']({'current_project': ''}))
        self.assertEqual(json_store.load(str(self.filename)), dict(original, current_project=''))
        self.pm.warning.assert_not_called()

    def test_interruptions_are_not_swallowed_as_display_or_write_validation(self):
        self.write_valid()
        payload = self.filename.read_bytes()
        with patch.object(json_store, 'load', side_effect=KeyboardInterrupt()):
            for name in ('readUserAssetData', 'setAssetUserDataDic'):
                with self.assertRaises(KeyboardInterrupt):
                    self.scope[name]()
        self.pm.warning.assert_not_called()
        self.assertEqual(self.filename.read_bytes(), payload)


if __name__ == '__main__':
    unittest.main()
