#!/usr/bin/env python3
"""Unit tests for Maya-independent asset version metadata validation."""

import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.core import json_store
from DPyTools.pipeline.assets import metadata, repository, versioning


class AssetMetadataTests(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp(prefix="dommytool-asset-")
        version_dir = os.path.join(self.root, "v001")
        os.mkdir(version_dir)
        self.filepath = os.path.join(version_dir, "hero_v001.ma")
        Path(self.filepath).write_text("// maya ascii")
        self.record = {
            "version": "v001",
            "filepath": self.filepath,
            "filename": "hero_v001.ma",
            "comment": "initial export",
            "creator": "artist",
            "extension": ".ma",
            "thumbnail": os.path.join(version_dir, "thumbnail.png"),
            "date": "2019/01/01 12:00:00",
        }

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def test_valid_record_is_copied(self):
        validated = metadata.validate_version_record(
            self.record, asset_root=self.root, require_file=True
        )
        self.assertEqual(validated, self.record)
        self.assertIsNot(validated, self.record)

    def test_missing_or_inconsistent_fields_are_rejected(self):
        missing = dict(self.record)
        del missing["creator"]
        with self.assertRaises(ValueError):
            metadata.validate_version_record(missing, asset_root=self.root)

        mismatch = dict(self.record, extension=".mb")
        with self.assertRaises(ValueError):
            metadata.validate_version_record(mismatch, asset_root=self.root)

        unknown = dict(self.record, internal_note="not part of the format")
        with self.assertRaises(ValueError):
            metadata.validate_version_record(unknown, asset_root=self.root)

    def test_paths_outside_asset_root_are_rejected(self):
        outside_dir = tempfile.mkdtemp(prefix="dommytool-outside-")
        try:
            outside = os.path.join(outside_dir, "hero_v001.ma")
            Path(outside).write_text("outside")
            record = dict(self.record, filepath=outside)
            with self.assertRaises(ValueError):
                metadata.validate_version_record(record, asset_root=self.root)
        finally:
            shutil.rmtree(outside_dir, ignore_errors=True)

    def test_symlink_escape_is_rejected(self):
        outside_dir = tempfile.mkdtemp(prefix="dommytool-outside-")
        outside = os.path.join(outside_dir, "linked.ma")
        link = os.path.join(self.root, "v001", "linked.ma")
        Path(outside).write_text("outside")
        try:
            os.symlink(outside, link)
        except (AttributeError, NotImplementedError, OSError):
            shutil.rmtree(outside_dir, ignore_errors=True)
            self.skipTest("Symbolic links are not available")
        try:
            record = dict(self.record, filepath=link, filename="linked.ma")
            with self.assertRaises(ValueError):
                metadata.validate_version_record(record, asset_root=self.root)
        finally:
            shutil.rmtree(outside_dir, ignore_errors=True)

    def test_missing_file_is_only_rejected_when_required(self):
        os.remove(self.filepath)
        metadata.validate_version_record(self.record, asset_root=self.root)
        with self.assertRaises(IOError):
            metadata.validate_version_record(
                self.record, asset_root=self.root, require_file=True
            )

    def test_import_validation_always_requires_the_current_file(self):
        metadata.validate_import_record(self.record, asset_root=self.root)
        os.remove(self.filepath)
        with self.assertRaises(IOError):
            metadata.validate_import_record(self.record, asset_root=self.root)

    def test_existing_posix_legacy_filename_remains_readable_after_new_name_policy(self):
        if os.name == 'nt': self.skipTest('POSIX legacy leaf fixture, not native Windows validation')
        path = Path(self.root) / 'v001' / 'legacy:name.ma'
        path.write_text('// legacy Maya scene', encoding='utf-8')
        record = dict(self.record, filepath=str(path), filename=path.name)
        self.assertEqual(metadata.validate_import_record(record, self.root), record)

    def test_duplicate_versions_are_rejected(self):
        with self.assertRaises(ValueError):
            metadata.validate_version_records(
                [self.record, dict(self.record)], asset_root=self.root
            )

    def test_duplicate_filepaths_are_rejected_across_versions(self):
        duplicate_path = dict(self.record, version="v002")
        with self.assertRaises(ValueError):
            metadata.validate_version_records(
                [self.record, duplicate_path], asset_root=self.root
            )

    def test_versioned_document_uses_portable_relative_paths(self):
        document = metadata.encode_version_document([self.record], self.root)

        self.assertEqual(document["schema_version"], 1)
        stored = document["versions"][0]
        self.assertEqual(stored["filepath"], "v001/hero_v001.ma")
        self.assertEqual(stored["thumbnail"], "v001/thumbnail.png")
        decoded = metadata.decode_version_document(document, self.root)
        self.assertEqual(decoded, [self.record])

    def test_legacy_list_is_still_accepted(self):
        decoded = metadata.decode_version_document([self.record], self.root)
        self.assertEqual(decoded, [self.record])

    def test_versioned_document_can_be_relocated(self):
        document = metadata.encode_version_document([self.record], self.root)
        relocated_root = tempfile.mkdtemp(prefix="dommytool-relocated-")
        try:
            relocated_version = os.path.join(relocated_root, "v001")
            os.mkdir(relocated_version)
            relocated_file = os.path.join(relocated_version, "hero_v001.ma")
            Path(relocated_file).write_text("// relocated")

            decoded = metadata.decode_version_document(
                document, relocated_root, require_files=True
            )
            self.assertEqual(decoded[0]["filepath"], relocated_file)
            self.assertEqual(
                decoded[0]["thumbnail"],
                os.path.join(relocated_version, "thumbnail.png"),
            )
        finally:
            shutil.rmtree(relocated_root, ignore_errors=True)

    def test_unsafe_relative_and_unsupported_schema_are_rejected(self):
        document = metadata.encode_version_document([self.record], self.root)
        document["versions"][0]["filepath"] = "../outside.ma"
        with self.assertRaises(ValueError):
            metadata.decode_version_document(document, self.root)

        document = metadata.encode_version_document([self.record], self.root)
        document["schema_version"] = 999
        with self.assertRaises(ValueError):
            metadata.decode_version_document(document, self.root)

    def test_repository_writes_new_format_and_reads_it_back(self):
        metadata_file = os.path.join(self.root, ".asset")
        repository.save_version_records(
            metadata_file, [self.record], asset_root=self.root
        )

        stored = json_store.load(metadata_file, expected_type=dict)
        self.assertEqual(stored["schema_version"], 1)
        self.assertEqual(stored["versions"][0]["filepath"], "v001/hero_v001.ma")
        self.assertEqual(
            repository.load_version_records(metadata_file, asset_root=self.root),
            [self.record],
        )

    def test_metadata_uses_the_same_complete_version_parser(self):
        self.assertIs(metadata.VERSION_PATTERN, versioning.VERSION_PATTERN)
        for value in ('v001\n', 'v001\r\n', 'v1000000', 'v' + '0' * 31 + '1'):
            with self.subTest(value=value):
                self.assertIsNone(versioning.parse_version(value))
                with self.assertRaises(ValueError):
                    metadata.validate_version_record(
                        dict(self.record, version=value), asset_root=self.root)

    def test_supported_version_spellings_are_preserved_without_normalization(self):
        for value in ('v000', 'v1', 'v001', 'v999999', 'v' + '0' * 30 + '1'):
            with self.subTest(value=value):
                record = dict(self.record, version=value)
                document = metadata.encode_version_document([record], self.root)
                self.assertEqual(document['versions'][0]['version'], value)
                self.assertEqual(metadata.decode_version_document(document, self.root), [record])

    def test_bad_version_save_is_rejected_before_json_replacement(self):
        filename = str(Path(self.root) / '.asset')
        repository.save_version_records(filename, [self.record], asset_root=self.root)
        original = Path(filename).read_bytes()
        for value in ('v001\n', 'v1000000'):
            with self.subTest(value=value), patch.object(json_store, 'dump') as dump:
                with self.assertRaises(ValueError):
                    repository.save_version_records(
                        filename, [dict(self.record, version=value)], asset_root=self.root)
                dump.assert_not_called()
                self.assertEqual(Path(filename).read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
