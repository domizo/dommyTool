#!/usr/bin/env python3
"""Round-trip checks for the Maya-independent DAnim serializer."""

import ast
import contextlib
import functools
import io
import itertools
import math
import os
import sys
import tempfile
import unittest
import warnings
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

with warnings.catch_warnings():
    warnings.simplefilter('ignore', DeprecationWarning)
    from lib2to3.refactor import RefactoringTool, get_fixers_from_package


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / 'python'))

from DPyTools.features.animation import parser
from DPyTools.features.animation import serializer
from DPyTools.core.atomic_text import AtomicTextWriter
from DPyTools.core import bounded_text


@functools.lru_cache(maxsize=1)
def _writer_code():
    filename = REPO_ROOT / 'python/DPyTools/features/animation/io.py'
    converter = RefactoringTool(get_fixers_from_package('lib2to3.fixes'))
    converted = str(converter.refactor_string(filename.read_text(encoding='utf-8') + '\n', str(filename)))
    function = next(node for node in ast.parse(converted).body
                    if isinstance(node, ast.FunctionDef) and node.name == 'd_animWrite')
    function.decorator_list = []  # Selection restoration remains a separate native gate.
    module = ast.fix_missing_locations(ast.Module(body=[function], type_ignores=[]))
    return compile(module, str(filename), 'exec')


def _writer_with_queries(records, scene_path=''):
    warnings_seen = []
    animations = [record for record in records if isinstance(record, parser.AnimationRecord)]
    statics = [record for record in records if isinstance(record, parser.StaticRecord)]
    namespace = {
        'pm': SimpleNamespace(confirmDialog=lambda **kwargs: 'Yes',
                              select=lambda *args, **kwargs: None,
                              ls=lambda **kwargs: ['disposable_node'],
                              listConnections=lambda *args, **kwargs: ['curve'] if animations else [],
                              warning=warnings_seen.append),
        'cmds': SimpleNamespace(file=lambda **kwargs: scene_path),
        'os': os, 'AtomicTextWriter': AtomicTextWriter,
        'animation_parser': parser, 'animation_serializer': serializer,
        '_animation_records_from_curve': lambda *args: animations,
        '_static_records_from_node': lambda *args: statics,
    }
    exec(_writer_code(), namespace)
    return namespace['d_animWrite'], warnings_seen


class AnimationSerializerTests(unittest.TestCase):
    def _record(self, **overrides):
        values = {
            'attribute': 'translateX',
            'node': '|root|control',
            'has_parent': True,
            'weighted': True,
            'pre_infinity': 'constant',
            'post_infinity': 'linear',
            'times': [1, 2],
            'values': [2.5, 4],
            'in_tangents': ['linear', 'fixed'],
            'out_tangents': ['linear', 'fixed'],
            'tangent_locks': [False, True],
            'weight_locks': [False, True],
            'breakdown_times': [1],
            'in_angles': [0, 10],
            'in_weights': [0, 1],
            'out_angles': [0, 20],
            'out_weights': [0, 1],
        }
        values.update(overrides)
        return serializer.build_animation_record(**values)

    def test_animation_record_round_trips_through_parser(self):
        record = self._record()
        encoded = serializer.serialize_record(record)
        decoded = parser.parse_animation_lines(encoded.splitlines(True))

        self.assertEqual(decoded.records, (record,))
        self.assertTrue(decoded.records[0].keys[0].breakdown)
        self.assertEqual(decoded.records[0].keys[1].out_angle, 20.0)

    def test_numeric_time_tokens_match_breakdown_times(self):
        record = self._record(times=["1", "2"], breakdown_times=[1])
        self.assertTrue(record.keys[0].breakdown)

    def test_static_boolean_round_trips_without_numeric_coercion(self):
        record = serializer.build_static_record(
            'visibility', '|root|control', True, False)
        decoded = parser.parse_animation_lines(
            serializer.serialize_record(record).splitlines(True))

        self.assertEqual(decoded.records, (record,))
        self.assertIs(decoded.records[0].value, False)

    def test_parallel_query_length_mismatch_is_rejected(self):
        with self.assertRaises(serializer.AnimationSerializationError):
            self._record(values=[2.5])

        with self.assertRaises(serializer.AnimationSerializationError):
            self._record(breakdown_times=None, breakdown_flags=[True])

    def test_unsupported_tangent_is_rejected(self):
        with self.assertRaises(serializer.AnimationSerializationError):
            self._record(in_tangents=['custom', 'fixed'])

    def test_non_finite_values_are_rejected(self):
        for value in (math.nan, math.inf, -math.inf):
            with self.subTest(value=value):
                with self.assertRaises(serializer.AnimationSerializationError):
                    self._record(values=[value, 4])

    def test_manually_constructed_invalid_record_is_revalidated(self):
        record = parser.StaticRecord('visibility', 'node', False, math.nan)
        with self.assertRaises(serializer.AnimationSerializationError):
            serializer.serialize_record(record)

    def test_missing_fixed_tangent_fields_are_not_replaced_with_zero(self):
        record = self._record()
        for field in ('in_angle', 'in_weight', 'out_angle', 'out_weight'):
            key = record.keys[1]._replace(**{field: None})
            invalid = record._replace(keys=(record.keys[0], key))
            with self.subTest(field=field):
                with self.assertRaises(serializer.AnimationSerializationError):
                    serializer.serialize_record(invalid)

    def test_actual_writer_rejects_output_budget_before_replacing_file(self):
        statics = tuple(serializer.build_static_record(name, 'node', False, 1)
                        for name in ('tx', 'ty', 'tz'))
        cases = ((statics, {'MAX_RECORDS': 2}),
                 (statics, {'MAX_FILE_SIZE': 64}),
                 (statics, {'MAX_LINE_LENGTH': 16}),
                 ((self._record(),), {'MAX_KEYS': 1}))
        with tempfile.TemporaryDirectory() as directory:
            filename = Path(directory) / 'fixture.danim'
            for records, limits in cases:
                filename.write_bytes(b'previous output')
                writer, warnings_seen = _writer_with_queries(records)
                with self.subTest(limits=limits), patch.multiple(parser, **limits), \
                        contextlib.redirect_stdout(io.StringIO()):
                    self.assertFalse(writer(filename=str(filename)))
                self.assertTrue(warnings_seen)
                self.assertEqual(filename.read_bytes(), b'previous output')
                self.assertEqual([p.name for p in Path(directory).iterdir()], ['fixture.danim'])

    def test_actual_writer_round_trip_and_exact_total_budget(self):
        records = (self._record(), serializer.build_static_record('visibility', 'node', False, True))
        writer, warnings_seen = _writer_with_queries(records, scene_path='scene-\u5236\u5fa1.ma')
        with tempfile.TemporaryDirectory() as directory:
            filename = Path(directory) / 'fixture.danim'
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertTrue(writer(filename=str(filename)))
            payload = filename.read_bytes()
            self.assertEqual(parser.parse_animation_file(str(filename)).records, records)
            with patch.object(parser, 'MAX_FILE_SIZE', len(payload)), \
                    patch.object(parser, 'MAX_RECORDS', len(records)), \
                    patch.object(parser, 'MAX_KEYS', 2), \
                    contextlib.redirect_stdout(io.StringIO()):
                self.assertTrue(writer(filename=str(filename)))
                self.assertEqual(parser.parse_animation_file(str(filename)).records, records)
            with patch.object(parser, 'MAX_FILE_SIZE', len(payload) - 1), \
                    contextlib.redirect_stdout(io.StringIO()):
                self.assertFalse(writer(filename=str(filename)))
            self.assertEqual(filename.read_bytes(), payload)
        self.assertEqual(len(warnings_seen), 1)

    def test_serializer_rejects_single_record_beyond_read_budget(self):
        record = self._record()
        encoded = serializer.serialize_record(record)
        limits = ({'MAX_KEYS': 1}, {'MAX_FILE_SIZE': len(encoded.encode('utf-8')) - 1},
                  {'MAX_LINE_LENGTH': max(len(line.encode('utf-8'))
                                          for line in encoded.splitlines(True)) - 1})
        for values in limits:
            with self.subTest(limits=values), patch.multiple(parser, **values):
                with self.assertRaises(serializer.AnimationSerializationError):
                    serializer.serialize_record(record)

    def test_parallel_query_consumption_stops_at_key_budget(self):
        consumed = []

        def times():
            for value in range(5):
                consumed.append(value)
                yield value

        with patch.object(parser, 'MAX_KEYS', 2):
            with self.assertRaises(serializer.AnimationSerializationError):
                self._record(times=times())
        self.assertEqual(consumed, [0, 1, 2])

    def test_actual_writer_rejects_missing_fixed_fields_and_keeps_previous_file(self):
        original = self._record()
        invalid_key = original.keys[1]._replace(in_angle=None)
        invalid = original._replace(keys=(original.keys[0], invalid_key))
        writer, warnings_seen = _writer_with_queries((invalid,))
        with tempfile.TemporaryDirectory() as directory:
            filename = Path(directory) / 'fixture.danim'
            filename.write_bytes(b'previous output')
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertFalse(writer(filename=str(filename)))
            self.assertEqual(filename.read_bytes(), b'previous output')
            self.assertEqual([p.name for p in Path(directory).iterdir()], ['fixture.danim'])
        self.assertTrue(warnings_seen)

    def test_fixed_zero_is_valid_but_nonfixed_none_is_not_encoded(self):
        record = self._record(in_angles=[None, 0], in_weights=[None, 0],
                              out_angles=[None, 0], out_weights=[None, 0])
        output = serializer.serialize_record(record)
        self.assertEqual(parser.parse_animation_lines(output.splitlines(True)).records, (record,))
        self.assertNotIn('None', output)

    def test_streaming_writer_checks_remaining_global_keys_without_writing_second_record(self):
        record = self._record()
        stream = io.BytesIO()
        with patch.object(parser, 'MAX_KEYS', 3):
            writer = serializer.AnimationWriter(stream)
            writer.write_record(record)
            first_output = stream.getvalue()
            with self.assertRaises(serializer.AnimationSerializationError):
                writer.write_record(record._replace(node='other'))
            self.assertEqual(stream.getvalue(), first_output)
            with self.assertRaises(serializer.AnimationSerializationError):
                writer.finish()
        self.assertEqual(writer.record_count, 1)
        self.assertEqual(writer.key_count, 2)

    def test_streaming_write_failures_are_terminal_and_preserve_primary_error(self):
        primary = OSError('injected write failure')

        class BrokenStream:
            def write(self, payload):
                raise primary

        writer = serializer.AnimationWriter(BrokenStream())
        with self.assertRaises(OSError) as caught:
            writer.write_record(serializer.build_static_record('tx', 'node', False, 1))
        self.assertIs(caught.exception, primary)
        for operation in (lambda: writer.write_header('# comment\n'), writer.finish):
            with self.assertRaises(serializer.AnimationSerializationError):
                operation()

    def test_short_binary_write_cannot_be_reported_as_success(self):
        stream = SimpleNamespace(write=lambda payload: len(payload) - 1)
        writer = serializer.AnimationWriter(stream)
        with self.assertRaisesRegex(IOError, 'incomplete write'):
            writer.write_record(serializer.build_static_record('tx', 'node', False, 1))
        with self.assertRaises(serializer.AnimationSerializationError):
            writer.finish()

    def test_header_shape_failure_and_empty_finalization_are_terminal(self):
        record = serializer.build_static_record('tx', 'node', False, 1)
        for header in ('# missing final newline', 'static tx tx node 0 1\n', None):
            writer = serializer.AnimationWriter(io.BytesIO())
            with self.subTest(header=header):
                with self.assertRaises(serializer.AnimationSerializationError):
                    writer.write_header(header)
                with self.assertRaises(serializer.AnimationSerializationError):
                    writer.write_record(record)
        writer = serializer.AnimationWriter(io.BytesIO())
        writer.write_header('# only comments\n')
        with self.assertRaises(serializer.AnimationSerializationError):
            writer.finish()
        with self.assertRaises(serializer.AnimationSerializationError):
            writer.write_record(record)

    def test_successful_finish_is_terminal_and_legacy_none_write_result_is_supported(self):
        payloads = []
        stream = SimpleNamespace(write=lambda payload: payloads.append(payload))
        writer = serializer.AnimationWriter(stream)
        record = serializer.build_static_record('tx', 'node', False, 1)
        writer.write_header('# header\n')
        writer.write_record(record)
        self.assertEqual(writer.finish(), 1)
        self.assertTrue(all(isinstance(payload, bytes) for payload in payloads))
        decoded = parser.parse_animation_lines(b''.join(payloads).decode('utf-8').splitlines(True))
        self.assertEqual(decoded.records, (record,))
        with self.assertRaises(serializer.AnimationSerializationError):
            writer.write_record(record)

    def test_chunk_budget_handles_utf8_split_lines_and_failed_admission_without_state_change(self):
        budget = bounded_text.TextBudget(16, 5, serializer.AnimationSerializationError)
        self.assertEqual(budget.admit('\u3042'), '\u3042'.encode('utf-8'))
        self.assertEqual((budget.total_bytes, budget.line_bytes), (3, 3))
        with self.assertRaises(serializer.AnimationSerializationError):
            budget.admit('abc')
        self.assertEqual((budget.total_bytes, budget.line_bytes), (3, 3))
        self.assertEqual(budget.admit('b\n'), b'b\n')
        self.assertEqual(budget.admit(b'ab\ncd\n'), b'ab\ncd\n')
        self.assertEqual((budget.total_bytes, budget.line_bytes), (11, 0))
        with self.assertRaises(serializer.AnimationSerializationError):
            budget.admit(b'123456')
        self.assertEqual((budget.total_bytes, budget.line_bytes), (11, 0))

    def test_generator_keys_stop_at_remaining_budget_before_field_expansion(self):
        record = self._record()
        consumed = []

        def keys():
            for key in record.keys * 3:
                consumed.append(key)
                yield key

        with patch.object(parser, 'MAX_KEYS', 2):
            with self.assertRaises(serializer.AnimationSerializationError):
                serializer.serialize_record(record._replace(keys=keys()))
        self.assertEqual(len(consumed), 3)

    def test_nonfinite_and_overflowing_fixed_fields_are_rejected(self):
        record = self._record()
        for value in (math.nan, math.inf, 10 ** 1000):
            invalid = record._replace(keys=(record.keys[1]._replace(in_angle=value),))
            with self.subTest(value_type=type(value).__name__):
                with self.assertRaises(serializer.AnimationSerializationError):
                    serializer.serialize_record(invalid)

    def test_header_and_token_limits_precede_expensive_split_operations(self):
        class LargeText(str):
            def split(self, *args, **kwargs):
                raise AssertionError('Overlong token should not be split')

            def splitlines(self, *args, **kwargs):
                raise AssertionError('Header must be processed without a full line list')

        with patch.object(parser, 'MAX_LINE_LENGTH', 8):
            with self.assertRaises(serializer.AnimationSerializationError):
                serializer.build_static_record(LargeText('x' * 9), 'node', False, 1)
        with patch.object(parser, 'MAX_LINE_LENGTH', 8):
            writer = serializer.AnimationWriter(io.BytesIO())
            with self.assertRaises(serializer.AnimationSerializationError):
                writer.write_header(LargeText('#' + 'x' * 20 + '\n'))
        stream = io.BytesIO()
        writer = serializer.AnimationWriter(stream)
        writer.write_header(LargeText('# one\n# two\n'))
        self.assertEqual(stream.getvalue(), b'# one\n# two\n')

    def test_chunk_budget_matches_independent_byte_line_reference(self):
        for count in range(5):
            for characters in itertools.product(('a', '\u3042', '\n'), repeat=count):
                text = ''.join(characters)
                raw = text.encode('utf-8')
                lengths = [len(part) + 1 for part in raw.split(b'\n')[:-1]]
                lengths.append(len(raw.split(b'\n')[-1]))
                for total_limit, line_limit in itertools.product((1, 4, 8, 16), repeat=2):
                    valid = len(raw) <= total_limit and max(lengths) <= line_limit
                    for cut in range(len(text) + 1):
                        budget = bounded_text.TextBudget(total_limit, line_limit,
                                                         serializer.AnimationSerializationError)
                        try:
                            first, second = budget.admit(text[:cut]), budget.admit(text[cut:])
                        except serializer.AnimationSerializationError:
                            self.assertFalse(valid, (text, cut, total_limit, line_limit))
                        else:
                            self.assertTrue(valid, (text, cut, total_limit, line_limit))
                            self.assertEqual(first + second, raw)

    def test_actual_writer_keeps_previous_file_after_mid_record_memory_error(self):
        record = self._record()
        original_lines = serializer._animation_lines

        def interrupted_lines(validated):
            yield next(original_lines(validated))
            raise MemoryError('injected serialization allocation failure')

        writer, warnings_seen = _writer_with_queries((record,))
        with tempfile.TemporaryDirectory() as directory:
            filename = Path(directory) / 'fixture.danim'
            filename.write_bytes(b'previous output')
            with patch.object(serializer, '_animation_lines', side_effect=interrupted_lines), \
                    contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaises(MemoryError):
                    writer(filename=str(filename))
            self.assertEqual(filename.read_bytes(), b'previous output')
            self.assertEqual([p.name for p in Path(directory).iterdir()], ['fixture.danim'])
        self.assertEqual(warnings_seen, [])


if __name__ == '__main__':
    unittest.main()
