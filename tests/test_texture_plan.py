#!/usr/bin/env python3
"""Unit tests for host-independent, collision-safe texture copying."""

import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.pipeline.lookdev import texture_plan
from DPyTools.core import atomic_file, file_publish


class TexturePlanTests(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp(prefix="dommytool-textures-")
        self.source_a = os.path.join(self.root, "source-a")
        self.source_b = os.path.join(self.root, "source-b")
        self.destination = os.path.join(self.root, "destination")
        os.mkdir(self.source_a)
        os.mkdir(self.source_b)
        os.mkdir(self.destination)

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def _texture(self, directory, name, content):
        filename = os.path.join(directory, name)
        Path(filename).write_bytes(content)
        return filename

    def test_unique_textures_are_copied_and_mapped(self):
        first = self._texture(self.source_a, "diffuse.png", b"diffuse")
        second = self._texture(self.source_b, "normal.png", b"normal")
        copies, mapping = texture_plan.build_copy_plan(
            [first, second], self.destination
        )
        created = texture_plan.execute_copy_plan(copies)
        self.assertEqual(set(created), set(mapping.values()))
        self.assertEqual(Path(mapping[first]).read_bytes(), b"diffuse")

    def test_empty_or_control_destination_is_rejected_before_path_normalization(self):
        with patch.object(texture_plan.os.path, 'abspath') as normalize, \
                patch.object(texture_plan, '_source_path') as source:
            for value in ('', 'bad\x00folder', 'bad\nfolder', 'bad\x7ffolder'):
                with self.assertRaises(texture_plan.TextureCopyError):
                    texture_plan.build_copy_plan(['source.png'], value)
            for value in (None, True, [], {}):
                with self.assertRaises(TypeError): texture_plan.build_copy_plan([], value)
        normalize.assert_not_called()
        source.assert_not_called()
        self.assertEqual(os.listdir(self.destination), [])

    def test_explicit_relative_destination_remains_supported(self):
        source = self._texture(self.source_a, 'diffuse.png', b'texture')
        previous = os.getcwd()
        try:
            os.chdir(self.root)
            copies, mapping = texture_plan.build_copy_plan([source], 'destination')
            texture_plan.execute_copy_plan(copies)
            self.assertTrue(os.path.samefile(os.path.dirname(mapping[source]), self.destination))
        finally: os.chdir(previous)

    @unittest.skipIf(os.name == 'nt', 'POSIX legacy source leaves, not Windows validation')
    def test_nonportable_copy_leaves_are_rejected_without_renaming_sources(self):
        for name in ('CON.png', 'bad:name.png', 'name.', 'a?b.png', 'has\\slash.png'):
            source = self._texture(self.source_a, name, b'legacy texture')
            with self.assertRaises(texture_plan.TextureCopyError):
                texture_plan.build_copy_plan([source], self.destination)
            self.assertEqual(Path(source).read_bytes(), b'legacy texture')
        self.assertEqual(os.listdir(self.destination), [])

    @unittest.skipIf(os.name == 'nt', 'POSIX legacy source leaf, not Windows validation')
    def test_invalid_later_leaf_does_not_produce_a_copied_prefix(self):
        good = self._texture(self.source_a, 'good.png', b'good')
        bad = self._texture(self.source_b, 'bad:name.png', b'bad')
        with self.assertRaises(texture_plan.TextureCopyError):
            texture_plan.build_copy_plan([good, bad], self.destination)
        self.assertEqual(os.listdir(self.destination), [])

    def test_unicode_portable_leaf_is_copied_without_changing_its_spelling(self):
        source = self._texture(self.source_a, '表面.png', b'unicode texture')
        copies, mapping = texture_plan.build_copy_plan([source], self.destination)
        texture_plan.execute_copy_plan(copies)
        self.assertEqual(os.path.basename(mapping[source]), '表面.png')
        self.assertEqual(Path(mapping[source]).read_bytes(), b'unicode texture')

    def test_different_sources_with_the_same_basename_are_rejected(self):
        first = self._texture(self.source_a, "diffuse.png", b"first")
        second = self._texture(self.source_b, "diffuse.png", b"second")
        with self.assertRaises(texture_plan.TextureCopyError):
            texture_plan.build_copy_plan([first, second], self.destination)

    def test_repeated_reference_to_the_same_source_is_copied_once(self):
        source = self._texture(self.source_a, "shared.png", b"shared")
        copies, mapping = texture_plan.build_copy_plan(
            [source, source], self.destination
        )
        self.assertEqual(len(copies), 1)
        self.assertEqual(mapping[source], copies[0][1])

    def test_same_file_alias_reuses_the_planned_destination(self):
        source = self._texture(self.source_a, "shared.png", b"shared")
        alias = os.path.join(self.source_b, "SHARED.PNG")
        try:
            os.symlink(source, alias)
        except (AttributeError, NotImplementedError, OSError):
            self.skipTest("Symbolic links are not available")

        copies, mapping = texture_plan.build_copy_plan(
            [source, alias], self.destination
        )
        self.assertEqual(len(copies), 1)
        self.assertEqual(mapping[source], mapping[alias])

    def test_partial_copy_is_rolled_back(self):
        first = self._texture(self.source_a, "first.png", b"first")
        second = self._texture(self.source_b, "second.png", b"second")
        copies, unused = texture_plan.build_copy_plan(
            [first, second], self.destination
        )
        os.remove(second)
        with self.assertRaises(IOError):
            texture_plan.execute_copy_plan(copies)
        self.assertEqual(os.listdir(self.destination), [])

    def test_failure_after_writing_bytes_removes_partial_copy(self):
        first = self._texture(self.source_a, "first.png", b"first")
        second = self._texture(self.source_b, "second.png", b"second")
        copies, mapping = texture_plan.build_copy_plan([first, second], self.destination)
        original_copy2, original_copyfileobj = shutil.copy2, shutil.copyfileobj

        def fail_copy2(source, destination):
            if source == second:
                Path(destination).write_bytes(b"partial")
                raise IOError("copy failed after writing bytes")
            return original_copy2(source, destination)

        def fail_copyfileobj(source, destination, *args):
            if source.name == second:
                destination.write(b"partial")
                raise IOError("copy failed after writing bytes")
            return original_copyfileobj(source, destination, *args)

        with patch.object(shutil, "copy2", side_effect=fail_copy2), \
                patch.object(shutil, "copyfileobj", side_effect=fail_copyfileobj):
            with self.assertRaisesRegex(IOError, "after writing bytes"):
                texture_plan.execute_copy_plan(copies)
        self.assertEqual(os.listdir(self.destination), [])
        self.assertEqual(Path(first).read_bytes(), b"first")
        self.assertEqual(Path(second).read_bytes(), b"second")
        self.assertFalse(os.path.exists(mapping[second]))

    def test_partial_destination_is_not_visible_during_copy(self):
        source = self._texture(self.source_a, "texture.png", b"complete")
        copies, mapping = texture_plan.build_copy_plan([source], self.destination)
        original_copy2, original_copyfileobj = shutil.copy2, shutil.copyfileobj

        def observe_copy2(source, destination):
            result = original_copy2(source, destination)
            self.assertFalse(os.path.lexists(mapping[source]))
            return result

        def observe_copyfileobj(input_stream, output_stream, *args):
            result = original_copyfileobj(input_stream, output_stream, *args)
            self.assertFalse(os.path.lexists(mapping[source]))
            return result

        with patch.object(shutil, "copy2", side_effect=observe_copy2), \
                patch.object(shutil, "copyfileobj", side_effect=observe_copyfileobj):
            texture_plan.execute_copy_plan(copies)
        self.assertEqual(Path(mapping[source]).read_bytes(), b"complete")
        self.assertEqual(os.listdir(self.destination), ["texture.png"])

    def test_destination_appearing_before_publication_is_not_overwritten(self):
        source = self._texture(self.source_a, "texture.png", b"source")
        copies, mapping = texture_plan.build_copy_plan([source], self.destination)
        original_stat = shutil.copystat

        def introduce_destination(input_path, staged_path, *args, **kwargs):
            original_stat(input_path, staged_path, *args, **kwargs)
            Path(mapping[source]).write_bytes(b"foreign")

        with patch.object(shutil, "copystat", side_effect=introduce_destination):
            with self.assertRaises((IOError, OSError, texture_plan.TextureCopyError)):
                texture_plan.execute_copy_plan(copies)
        self.assertEqual(Path(mapping[source]).read_bytes(), b"foreign")
        self.assertEqual(os.listdir(self.destination), ["texture.png"])

    def test_metadata_failure_cleans_staging_without_publishing(self):
        source = self._texture(self.source_a, "texture.png", b"source")
        copies, unused = texture_plan.build_copy_plan([source], self.destination)
        with patch.object(shutil, "copystat", side_effect=OSError("metadata failed")):
            with self.assertRaisesRegex(OSError, "metadata failed"):
                texture_plan.execute_copy_plan(copies)
        self.assertEqual(os.listdir(self.destination), [])

    def test_success_preserves_source_mode_and_mtime(self):
        source = self._texture(self.source_a, "texture.png", b"source")
        os.chmod(source, 0o640)
        os.utime(source, (1000000000, 1000000000))
        copies, mapping = texture_plan.build_copy_plan([source], self.destination)
        texture_plan.execute_copy_plan(copies)
        actual, expected = os.stat(mapping[source]), os.stat(source)
        self.assertEqual(actual.st_mode & 0o777, expected.st_mode & 0o777)
        self.assertEqual(actual.st_mtime, expected.st_mtime)

    def test_ledger_allocation_failure_cleans_unpublished_staging(self):
        source = self._texture(self.source_a, "texture.png", b"source")
        copies, unused = texture_plan.build_copy_plan([source], self.destination)
        with patch.object(texture_plan, "_CopyOwnership", side_effect=MemoryError("ledger failed")):
            with self.assertRaisesRegex(MemoryError, "ledger failed"):
                texture_plan.execute_copy_plan(copies)
        self.assertEqual(os.listdir(self.destination), [])

    @unittest.skipIf(os.name == 'nt', 'POSIX hard-link publication')
    def test_publication_failure_never_falls_back_to_overwrite(self):
        source = self._texture(self.source_a, "texture.png", b"source")
        copies, unused = texture_plan.build_copy_plan([source], self.destination)
        with patch.object(file_publish.os, 'link', side_effect=OSError('hard links unsupported')):
            with self.assertRaisesRegex(OSError, 'hard links unsupported'):
                texture_plan.execute_copy_plan(copies)
        self.assertEqual(os.listdir(self.destination), [])

    @unittest.skipIf(os.name == 'nt', 'POSIX hard-link publication')
    def test_missing_publication_primitive_fails_without_output(self):
        source = self._texture(self.source_a, 'texture.png', b'source')
        copies, unused = texture_plan.build_copy_plan([source], self.destination)
        with patch.object(file_publish.os, 'link', None):
            with self.assertRaisesRegex(OSError, 'unavailable on this platform'):
                texture_plan.execute_copy_plan(copies)
        self.assertEqual(os.listdir(self.destination), [])

    @unittest.skipIf(os.name == 'nt', 'POSIX identity substitution fixture')
    def test_rollback_does_not_delete_a_substituted_published_file(self):
        first = self._texture(self.source_a, 'first.png', b'first')
        second = self._texture(self.source_b, 'second.png', b'second')
        copies, mapping = texture_plan.build_copy_plan([first, second], self.destination)
        original_copy = shutil.copyfileobj

        def substitute_then_fail(source, destination, *args):
            if source.name == second:
                os.rename(mapping[first], os.path.join(self.root, 'retained-original.png'))
                Path(mapping[first]).write_bytes(b'foreign')
                destination.write(b'partial')
                raise IOError('second copy failed')
            return original_copy(source, destination, *args)

        with patch.object(shutil, 'copyfileobj', side_effect=substitute_then_fail):
            with self.assertRaisesRegex(IOError, 'second copy failed'):
                texture_plan.execute_copy_plan(copies)
        self.assertEqual(os.listdir(self.destination), ['first.png'])
        self.assertEqual(Path(mapping[first]).read_bytes(), b'foreign')

    def test_rollback_continues_after_one_cleanup_failure(self):
        sources = [self._texture(self.source_a, name + '.png', name.encode('ascii'))
                   for name in ('first', 'second', 'third')]
        copies, mapping = texture_plan.build_copy_plan(sources, self.destination)
        os.unlink(sources[2])
        original_unlink = os.unlink

        def block_second(path, *args, **kwargs):
            if path == mapping[sources[1]]:
                raise OSError('cleanup blocked')
            return original_unlink(path, *args, **kwargs)

        with patch.object(atomic_file.os, 'unlink', side_effect=block_second):
            with self.assertRaisesRegex(IOError, 'source disappeared'):
                texture_plan.execute_copy_plan(copies)
        self.assertEqual(os.listdir(self.destination), ['second.png'])
        self.assertEqual(Path(mapping[sources[1]]).read_bytes(), b'second')


if __name__ == "__main__":
    unittest.main()
