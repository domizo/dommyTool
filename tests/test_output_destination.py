#!/usr/bin/env python3
"""Shared output-leaf policy with disposable real text/JSON/image boundaries."""

import errno
import os
import stat
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'python'))
from DPyTools.core import atomic_file, atomic_text, file_publish, image_output, json_store
from DPyTools.features.rigging.face import lip_edges


class OutputDestinationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='dommy-output-policy-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.destination = self.root / 'output.txt'

    def test_absent_and_regular_destinations_are_admitted_without_mutation(self):
        self.assertEqual(atomic_file.validate_destination(str(self.destination)), str(self.destination))
        self.assertFalse(self.destination.exists())
        self.destination.write_bytes(b'old')
        self.assertEqual(atomic_file.validate_destination(str(self.destination)), str(self.destination))
        self.assertEqual(self.destination.read_bytes(), b'old')

    def test_initial_symlink_and_dangling_symlink_fail_before_staging(self):
        for present in (False, True):
            target = self.root / 'borrowed'
            if present: target.write_bytes(b'borrowed')
            self.destination.symlink_to(target)
            with patch.object(atomic_file.tempfile, 'mkstemp') as allocation:
                with self.assertRaises(IOError): atomic_text.AtomicTextWriter(str(self.destination))
            allocation.assert_not_called()
            self.assertTrue(self.destination.is_symlink())
            if present: self.assertEqual(target.read_bytes(), b'borrowed')
            self.destination.unlink()

    def test_directory_and_fifo_outputs_are_refused_without_opening_them(self):
        self.destination.mkdir()
        with patch.object(atomic_file.tempfile, 'mkstemp') as allocation:
            with self.assertRaises(IOError): atomic_text.write_text(str(self.destination), 'new')
            with self.assertRaises(IOError): json_store.dump(str(self.destination), {})
            with self.assertRaises(ValueError): image_output._destination(str(self.destination))
        allocation.assert_not_called()
        self.destination.rmdir()
        if hasattr(os, 'mkfifo'):
            os.mkfifo(str(self.destination))
            with patch.object(atomic_file.tempfile, 'mkstemp') as allocation:
                with self.assertRaises(IOError): atomic_text.write_text(str(self.destination), 'new')
                with self.assertRaises(IOError): json_store.dump(str(self.destination), {})
            allocation.assert_not_called()
            self.assertTrue(stat.S_ISFIFO(os.lstat(str(self.destination)).st_mode))

    def test_nonregular_native_types_use_the_same_admission_policy(self):
        for mode in (stat.S_IFCHR, stat.S_IFBLK, stat.S_IFSOCK, stat.S_IFIFO, stat.S_IFDIR, stat.S_IFLNK):
            with patch.object(atomic_file.os, 'lstat', return_value=SimpleNamespace(st_mode=mode)):
                with self.assertRaises(IOError): atomic_file.validate_destination('output')

    def test_lstat_permission_error_is_not_mistaken_for_absence_or_malformed_image(self):
        failure = OSError(errno.EACCES, 'access denied')
        with patch.object(atomic_file.os, 'lstat', side_effect=failure), \
                patch.object(atomic_file.tempfile, 'mkstemp') as allocation:
            for admit in (lambda: atomic_file.validate_destination(str(self.destination)),
                          lambda: atomic_text.AtomicTextWriter(str(self.destination)),
                          lambda: image_output._destination(str(self.destination))):
                with self.assertRaises(OSError) as caught: admit()
                self.assertIs(caught.exception, failure)
        allocation.assert_not_called()

    def test_text_commit_refuses_late_symlink_and_cleans_only_staging(self):
        self.destination.write_bytes(b'old')
        borrowed = self.root / 'borrowed'
        borrowed.write_bytes(b'borrowed')
        with self.assertRaises(IOError):
            with atomic_text.AtomicTextWriter(str(self.destination)) as writer:
                writer.write('new')
                self.destination.unlink()
                self.destination.symlink_to(borrowed)
        self.assertTrue(self.destination.is_symlink())
        self.assertEqual(borrowed.read_bytes(), b'borrowed')
        self.assertEqual(set(self.root.iterdir()), {self.destination, borrowed})

    def test_text_commit_refuses_late_directory_or_fifo(self):
        for kind in ('directory', 'fifo'):
            if kind == 'fifo' and not hasattr(os, 'mkfifo'): continue
            with self.assertRaises(IOError):
                with atomic_text.AtomicTextWriter(str(self.destination)) as writer:
                    writer.write('new')
                    if kind == 'directory': self.destination.mkdir()
                    else: os.mkfifo(str(self.destination))
            self.assertEqual(list(self.root.iterdir()), [self.destination])
            if kind == 'directory': self.destination.rmdir()
            else: self.destination.unlink()

    def test_json_late_symlink_is_not_replaced(self):
        borrowed = self.root / 'borrowed'
        borrowed.write_bytes(b'borrowed')
        chunks = json_store._output_chunks
        def substitute(encoder, data):
            self.destination.symlink_to(borrowed)
            return chunks(encoder, data)
        with patch.object(json_store, '_output_chunks', side_effect=substitute):
            with self.assertRaises(IOError): json_store.dump(str(self.destination), {'new': True})
        self.assertTrue(self.destination.is_symlink())
        self.assertEqual(borrowed.read_bytes(), b'borrowed')
        self.assertEqual(set(self.root.iterdir()), {borrowed, self.destination})

    def test_staged_replace_and_no_replace_refuse_late_nonregular_destination(self):
        for replace in (False, True):
            borrowed = self.root / 'borrowed'
            borrowed.write_bytes(b'borrowed')
            with self.assertRaises(IOError):
                with atomic_file.StagedFile(str(self.destination)) as staged:
                    staged.stream.write('new')
                    staged.seal()
                    self.destination.symlink_to(borrowed)
                    with patch.object(file_publish, 'replace_file') as replacement, \
                            patch.object(file_publish, 'publish_file_no_replace') as publication:
                        try: staged.publish(str(self.destination), replace_existing=replace)
                        finally:
                            replacement.assert_not_called()
                            publication.assert_not_called()
            self.assertTrue(self.destination.is_symlink())
            self.assertEqual(borrowed.read_bytes(), b'borrowed')
            self.destination.unlink()

    def test_late_lstat_failure_preserves_primary_error_and_old_output(self):
        self.destination.write_bytes(b'old')
        inspect = atomic_file.os.lstat
        failure = OSError(errno.EACCES, 'late destination inspection failed')
        def fail_destination(path):
            if path == str(self.destination): raise failure
            return inspect(path)
        with self.assertRaises(OSError) as caught:
            with atomic_text.AtomicTextWriter(str(self.destination)) as writer:
                writer.write('new')
                with patch.object(atomic_file.os, 'lstat', side_effect=fail_destination):
                    writer.commit()
        self.assertIs(caught.exception, failure)
        self.assertEqual(self.destination.read_bytes(), b'old')
        self.assertEqual(list(self.root.iterdir()), [self.destination])

    def test_lip_edge_save_never_replaces_the_existing_alias(self):
        borrowed = self.root / 'borrowed.elip'
        borrowed.write_bytes(b'borrowed original')
        self.destination.symlink_to(borrowed)
        with self.assertRaises(IOError): lip_edges.save(str(self.destination), [1], [2])
        self.assertTrue(self.destination.is_symlink())
        self.assertEqual(borrowed.read_bytes(), b'borrowed original')

    def test_regular_text_replacement_retains_mode_and_bytes(self):
        self.destination.write_bytes(b'old')
        os.chmod(str(self.destination), 0o640)
        before = stat.S_IMODE(os.stat(str(self.destination)).st_mode)
        with atomic_text.AtomicTextWriter(str(self.destination), mode='wb') as writer:
            writer.write('制御\n'.encode('utf-8'))
        self.assertEqual(self.destination.read_bytes(), '制御\n'.encode('utf-8'))
        self.assertEqual(stat.S_IMODE(os.stat(str(self.destination)).st_mode), before)

    def test_json_image_and_staging_share_one_validator(self):
        self.assertIs(json_store.validate_destination, atomic_file.validate_destination)
        self.assertIs(image_output.validate_destination, atomic_file.validate_destination)


if __name__ == '__main__': unittest.main()
