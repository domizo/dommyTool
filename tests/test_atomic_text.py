#!/usr/bin/env python3
"""Regression checks for streamed atomic text output."""

import errno
import gc
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / 'python'))

from DPyTools.core.atomic_text import AtomicTextWriter, write_text
from DPyTools.core import atomic_file, file_publish


class AtomicTextWriterTests(unittest.TestCase):
    def test_fdopen_failure_closes_descriptor_and_removes_temporary_file(self):
        for failure in (OSError("fdopen failed"), MemoryError("allocation failed")):
            with self.subTest(failure=type(failure).__name__), tempfile.TemporaryDirectory() as directory:
                destination = Path(directory) / 'output.txt'
                destination.write_text('old', encoding='utf-8')
                retained = []
                original_mkstemp = tempfile.mkstemp

                def capture_temp(*args, **kwargs):
                    result = original_mkstemp(*args, **kwargs)
                    retained.append(result)
                    return result

                try:
                    with patch.object(tempfile, 'mkstemp', side_effect=capture_temp), \
                            patch.object(os, 'fdopen', side_effect=failure):
                        with self.assertRaises(type(failure)) as caught:
                            AtomicTextWriter(str(destination))
                    self.assertIs(caught.exception, failure)
                    self.assertEqual(len(retained), 1)
                    descriptor, filename = retained[0]
                    with self.assertRaises(OSError) as closed:
                        os.fstat(descriptor)
                    self.assertEqual(closed.exception.errno, errno.EBADF)
                    self.assertFalse(os.path.lexists(filename))
                    self.assertEqual(destination.read_text(encoding='utf-8'), 'old')
                finally:
                    gc.collect()
                    for descriptor, filename in retained:
                        try:
                            os.close(descriptor)
                        except OSError:
                            pass
                        if os.path.lexists(filename):
                            os.unlink(filename)

    def test_commit_replaces_destination(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / 'animation.danim'
            destination.write_text('old', encoding='utf-8')
            writer = AtomicTextWriter(str(destination))
            writer.write('new')
            writer.commit()
            self.assertEqual(destination.read_text(encoding='utf-8'), 'new')

    def test_abort_preserves_destination(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / 'animation.danim'
            destination.write_text('old', encoding='utf-8')
            writer = AtomicTextWriter(str(destination))
            writer.write('partial')
            writer.abort()
            self.assertEqual(destination.read_text(encoding='utf-8'), 'old')

    def test_context_manager_commits_only_on_success(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / 'output.txt'
            destination.write_text('old', encoding='utf-8')
            with AtomicTextWriter(str(destination)) as writer:
                writer.write('new')
            self.assertEqual(destination.read_text(encoding='utf-8'), 'new')

            with self.assertRaises(RuntimeError):
                with AtomicTextWriter(str(destination)) as writer:
                    writer.write('partial')
                    raise RuntimeError('stop')
            self.assertEqual(destination.read_text(encoding='utf-8'), 'new')

    def test_write_text_uses_atomic_writer(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / 'output.txt'
            write_text(str(destination), 'complete')
            self.assertEqual(
                destination.read_text(encoding='utf-8'), 'complete')

    def test_binary_mode_preserves_utf8_and_canonical_lf_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / 'output.txt'
            payload = '\u5236\u5fa1\nnext\n'.encode('utf-8')
            with AtomicTextWriter(str(destination), mode='wb') as writer:
                self.assertEqual(writer.write(payload), len(payload))
            self.assertEqual(destination.read_bytes(), payload)

    def test_invalid_mode_does_not_allocate_staging(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):
                AtomicTextWriter(os.path.join(directory, 'output.txt'), mode='a')
            self.assertEqual(os.listdir(directory), [])

    def test_abort_close_failure_does_not_mask_body_exception(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / 'output.txt'
            destination.write_text('old', encoding='utf-8')
            writer = AtomicTextWriter(str(destination))
            original_stream = writer.stream

            class FailAfterClose(object):
                def close(self):
                    original_stream.close()
                    raise OSError('close failed')

                def __getattr__(self, name):
                    return getattr(original_stream, name)

            writer.stream = FailAfterClose()
            with self.assertRaisesRegex(ValueError, 'original body error'):
                with writer:
                    raise ValueError('original body error')
            self.assertEqual(os.listdir(directory), ['output.txt'])
            self.assertEqual(destination.read_text(encoding='utf-8'), 'old')

    def test_commit_failure_preserves_output_and_cleans_temporary_file(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / 'output.txt'
            destination.write_text('old', encoding='utf-8')
            with patch.object(os, 'fsync', side_effect=OSError('fsync failed')):
                with self.assertRaisesRegex(OSError, 'fsync failed'):
                    with AtomicTextWriter(str(destination)) as writer:
                        writer.write('new')
            self.assertEqual(destination.read_text(encoding='utf-8'), 'old')
            self.assertEqual(os.listdir(directory), ['output.txt'])

    def test_replace_failure_preserves_existing_output(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / 'output.txt'
            destination.write_text('old', encoding='utf-8')
            with patch.object(file_publish, 'replace_file', side_effect=OSError('replace failed')):
                with self.assertRaisesRegex(OSError, 'replace failed'):
                    with AtomicTextWriter(str(destination)) as writer:
                        writer.write('new')
            self.assertEqual(destination.read_text(encoding='utf-8'), 'old')
            self.assertEqual(os.listdir(directory), ['output.txt'])

    def test_staged_stat_failure_closes_raw_descriptor(self):
        with tempfile.TemporaryDirectory() as directory:
            retained = []
            original_mkstemp = tempfile.mkstemp

            def capture_temp(*args, **kwargs):
                value = original_mkstemp(*args, **kwargs)
                retained.append(value)
                return value

            with patch.object(tempfile, 'mkstemp', side_effect=capture_temp), \
                    patch.object(os, 'fstat', side_effect=OSError('stat failed')):
                with self.assertRaisesRegex(OSError, 'stat failed'):
                    AtomicTextWriter(os.path.join(directory, 'output.txt'))
            descriptor, filename = retained[0]
            with self.assertRaises(OSError) as closed:
                os.fstat(descriptor)
            self.assertEqual(closed.exception.errno, errno.EBADF)
            self.assertFalse(os.path.lexists(filename))

    def test_explicit_discard_retries_failed_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            staged = atomic_file.StagedFile(os.path.join(directory, 'output.txt'))
            temporary_name = staged.temporary_name
            with patch.object(os, 'unlink', side_effect=OSError('unlink failed')):
                with self.assertRaisesRegex(OSError, 'unlink failed'):
                    staged.discard()
            self.assertIsNone(staged.stream)
            self.assertEqual(staged.temporary_name, temporary_name)
            self.assertTrue(os.path.exists(temporary_name))
            staged.discard()
            staged.discard()
            self.assertFalse(os.path.lexists(temporary_name))

    def test_failed_raw_close_is_not_retried_after_descriptor_reuse(self):
        with tempfile.TemporaryDirectory() as directory:
            owner = object.__new__(atomic_file.StagedFile)
            descriptor, filename = tempfile.mkstemp(dir=directory)
            owner.stream = None
            owner.temporary_name = filename
            owner.identity = atomic_file._file_identity(os.fstat(descriptor))
            owner._descriptor = descriptor
            original_close = os.close

            def close_then_fail(value):
                original_close(value)
                raise OSError('ambiguous close failure')

            with patch.object(os, 'close', side_effect=close_then_fail) as close:
                with self.assertRaisesRegex(OSError, 'ambiguous close failure'):
                    owner.discard()
                replacement = os.open(directory, os.O_RDONLY) if os.name != 'nt' else None
                try:
                    owner.discard()
                    self.assertIsNone(owner._descriptor)
                    self.assertEqual(close.call_count, 1)
                    if replacement is not None:
                        os.fstat(replacement)
                finally:
                    if replacement is not None:
                        original_close(replacement)
            self.assertFalse(os.path.lexists(filename))

    @unittest.skipIf(os.name == 'nt', 'POSIX identity substitution fixture')
    def test_staging_substitution_is_not_published_or_deleted(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / 'output.txt'
            staged = atomic_file.StagedFile(str(destination))
            staged.stream.write('owned')
            staged.seal()
            temporary_name = staged.temporary_name
            # Keep the original inode alive so this test does not rely on inode
            # reuse timing or claim that dev/inode identifiers authenticate data.
            retained = Path(directory) / 'retained.tmp'
            os.link(temporary_name, str(retained))
            os.unlink(temporary_name)
            Path(temporary_name).write_text('foreign', encoding='utf-8')
            with self.assertRaisesRegex(IOError, 'has been replaced'):
                staged.publish(str(destination))
            staged.discard()
            self.assertFalse(destination.exists())
            self.assertEqual(Path(temporary_name).read_text(encoding='utf-8'), 'foreign')


if __name__ == '__main__':
    unittest.main()
