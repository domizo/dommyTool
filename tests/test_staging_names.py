#!/usr/bin/env python3
"""Short sibling staging for valid long output leaves without Maya."""

import contextlib
import errno
import io
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from DPyTools.core import atomic_file, file_publish, image_output, json_store, paths
from DPyTools.core.atomic_text import AtomicTextWriter, write_text
from DPyTools.features.animation import parser, serializer
from DPyTools.features.rigging.face import lip_edges
from DPyTools.pipeline.lookdev import texture_plan
from test_animation_serializer import _writer_with_queries


class StagingNameTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='dommy-staging-names-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def long_leaf(self, suffix=''):
        if os.name == 'nt' or not hasattr(os, 'pathconf'):
            self.skipTest('POSIX NAME_MAX fixture, not native Windows path validation')
        self.name_max = os.pathconf(str(self.root), 'PC_NAME_MAX')
        if not 255 <= self.name_max <= 1024:
            self.skipTest('Fixture requires a bounded filesystem with 255-byte leaf support')
        leaf = 'x' * (255 - len(suffix)) + suffix
        self.assertEqual(paths.validate_portable_component(leaf), leaf)
        return leaf

    def assert_short_stage(self, stage, parent=None):
        stage = Path(stage)
        self.assertEqual(stage.parent, self.root if parent is None else parent)
        self.assertTrue(stage.name.startswith('.dommy-'))
        self.assertTrue(stage.name.endswith('.tmp'))
        self.assertLess(len(stage.name.encode('ascii')), 32)

    def test_staging_leaf_is_short_and_does_not_reuse_destination_text(self):
        for leaf in ('private-output-name.txt', '画像-output.txt'):
            with atomic_file.StagedFile(str(self.root / leaf), mode='wb') as staged:
                self.assert_short_stage(staged.temporary_name)
                self.assertNotIn(leaf, Path(staged.temporary_name).name)
            self.assertEqual(list(self.root.iterdir()), [])

    def test_same_destination_has_independent_exclusive_staging_files(self):
        destination = str(self.root / 'output.txt')
        with atomic_file.StagedFile(destination) as first, atomic_file.StagedFile(destination) as second:
            self.assertNotEqual(first.temporary_name, second.temporary_name)
            self.assertNotEqual(first.identity, second.identity)
            self.assert_short_stage(first.temporary_name)
            self.assert_short_stage(second.temporary_name)
        self.assertEqual(list(self.root.iterdir()), [])

    def test_255_byte_new_text_leaf_is_preserved_without_truncation(self):
        destination = self.root / self.long_leaf('.txt')
        write_text(str(destination), 'complete\n')
        self.assertEqual(destination.read_bytes(), b'complete\n')
        self.assertEqual(list(self.root.iterdir()), [destination])

    def test_255_byte_existing_text_leaf_preserves_mode_after_replacement(self):
        destination = self.root / self.long_leaf('.txt')
        destination.write_bytes(b'previous')
        destination.chmod(0o640)
        with AtomicTextWriter(str(destination)) as writer:
            self.assert_short_stage(writer.temporary_name)
            writer.write('new\n')
        self.assertEqual(destination.read_bytes(), b'new\n')
        self.assertEqual(destination.stat().st_mode & 0o777, 0o640)
        self.assertEqual(list(self.root.iterdir()), [destination])

    def test_255_byte_binary_leaf_keeps_utf8_and_lf_bytes(self):
        destination = self.root / self.long_leaf('.danim')
        payload = '制御\nnext\n'.encode('utf-8')
        with AtomicTextWriter(str(destination), mode='wb') as writer:
            writer.write(payload)
        self.assertEqual(destination.read_bytes(), payload)

    def test_255_byte_json_leaf_supports_new_exclusive_publication(self):
        destination = self.root / self.long_leaf('.json')
        data = {'name': '画像'}
        json_store.dump(str(destination), data, ensure_ascii=False, replace_existing=False)
        self.assertEqual(json_store.load(str(destination)), data)
        self.assertEqual(list(self.root.iterdir()), [destination])
        with self.assertRaises(OSError):
            json_store.dump(str(destination), {'name': 'not-published'}, replace_existing=False)
        self.assertEqual(json_store.load(str(destination)), data)
        self.assertEqual(list(self.root.iterdir()), [destination])

    def test_255_byte_json_leaf_supports_regular_replacement(self):
        destination = self.root / self.long_leaf('.json')
        destination.write_bytes(b'{"old": true}\n')
        json_store.dump(str(destination), {'new': True})
        self.assertEqual(json_store.load(str(destination)), {'new': True})
        self.assertEqual(list(self.root.iterdir()), [destination])

    def test_admitted_255_byte_image_leaf_reaches_encoder_with_explicit_format(self):
        leaf = self.long_leaf('.png')
        destination, fmt, size = image_output.output_plan(str(self.root), leaf[:-4])
        def encode(filename, image_format):
            self.assert_short_stage(filename)
            self.assertEqual(image_format, 'PNG')
            Path(filename).write_bytes(b'encoded-image')
            return True
        self.assertEqual(image_output.publish_image(destination, fmt, encode), destination)
        self.assertEqual((self.root / leaf).read_bytes(), b'encoded-image')
        self.assertEqual(list(self.root.iterdir()), [self.root / leaf])

    def test_failed_image_encoder_preserves_long_existing_output(self):
        destination = self.root / self.long_leaf('.png')
        destination.write_bytes(b'old-image')
        def encode(filename, image_format):
            self.assert_short_stage(filename)
            Path(filename).write_bytes(b'partial-image')
            return False
        with self.assertRaisesRegex(IOError, 'successful save'):
            image_output.publish_image(str(destination), 'PNG', encode)
        self.assertEqual(destination.read_bytes(), b'old-image')
        self.assertEqual(list(self.root.iterdir()), [destination])

    def test_255_byte_texture_leaf_copies_without_renaming_source_or_destination(self):
        leaf = self.long_leaf('.png')
        source_root, target_root = self.root / 'source', self.root / 'target'
        source_root.mkdir()
        target_root.mkdir()
        source = source_root / leaf
        source.write_bytes(b'texture')
        source.chmod(0o640)
        copies, mapping = texture_plan.build_copy_plan([str(source)], str(target_root))
        self.assertEqual(mapping[str(source)], str(target_root / leaf))
        texture_plan.execute_copy_plan(copies)
        self.assertEqual(source.read_bytes(), b'texture')
        self.assertEqual((target_root / leaf).read_bytes(), b'texture')
        self.assertEqual((target_root / leaf).stat().st_mode & 0o777, 0o640)
        self.assertEqual(list(target_root.iterdir()), [target_root / leaf])

    def test_later_texture_failure_removes_only_the_published_long_owned_leaf(self):
        leaf = self.long_leaf('.png')
        source_root, target_root = self.root / 'source', self.root / 'target'
        source_root.mkdir()
        target_root.mkdir()
        first, second = source_root / leaf, source_root / 'second.png'
        first.write_bytes(b'first')
        second.write_bytes(b'second')
        copies, mapping = texture_plan.build_copy_plan([str(first), str(second)], str(target_root))
        second.unlink()
        with self.assertRaises(IOError):
            texture_plan.execute_copy_plan(copies)
        self.assertEqual(first.read_bytes(), b'first')
        self.assertEqual(list(target_root.iterdir()), [])

    def test_body_failure_preserves_long_output_and_removes_short_staging(self):
        destination = self.root / self.long_leaf('.txt')
        destination.write_bytes(b'old')
        with self.assertRaisesRegex(ValueError, 'body failed'):
            with AtomicTextWriter(str(destination)) as writer:
                writer.write('partial')
                raise ValueError('body failed')
        self.assertEqual(destination.read_bytes(), b'old')
        self.assertEqual(list(self.root.iterdir()), [destination])

    def test_publication_failure_preserves_long_output_and_primary_error(self):
        destination = self.root / self.long_leaf('.txt')
        destination.write_bytes(b'old')
        primary = OSError('publication failed')
        with patch.object(file_publish, 'replace_file', side_effect=primary):
            with self.assertRaises(OSError) as caught:
                with AtomicTextWriter(str(destination)) as writer:
                    writer.write('new')
        self.assertIs(caught.exception, primary)
        self.assertEqual(destination.read_bytes(), b'old')
        self.assertEqual(list(self.root.iterdir()), [destination])

    def test_supported_long_unicode_leaf_does_not_consume_staging_name_budget(self):
        self.long_leaf()
        leaf = '名' * 80 + '.txt'
        self.assertEqual(len(leaf.encode('utf-8')), 244)
        destination = self.root / leaf
        destination.write_bytes(b'old')
        with AtomicTextWriter(str(destination), mode='wb') as writer:
            self.assert_short_stage(writer.temporary_name)
            writer.write(b'new')
        self.assertEqual(destination.read_bytes(), b'new')
        self.assertEqual(list(self.root.iterdir()), [destination])

    def test_invalid_native_leaf_still_fails_before_staging_allocation(self):
        self.long_leaf()
        destination = self.root / ('x' * (self.name_max + 1))
        with patch.object(atomic_file.tempfile, 'mkstemp') as allocation:
            with self.assertRaises(OSError) as caught:
                atomic_file.StagedFile(str(destination))
        self.assertEqual(caught.exception.errno, errno.ENAMETOOLONG)
        allocation.assert_not_called()
        self.assertEqual(list(self.root.iterdir()), [])

    def test_actual_lip_edge_writer_round_trips_a_255_byte_leaf(self):
        destination = self.root / self.long_leaf('.elip')
        data = lip_edges.save(str(destination), [1, 2], [3, 4])
        self.assertEqual(lip_edges.load(str(destination)), data)
        self.assertEqual(list(self.root.iterdir()), [destination])

    def test_converted_actual_danim_writer_round_trips_a_255_byte_leaf(self):
        destination = self.root / self.long_leaf('.danim')
        record = serializer.build_static_record('tx', 'node', False, 1)
        writer, warnings = _writer_with_queries((record,))
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertTrue(writer(filename=str(destination)))
        self.assertEqual(parser.parse_animation_file(str(destination)).records, (record,))
        self.assertEqual(warnings, [])
        self.assertEqual(list(self.root.iterdir()), [destination])


if __name__ == '__main__':
    unittest.main()
