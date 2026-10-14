#!/usr/bin/env python3
"""Host-free export admission before native files or scene edits."""

import tempfile
import unittest
from pathlib import Path

from test_python_rig_safety import functions
from DPyTools.pipeline.assets import export_plan


class ExportPlanTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.options = dict(asset_root=str(self.root), export_path=str(self.root / 'v001'),
                            asset_name='character', extension='.ma', timerange=[1, 24],
                            creator='担当者', comment='archived work', date='2019/01/01', thumbnail=True)
    def plan(self, **updates):
        options = dict(self.options, **updates)
        return export_plan.build(**options)
    def test_scene_and_cache_layout_use_the_same_reserved_version(self):
        for extension in ('.ma', '.mb', '.abc', '.anim', '.danim'):
            plan = self.plan(extension=extension)
            record = plan['record']
            self.assertEqual(record['filename'], 'character_v001' + extension)
            self.assertEqual(Path(record['filepath']).parent.name, 'scenes' if extension in ('.ma', '.mb') else 'v001')
            self.assertEqual(plan['timerange'], (1.0, 24.0))
            self.assertFalse((self.root / 'v001').exists())
    def test_disabled_thumbnail_is_empty_and_unicode_names_remain_supported(self):
        self.assertEqual(self.plan(thumbnail=False, asset_name='人物')['record']['thumbnail'], '')
    def test_invalid_time_ranges_fail_without_directory_creation(self):
        for value in (None, [1], [2, 1], [True, 3], ['1', 3], [1, float('inf')], [1, 10 ** 400]):
            with self.subTest(value=value), self.assertRaises((TypeError, ValueError)): self.plan(timerange=value)
        self.assertFalse((self.root / 'v001').exists())
    def test_layout_refuses_outside_nested_or_invalid_version_paths(self):
        for path in (self.root.parent / 'v001', self.root / 'sub' / 'v001', self.root / 'invalid'):
            with self.assertRaises(ValueError): self.plan(export_path=str(path))
        with self.assertRaises(ValueError): self.plan(export_path='relative/v001')
    def test_missing_symlink_roots_and_occupied_versions_fail_closed(self):
        with self.assertRaises(IOError): self.plan(asset_root=str(self.root / 'missing'))
        alias = self.root / 'alias'
        alias.symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(IOError): self.plan(asset_root=str(alias), export_path=str(alias / 'v001'))
        (self.root / 'v001').mkdir()
        with self.assertRaises(IOError): self.plan()
    def test_names_formats_and_record_fields_validate_before_mutation(self):
        for updates in ({'asset_name': '../escape'}, {'asset_name': 'x' * 250}, {'extension': '.exe'},
                        {'creator': None}, {'comment': 'bad\x00data'}, {'thumbnail': 1}):
            with self.subTest(updates=updates), self.assertRaises((TypeError, ValueError)): self.plan(**updates)
        self.assertFalse((self.root / 'v001').exists())
    def test_nonportable_export_names_are_rejected_before_directory_creation(self):
        for name in ('CON', 'name.', 'bad:name', 'a?b', 'aux.txt'):
            with self.assertRaises(ValueError): self.plan(asset_name=name)
        self.assertFalse((self.root / 'v001').exists())


if __name__ == '__main__': unittest.main()
