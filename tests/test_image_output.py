#!/usr/bin/env python3
"""Pure capture admission and real disposable staged-image publication."""

import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'python'))
from DPyTools.core import image_output, paths
from DPyTools.pipeline.assets import paths as asset_paths


class ImageOutputTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.destination = self.root / 'thumbnail.png'
    def encoder(self, filename, image_format):
        self.assertIn(image_format, ('PNG', 'JPEG'))
        Path(filename).write_bytes(b'encoded-image')
        return True
    def test_default_plan_and_uppercase_formats_preserve_portable_names(self):
        destination, fmt, size = image_output.output_plan(str(self.root), 'thumbnail', 'PNG')
        self.assertEqual((destination, fmt, size), (str(self.destination), 'PNG', 320))
        self.assertEqual(image_output.output_plan(str(self.root), '画像', 'jpeg', 4096)[1:], ('JPEG', 4096))
    def test_component_validation_is_shared_with_asset_paths(self):
        self.assertIs(asset_paths.validate_component, paths.validate_component)
        for value in ('../escape', 'a/b', 'a\\b', '.', '..', ' bad', 'bad\x00name', 'C:escape'):
            with self.assertRaises((TypeError, ValueError)): image_output.output_plan(str(self.root), value)
    def test_image_portability_policy_is_shared_not_a_second_name_table(self):
        self.assertIs(image_output.RESERVED_NAMES, paths.PORTABLE_RESERVED_NAMES)
        with patch.object(paths, 'validate_portable_component', wraps=paths.validate_portable_component) as admit:
            image_output.output_plan(str(self.root), 'thumbnail')
        self.assertEqual([call.args[0] for call in admit.call_args_list], ['thumbnail', 'thumbnail.png'])
    def test_reserved_or_invalid_image_leaves_and_formats_fail(self):
        for value in ('CON', 'nul.txt', 'com1', 'lpt9', 'bad:name', 'name.', 'a?b', 'x' * 255):
            with self.assertRaises(ValueError): image_output.output_plan(str(self.root), value)
        for ext in ('gif', '.png', None, '', 'png/escape'):
            with self.assertRaises(ValueError): image_output.output_plan(str(self.root), 'thumbnail', ext)
    def test_size_requires_bounded_nonboolean_integers(self):
        for size in (True, False, 1.0, 10.5, '320', 0, -1, 4097, 10 ** 400):
            with self.subTest(size=size), self.assertRaises((TypeError, ValueError)):
                image_output.output_plan(str(self.root), 'thumbnail', size=size)
    def test_viewport_byte_budget_rejects_invalid_or_overlarge_dimensions(self):
        self.assertEqual(image_output.capture_byte_count(1920, 1080), 8294400)
        for pair in ((0, 2), (-1, 2), (True, 2), (2.0, 3), (16385, 1), (8192, 4096), (10 ** 400, 1)):
            with self.subTest(pair=pair), self.assertRaises((TypeError, ValueError)): image_output.capture_byte_count(*pair)
    def test_missing_symlink_directory_or_nonregular_destination_is_rejected(self):
        with self.assertRaises(IOError): image_output.output_plan(str(self.root / 'missing'), 'thumbnail')
        alias = self.root / 'alias'
        alias.symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(IOError): image_output.output_plan(str(alias), 'thumbnail')
        self.destination.mkdir()
        with self.assertRaises(ValueError): image_output.output_plan(str(self.root), 'thumbnail')
    def test_destination_symlink_is_never_written_through(self):
        borrowed = self.root / 'borrowed.png'
        borrowed.write_bytes(b'borrowed')
        self.destination.symlink_to(borrowed)
        with self.assertRaises(ValueError): image_output.output_plan(str(self.root), 'thumbnail')
        with self.assertRaises(ValueError): image_output.publish_image(str(self.destination), 'PNG', self.encoder)
        self.assertEqual(borrowed.read_bytes(), b'borrowed')
    def test_success_publishes_completed_bytes_and_removes_staging_file(self):
        self.destination.write_bytes(b'old')
        self.assertEqual(image_output.publish_image(str(self.destination), 'PNG', self.encoder), str(self.destination))
        self.assertEqual(self.destination.read_bytes(), b'encoded-image')
        self.assertEqual(list(self.root.iterdir()), [self.destination])
    def test_false_missing_or_unexpected_save_result_preserves_old_file(self):
        self.destination.write_bytes(b'old')
        for result in (False, None, 1, 'saved'):
            def encoder(filename, fmt):
                Path(filename).write_bytes(b'partial')
                return result
            with self.assertRaises(IOError): image_output.publish_image(str(self.destination), 'PNG', encoder)
            self.assertEqual(self.destination.read_bytes(), b'old')
            self.assertEqual(list(self.root.iterdir()), [self.destination])
    def test_empty_or_over_budget_encoded_output_cannot_publish(self):
        for data in (b'', b'four'):
            def encoder(filename, fmt):
                Path(filename).write_bytes(data)
                return True
            with patch.object(image_output, 'MAX_ENCODED_BYTES', 3), self.assertRaises(IOError): image_output.publish_image(str(self.destination), 'PNG', encoder)
        self.assertEqual(list(self.root.iterdir()), [])
    def test_encoder_failure_or_interruption_discards_only_owned_staging(self):
        self.destination.write_bytes(b'old')
        for error in (RuntimeError('encoder failed'), KeyboardInterrupt()):
            def encoder(filename, fmt):
                Path(filename).write_bytes(b'partial')
                raise error
            with self.assertRaises(type(error)): image_output.publish_image(str(self.destination), 'PNG', encoder)
            self.assertEqual(self.destination.read_bytes(), b'old')
            self.assertEqual(list(self.root.iterdir()), [self.destination])
    def test_publication_failure_preserves_old_image_and_discards_stage(self):
        self.destination.write_bytes(b'old')
        with patch('DPyTools.core.file_publish.replace_file', side_effect=OSError('replace failed')), self.assertRaises(OSError):
            image_output.publish_image(str(self.destination), 'PNG', self.encoder)
        self.assertEqual(self.destination.read_bytes(), b'old')
        self.assertEqual(list(self.root.iterdir()), [self.destination])
    def test_destination_symlink_appearing_during_encode_is_not_followed(self):
        borrowed = self.root / 'borrowed.png'
        borrowed.write_bytes(b'borrowed')
        def encoder(filename, fmt):
            self.encoder(filename, fmt)
            self.destination.symlink_to(borrowed)
            return True
        with self.assertRaises(ValueError): image_output.publish_image(str(self.destination), 'PNG', encoder)
        self.assertEqual(borrowed.read_bytes(), b'borrowed')
    def test_nonowned_staging_substitution_is_not_published_or_deleted(self):
        replacement = self.root / 'replacement'
        replacement.write_bytes(b'foreign')
        stage = []
        def encoder(filename, fmt):
            stage.append(Path(filename))
            os.replace(str(replacement), filename)
            return True
        with self.assertRaises(IOError): image_output.publish_image(str(self.destination), 'PNG', encoder)
        self.assertFalse(self.destination.exists())
        self.assertEqual(stage[0].read_bytes(), b'foreign')
    def test_substituted_stage_cannot_be_read_or_chmodded_as_an_owned_image(self):
        self.destination.write_bytes(b'old')
        borrowed = self.root / 'borrowed.png'
        borrowed.write_bytes(b'foreign')
        stage = []
        def encoder(filename, fmt):
            stage.append(Path(filename))
            os.unlink(filename)
            os.symlink(str(borrowed), filename)
            return True
        with patch.object(image_output.os.path, 'getsize') as size, patch.object(image_output.os, 'chmod') as mode:
            with self.assertRaises(IOError): image_output.publish_image(str(self.destination), 'PNG', encoder)
            size.assert_not_called()
            mode.assert_not_called()
        self.assertTrue(stage[0].is_symlink())
        self.assertEqual(borrowed.read_bytes(), b'foreign')
        self.assertEqual(self.destination.read_bytes(), b'old')


if __name__ == '__main__': unittest.main()
