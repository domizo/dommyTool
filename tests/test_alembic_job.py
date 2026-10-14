#!/usr/bin/env python3
"""Unit tests for safe, Maya-independent Alembic job construction."""

import os
import sys
import tempfile
import time
import unittest
from fractions import Fraction
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock

from test_asset_browser import methods


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.pipeline.io import alembic_job
from DPyTools.core.components import component_index


class AlembicJobTests(unittest.TestCase):
    def test_roots_and_filepaths_are_quoted(self):
        job = alembic_job.build_export_job(
            1,
            120,
            0.5,
            "/project/cache/hero mesh.abc",
            roots=["|root group|hero"],
            flags=["-worldSpace", "-uvWrite"],
            data_format="ogawa",
        )
        self.assertIn('-root "|root group|hero"', job)
        self.assertIn('-file "/project/cache/hero mesh.abc"', job)
        self.assertIn("-dataFormat ogawa", job)

    def test_parser_control_characters_are_rejected(self):
        for value in ('bad"name', "bad\nname", "bad\x00name"):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    alembic_job.quote_argument(value)

    def test_frame_range_and_step_are_validated(self):
        with self.assertRaises(ValueError):
            alembic_job.build_export_job(2, 1, 1, "/tmp/a.abc")
        with self.assertRaises(ValueError):
            alembic_job.build_export_job(1, 2, 0, "/tmp/a.abc")
        with self.assertRaises(ValueError):
            alembic_job.build_export_job(1, float("inf"), 1, "/tmp/a.abc")

    def test_unknown_flags_and_conflicting_selection_are_rejected(self):
        with self.assertRaises(ValueError):
            alembic_job.build_export_job(
                1, 2, 1, "/tmp/a.abc", flags=["-injected"]
            )
        with self.assertRaises(ValueError):
            alembic_job.build_export_job(
                1, 2, 1, "/tmp/a.abc", roots=["|root"], selection=True
            )

    def test_fraction_values_emit_normalized_decimal_tokens(self):
        job = alembic_job.build_export_job(Fraction(1, 2), Fraction(5, 2),
                                          Fraction(1, 4), '/tmp/a.abc')
        self.assertEqual(job, '-frameRange 0.5 2.5 -step 0.25 -file "/tmp/a.abc"')

    def test_numeric_string_overrides_cannot_add_job_options(self):
        class OddFloat(float):
            def __str__(self):
                raise AssertionError('caller-controlled float text must not be used')

        class OddInt(int):
            def __str__(self):
                return '1 -root "|unexpected_root"'

        job = alembic_job.build_export_job(OddInt(1), OddFloat(2), OddFloat(0.5), '/tmp/a.abc')
        self.assertEqual(job, '-frameRange 1.0 2.0 -step 0.5 -file "/tmp/a.abc"')
        self.assertNotIn('unexpected_root', job)

    def test_each_numeric_field_rejects_nonreal_and_nonfinite_values(self):
        for index in range(3):
            for value, error in ((True, TypeError), ('1', TypeError), (None, TypeError),
                                 (1j, TypeError), (float('nan'), ValueError),
                                 (float('inf'), ValueError), (10 ** 400, ValueError)):
                with self.subTest(index=index, value=value):
                    arguments = [1, 2, 1]
                    arguments[index] = value
                    with self.assertRaises(error):
                        alembic_job.build_export_job(*arguments, filepath='/tmp/a.abc')

    def test_numeric_values_are_converted_only_once_per_field(self):
        calls = []

        class Counted(float):
            def __float__(self):
                calls.append(float.__float__(self))
                return float.__float__(self)

        alembic_job.build_export_job(Counted(1), Counted(2), Counted(0.5), '/tmp/a.abc')
        self.assertEqual(calls, [1, 2, 0.5])

    def test_selection_requires_explicit_boolean(self):
        for value in ('false', 'true', 0, 1, None, [], {}):
            with self.subTest(value=value), self.assertRaises(TypeError):
                alembic_job.build_export_job(1, 2, 1, '/tmp/a.abc', selection=value)
        job = alembic_job.build_export_job(1, 2, 1, '/tmp/a.abc', selection=True)
        self.assertIn(' -selection ', job)

    def test_flag_container_and_item_types_are_explicit(self):
        for value in ('-verbose', {'-verbose'}, {'-verbose': True}, [None], [[]], [1]):
            with self.subTest(value=value), self.assertRaises(TypeError):
                alembic_job.build_export_job(1, 2, 1, '/tmp/a.abc', flags=value)

    def test_admitted_flag_order_and_deduplication_are_retained(self):
        job = alembic_job.build_export_job(1, 2, 1, '/tmp/a.abc',
                                          flags=('-uvWrite', '-verbose', '-uvWrite'))
        self.assertEqual(job, '-frameRange 1.0 2.0 -step 1.0 -uvWrite -verbose -file "/tmp/a.abc"')

    def test_data_format_admission_and_root_object_compatibility(self):
        for value in (True, 1, [], {}):
            with self.assertRaises(TypeError):
                alembic_job.build_export_job(1, 2, 1, '/tmp/a.abc', data_format=value)
        with self.assertRaises(ValueError):
            alembic_job.build_export_job(1, 2, 1, '/tmp/a.abc', data_format='unsupported')

        class RootObject:
            def __str__(self):
                return '|root group|hero'

        job = alembic_job.build_export_job(1, 2, 1, r'C:\cache\hero.abc', roots=[RootObject()])
        self.assertIn('-root "|root group|hero"', job)
        self.assertIn('-file "C:/cache/hero.abc"', job)


class AlembicExportAdapterTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='dommy-alembic-')
        self.addCleanup(self.temporary.cleanup)
        self.frames = (1, 2)
        self.step = 1
        self.pm = SimpleNamespace(
            textField=Mock(return_value=os.path.join(self.temporary.name, 'output.abc')),
            warning=Mock(), waitCursor=Mock(), optionMenuGrp=Mock(return_value=1),
            checkBox=Mock(return_value=False),
            intFieldGrp=Mock(side_effect=lambda control, **kwargs:
                             self.frames[0 if kwargs.get('value1') else 1]),
            floatSliderGrp=Mock(side_effect=lambda *args, **kwargs: self.step))
        self.cmds = SimpleNamespace(DExportAbcCmd=Mock())
        self.scope = methods('python/DPyTools/pipeline/io/alembic.py', 'ddata_export',
                             {'pm': self.pm, 'cmds': self.cmds, 'os': os, 'time': time,
                              'alembic_job': alembic_job, 'component_index': component_index}, ['exportRun'])
        self.owner = SimpleNamespace(exportFileField='file', selectedCheck='selection',
                                     isVerbose='verbose', frameRange='frames', frameStep='step',
                                     ogawaCheckBox='format')

    def run_export(self):
        return self.scope['exportRun'](self.owner)

    def assert_cursor_released(self):
        self.assertEqual([call.kwargs for call in self.pm.waitCursor.call_args_list],
                         [{'state': True}, {'state': False}])

    def test_numeric_type_error_warns_without_calling_native_export(self):
        self.frames = ('1', 2)
        self.run_export()
        self.cmds.DExportAbcCmd.assert_not_called()
        self.pm.warning.assert_called_once()
        self.assert_cursor_released()

    def test_numeric_overflow_warns_without_calling_native_export(self):
        self.frames = (1, 10 ** 400)
        self.run_export()
        self.cmds.DExportAbcCmd.assert_not_called()
        self.pm.warning.assert_called_once()
        self.assert_cursor_released()

    def test_success_uses_normalized_job_and_releases_cursor(self):
        self.step = Fraction(1, 2)
        self.run_export()
        self.assertIn('-frameRange 1.0 2.0 -step 0.5', self.cmds.DExportAbcCmd.call_args.kwargs['j'])
        self.pm.warning.assert_not_called()
        self.assert_cursor_released()

    def test_native_interruption_propagates_and_releases_cursor(self):
        self.cmds.DExportAbcCmd.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            self.run_export()
        self.pm.warning.assert_not_called()
        self.assert_cursor_released()

    def test_invalid_scope_cannot_fall_back_to_all_objects(self):
        for mode in (0, 4, -1, None, True, False, 1.0, '1', [], 10 ** 400):
            with self.subTest(mode=mode):
                self.setUp()
                self.pm.optionMenuGrp.return_value = mode
                self.run_export()
                self.cmds.DExportAbcCmd.assert_not_called()
                self.pm.warning.assert_called_once()
                self.assert_cursor_released()

    def test_supported_scopes_keep_explicit_all_roots_or_selection(self):
        for mode in (1, 2, 3):
            self.setUp()
            self.pm.optionMenuGrp.return_value = mode
            self.pm.ls = Mock(return_value=['|root'])
            self.run_export()
            job = self.cmds.DExportAbcCmd.call_args.kwargs['j']
            self.assertEqual('-root "|root"' in job, mode == 2)
            self.assertEqual('-selection' in job, mode == 3)
            self.pm.warning.assert_not_called()
            self.assert_cursor_released()

    def test_empty_selected_scope_never_exports_everything(self):
        for mode in (2, 3):
            self.setUp()
            self.pm.optionMenuGrp.return_value = mode
            self.pm.ls = Mock(return_value=[])
            self.run_export()
            self.cmds.DExportAbcCmd.assert_not_called()
            self.pm.warning.assert_called_once()
            self.assert_cursor_released()


if __name__ == "__main__":
    unittest.main()
