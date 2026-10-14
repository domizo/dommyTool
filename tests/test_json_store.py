#!/usr/bin/env python3
"""Unit tests for durable JSON persistence."""

import errno
import json
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))

from DPyTools.core import file_publish, json_store


class JsonStoreTests(unittest.TestCase):
    def test_windows_move_flags_distinguish_replace_and_no_replace(self):
        calls = []

        class MoveFile(object):
            def __call__(self, source, destination, flags):
                calls.append((source, destination, flags))
                return 1

        function = MoveFile()
        dll = SimpleNamespace(kernel32=SimpleNamespace(MoveFileExW=function))
        source, destination = u'payload-\u03b4.tmp', u'output-\u03b4.png'
        with patch.object(file_publish.ctypes, 'windll', dll, create=True):
            json_store._move_file_windows(source, destination, True)
            json_store._move_file_windows(source, destination, False)
        self.assertEqual(calls, [(source, destination, 0x9), (source, destination, 0x8)])
        self.assertEqual(len(function.argtypes), 3)
        self.assertIs(function.restype, file_publish.ctypes.c_int)

    def test_windows_move_failure_is_reported(self):
        class MoveFile(object):
            def __call__(self, unused_source, unused_destination, unused_flags):
                return 0

        dll = SimpleNamespace(kernel32=SimpleNamespace(MoveFileExW=MoveFile()))
        failure = OSError('native move failed')
        with patch.object(file_publish.ctypes, 'windll', dll, create=True), \
                patch.object(file_publish.ctypes, 'WinError', return_value=failure, create=True):
            with self.assertRaises(OSError) as caught:
                json_store._move_file_windows(u'source.tmp', u'destination.png', False)
        self.assertIs(caught.exception, failure)

    def setUp(self):
        self.temporary_directory = tempfile.mkdtemp(prefix="dommytool-json-")
        self.filename = os.path.join(self.temporary_directory, "settings.json")

    def tearDown(self):
        shutil.rmtree(self.temporary_directory, ignore_errors=True)

    def test_round_trip_and_replace(self):
        json_store.dump(self.filename, {"version": 1})
        json_store.dump(self.filename, {"version": 2, "name": "テスト"})

        self.assertEqual(
            json_store.load(self.filename, expected_type=dict),
            {"version": 2, "name": "テスト"})

    def test_no_replace_creates_only_an_absent_file(self):
        json_store.dump(self.filename, {'version': 1}, replace_existing=False)
        original = Path(self.filename).read_bytes()
        with self.assertRaises(OSError):
            json_store.dump(self.filename, {'version': 2}, replace_existing=False)
        self.assertEqual(Path(self.filename).read_bytes(), original)
        self.assertEqual(os.listdir(self.temporary_directory), ['settings.json'])

    def test_no_replace_preserves_unreadable_or_invalid_existing_payload(self):
        original = b'not JSON; must not be reset'
        Path(self.filename).write_bytes(original)
        with self.assertRaises(OSError):
            json_store.dump(self.filename, {'defaults': True}, replace_existing=False)
        self.assertEqual(Path(self.filename).read_bytes(), original)
        self.assertEqual(os.listdir(self.temporary_directory), ['settings.json'])

    def test_no_replace_refuses_file_appearing_during_encoding(self):
        original_chunks = json_store._output_chunks
        original = b'concurrent output'

        def chunks(encoder, data):
            Path(self.filename).write_bytes(original)
            return original_chunks(encoder, data)

        with patch.object(json_store, '_output_chunks', side_effect=chunks):
            with self.assertRaises(OSError):
                json_store.dump(self.filename, {'version': 1}, replace_existing=False)
        self.assertEqual(Path(self.filename).read_bytes(), original)
        self.assertEqual(os.listdir(self.temporary_directory), ['settings.json'])

    def test_replacement_policy_is_boolean_and_checked_before_staging(self):
        with patch.object(json_store, 'StagedFile') as staging:
            for value in (0, 1, None, 'false'):
                with self.assertRaises(TypeError):
                    json_store.dump(self.filename, {}, replace_existing=value)
        staging.assert_not_called()
        self.assertEqual(os.listdir(self.temporary_directory), [])

    def test_missing_default_and_type_validation(self):
        self.assertEqual(json_store.load(self.filename, default=[]), [])
        with self.assertRaises(TypeError):
            json_store.load(self.filename, default=[], expected_type=dict)

    def test_serialization_error_preserves_existing_file(self):
        json_store.dump(self.filename, {"stable": True})
        with self.assertRaises(TypeError):
            json_store.dump(self.filename, {"invalid": object()})
        self.assertEqual(json_store.load(self.filename), {"stable": True})

    def test_oversized_input_is_rejected_before_parsing(self):
        Path(self.filename).write_text('{"payload": "0123456789"}')
        with self.assertRaises(ValueError):
            json_store.load(self.filename, max_file_size=8)
        with self.assertRaises((TypeError, ValueError)):
            json_store.load(self.filename, max_file_size=0)

    def test_oversized_output_does_not_create_a_file(self):
        with self.assertRaises(ValueError):
            json_store.dump(
                self.filename, {"payload": "0123456789"}, max_file_size=8
            )
        self.assertFalse(os.path.exists(self.filename))

    def test_duplicate_keys_are_rejected_after_escape_decoding(self):
        for payload in (
                '{"opt_in": false, "opt_in": true}',
                '{"nested": {"name": 1, "na\\u006de": 2}}'):
            with self.subTest(payload=payload):
                Path(self.filename).write_text(payload, encoding='utf-8')
                with self.assertRaisesRegex(ValueError, 'Duplicate'):
                    json_store.load(self.filename)

    def test_non_finite_input_numbers_are_rejected(self):
        for token in ('NaN', 'Infinity', '-Infinity', '1e9999', '-1e9999'):
            with self.subTest(token=token):
                Path(self.filename).write_text('{"value":' + token + '}')
                with self.assertRaises(ValueError):
                    json_store.load(self.filename)
        Path(self.filename).write_text('{"value":1.25e2}')
        self.assertEqual(json_store.load(self.filename), {'value': 125.0})

    def test_deep_input_is_rejected_before_decoder_allocation(self):
        Path(self.filename).write_text('[' * 2000 + '0' + ']' * 2000)
        with patch.object(json_store.json, 'loads') as decoder:
            with self.assertRaisesRegex(ValueError, 'nesting'):
                json_store.load(self.filename)
        decoder.assert_not_called()

    def test_nesting_boundary_ignores_escaped_string_delimiters(self):
        text = 'brackets [[{{}}]]; quote " and backslash \\; Unicode 口'
        data = text
        for unused_index in range(64):
            data = [data]
        payload = json.dumps(data, ensure_ascii=False)
        Path(self.filename).write_text(payload, encoding='utf-8')
        self.assertEqual(json_store.load(self.filename), data)
        Path(self.filename).write_text('[' + payload + ']', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'nesting'):
            json_store.load(self.filename)

    def test_decoder_recursion_failure_is_a_ui_compatible_value_error(self):
        Path(self.filename).write_text('{"valid":true}')
        with patch.object(json_store.json, 'loads', side_effect=RuntimeError('recursion')):
            with self.assertRaisesRegex(ValueError, 'nesting'):
                json_store.load(self.filename)

    def test_invalid_syntax_and_utf8_remain_rejected(self):
        for payload in (b'{"value": ]}', b'{"value": "\xff"}', b'[{]}', b'"unterminated'):
            with self.subTest(payload=payload):
                Path(self.filename).write_bytes(payload)
                with self.assertRaises(ValueError):
                    json_store.load(self.filename)

    def test_streamed_output_matches_standard_format_and_exact_byte_limits(self):
        values = (
            None, True, -12, 1.25, [], (), {},
            {'name': '口"\\\n', 'values': [0, False, None, 2.5]},
            {1: 'integer key', 2.5: 'float key', False: 'boolean key', None: 'null key'},
        )
        for value in values:
            for indent in (None, -1, 0, 4):
                for ensure_ascii in (True, False):
                    with self.subTest(value=value, indent=indent, ascii=ensure_ascii):
                        payload = (json.dumps(
                            value, indent=indent, ensure_ascii=ensure_ascii) + '\n').encode('utf-8')
                        json_store.dump(
                            self.filename, value, indent=indent, ensure_ascii=ensure_ascii,
                            max_file_size=len(payload))
                        self.assertEqual(Path(self.filename).read_bytes(), payload)
                        self.assertEqual(json_store.load(self.filename), json.loads(payload))
                        with self.assertRaises(ValueError):
                            json_store.dump(
                                self.filename, value, indent=indent,
                                ensure_ascii=ensure_ascii, max_file_size=len(payload) - 1)
                        self.assertEqual(Path(self.filename).read_bytes(), payload)
                        self.assertEqual(os.listdir(self.temporary_directory), ['settings.json'])

    def test_dump_does_not_materialize_the_whole_document(self):
        with patch.object(json_store.json, 'dumps', side_effect=AssertionError('whole payload')):
            json_store.dump(self.filename, {'values': list(range(200))})
        self.assertEqual(json_store.load(self.filename), {'values': list(range(200))})

    def test_large_scalar_depth_and_indent_fail_before_encoder_or_staging(self):
        deep = 0
        for unused_index in range(65):
            deep = [deep]
        cycle = []
        cycle.append(cycle)
        cases = (
            ({'text': 'x' * 65536}, {}),
            ({'text': '\x00' * 100}, {}),
            ({'text': '\U0001f600' * 64}, {}),
            ({'number': 1 << 65536}, {}),
            (deep, {}), (cycle, {}),
            ({'value': 1}, {'indent': 65536}),
        )
        for data, options in cases:
            with self.subTest(options=options), \
                    patch.object(json_store.json, 'JSONEncoder') as encoder, \
                    patch.object(json_store, 'StagedFile') as stage:
                with self.assertRaises(ValueError):
                    json_store.dump(self.filename, data, max_file_size=128, **options)
                encoder.assert_not_called()
                stage.assert_not_called()

    def test_output_non_finite_values_and_key_collisions_are_rejected(self):
        json_store.dump(self.filename, {'stable': True})
        original = Path(self.filename).read_bytes()
        cases = (
            {'value': float('nan')}, {'value': float('inf')},
            {float('-inf'): 1}, {1: 'first', '1': 'second'},
            {False: 'first', 'false': 'second'}, {None: 1, 'null': 2},
            {1.25: 1, '1.25': 2},
            {'\U0001f600': 1, '\ud83d\ude00': 2},
            {'nested': {1: 'first', '1': 'second'}},
        )
        for data in cases:
            with self.subTest(data=data):
                with self.assertRaises(ValueError):
                    json_store.dump(self.filename, data)
                self.assertEqual(Path(self.filename).read_bytes(), original)
                self.assertEqual(os.listdir(self.temporary_directory), ['settings.json'])

    def test_shared_containers_are_not_mistaken_for_cycles(self):
        shared = {'value': [1, 2, 3]}
        data = [shared, shared]
        json_store.dump(self.filename, data)
        self.assertEqual(json_store.load(self.filename), data)

    def test_output_depth_boundary_round_trips(self):
        data = 'value'
        for unused_index in range(64):
            data = {'child': data}
        json_store.dump(self.filename, data)
        self.assertEqual(json_store.load(self.filename), data)

    def test_stream_budget_stops_consumption_and_retains_previous_file(self):
        json_store.dump(self.filename, {'stable': True})
        consumed = []

        def chunks(unused_data):
            for index in range(1000):
                consumed.append(index)
                yield 'x' * 32

        encoder = Mock()
        encoder.iterencode.side_effect = chunks
        with patch.object(json_store.json, 'JSONEncoder', return_value=encoder):
            with self.assertRaises(ValueError):
                json_store.dump(self.filename, {'value': 0}, max_file_size=64)
        self.assertEqual(consumed, [0, 1, 2])
        self.assertEqual(json_store.load(self.filename), {'stable': True})
        self.assertEqual(os.listdir(self.temporary_directory), ['settings.json'])

    def test_mid_stream_encoder_failure_discards_staging(self):
        json_store.dump(self.filename, {'stable': True})
        primary = ValueError('encoder failed')

        def chunks(unused_data):
            yield '{'
            raise primary

        encoder = Mock()
        encoder.iterencode.side_effect = chunks
        with patch.object(json_store.json, 'JSONEncoder', return_value=encoder):
            with self.assertRaises(ValueError) as caught:
                json_store.dump(self.filename, {'value': 0})
        self.assertIs(caught.exception, primary)
        self.assertEqual(json_store.load(self.filename), {'stable': True})
        self.assertEqual(os.listdir(self.temporary_directory), ['settings.json'])

    def test_short_or_failed_stream_writes_do_not_publish(self):
        json_store.dump(self.filename, {'stable': True})
        original_stage = json_store.StagedFile
        primary = OSError('write failed')
        for failure in (0, primary, RuntimeError('stream failed'),
                        MemoryError('allocation failed'), KeyboardInterrupt()):
            with self.subTest(failure=failure):
                def allocate(*args, **kwargs):
                    staged = original_stage(*args, **kwargs)
                    wrapped = Mock(wraps=staged.stream)
                    if isinstance(failure, BaseException):
                        wrapped.write.side_effect = failure
                    else:
                        wrapped.write.return_value = failure
                    staged.stream = wrapped
                    return staged

                with patch.object(json_store, 'StagedFile', side_effect=allocate):
                    with self.assertRaises((IOError, RuntimeError, MemoryError, KeyboardInterrupt)) as caught:
                        json_store.dump(self.filename, {'value': 0})
                if isinstance(failure, BaseException):
                    self.assertIs(caught.exception, failure)
                self.assertEqual(json_store.load(self.filename), {'stable': True})
                self.assertEqual(os.listdir(self.temporary_directory), ['settings.json'])

    def test_python2_none_write_result_remains_supported(self):
        original_stage = json_store.StagedFile

        def allocate(*args, **kwargs):
            staged = original_stage(*args, **kwargs)
            original_write = staged.stream.write
            wrapped = Mock(wraps=staged.stream)

            def write(value):
                original_write(value)
                return None

            wrapped.write.side_effect = write
            staged.stream = wrapped
            return staged

        with patch.object(json_store, 'StagedFile', side_effect=allocate):
            json_store.dump(self.filename, {'value': '口'}, ensure_ascii=False)
        self.assertEqual(json_store.load(self.filename), {'value': '口'})

    def test_string_preflight_matches_standard_escape_size_reference(self):
        code_points = list(range(256)) + [0x7ff, 0x800, 0xd7ff, 0xe000, 0xffff, 0x10000, 0x1f600, 0x10ffff]
        for code_point in code_points:
            text = 'prefix' + chr(code_point) + 'suffix'
            for ensure_ascii in (True, False):
                for data in (text, {text: text}, [text, text]):
                    with self.subTest(code_point=code_point, ascii=ensure_ascii):
                        size = len((json.dumps(data, ensure_ascii=ensure_ascii) + '\n').encode('utf-8'))
                        json_store._validate_output(data, size, None, ensure_ascii)
            for ensure_ascii in (True, False):
                expected = len(json.dumps(text, ensure_ascii=ensure_ascii).encode('utf-8'))
                self.assertEqual(json_store._string_size(text, expected, ensure_ascii), expected)
                with self.assertRaises(ValueError):
                    json_store._string_size(text, expected - 1, ensure_ascii)

    def test_unpaired_surrogates_require_ascii_escaping(self):
        json_store.dump(self.filename, {'text': '\ud800'}, ensure_ascii=True)
        original = Path(self.filename).read_bytes()
        self.assertEqual(json_store.load(self.filename), {'text': '\ud800'})
        with self.assertRaises(ValueError):
            json_store.dump(self.filename, {'text': '\ud800'}, ensure_ascii=False)
        self.assertEqual(Path(self.filename).read_bytes(), original)

    def test_encoder_recursion_is_normalized_but_memory_failure_is_preserved(self):
        json_store.dump(self.filename, {'stable': True})
        for primary in (RuntimeError('recursion'), MemoryError('allocation failed')):
            def chunks(unused_data):
                yield '{'
                raise primary

            encoder = Mock()
            encoder.iterencode.side_effect = chunks
            expected = ValueError if isinstance(primary, RuntimeError) else MemoryError
            with patch.object(json_store.json, 'JSONEncoder', return_value=encoder):
                with self.assertRaises(expected) as caught:
                    json_store.dump(self.filename, {'value': 0})
            if isinstance(primary, MemoryError):
                self.assertIs(caught.exception, primary)
            self.assertEqual(json_store.load(self.filename), {'stable': True})
            self.assertEqual(os.listdir(self.temporary_directory), ['settings.json'])

    def test_unused_large_indentation_is_not_allocated_for_scalar_or_empty_output(self):
        for value in (0, [], (), {}):
            payload = (json.dumps(value, indent=65536) + '\n').encode('utf-8')
            original_encoder = json_store.json.JSONEncoder
            with patch.object(json_store.json, 'JSONEncoder', wraps=original_encoder) as encoder:
                json_store.dump(self.filename, value, indent=65536, max_file_size=len(payload))
            self.assertEqual(encoder.call_args.kwargs['indent'], 0)
            self.assertEqual(Path(self.filename).read_bytes(), payload)

    def test_symbolic_link_destination_is_not_replaced(self):
        target = os.path.join(self.temporary_directory, "target.json")
        Path(target).write_text('{"stable": true}')
        try:
            os.symlink(target, self.filename)
        except (AttributeError, NotImplementedError, OSError):
            self.skipTest("Symbolic links are not available")
        with self.assertRaises(OSError):
            json_store.dump(self.filename, {"stable": False})
        self.assertEqual(json_store.load(target), {"stable": True})

    def test_fdopen_and_raw_close_failure_keep_primary_and_attempt_unlink(self):
        json_store.dump(self.filename, {'stable': True})
        primary = OSError('primary fdopen failure')
        descriptors = []
        original_mkstemp, original_close = tempfile.mkstemp, os.close

        def allocate(*args, **kwargs):
            result = original_mkstemp(*args, **kwargs)
            descriptors.append(result[0])
            return result

        def close_then_fail(descriptor):
            original_close(descriptor)
            raise OSError('cleanup close failure')

        with patch.object(tempfile, 'mkstemp', side_effect=allocate), \
                patch.object(os, 'fdopen', side_effect=primary), \
                patch.object(os, 'close', side_effect=close_then_fail):
            with self.assertRaises(OSError) as caught:
                json_store.dump(self.filename, {'stable': False})
        self.assertIs(caught.exception, primary)
        self.assertEqual(os.listdir(self.temporary_directory), ['settings.json'])
        with self.assertRaises(OSError) as closed:
            os.fstat(descriptors[0])
        self.assertEqual(closed.exception.errno, errno.EBADF)
        self.assertEqual(json_store.load(self.filename), {'stable': True})

    def test_failed_unlink_does_not_hide_fsync_error(self):
        json_store.dump(self.filename, {'stable': True})
        primary = OSError('primary fsync failure')
        with patch.object(os, 'fsync', side_effect=primary), \
                patch.object(os, 'remove', side_effect=OSError('cleanup removal failure')), \
                patch.object(os, 'unlink', side_effect=OSError('cleanup removal failure')):
            with self.assertRaises(OSError) as caught:
                json_store.dump(self.filename, {'stable': False})
        self.assertIs(caught.exception, primary)
        self.assertEqual(json_store.load(self.filename), {'stable': True})

    def test_fsync_failure_cleans_staging_and_keeps_output_mode(self):
        json_store.dump(self.filename, {'stable': True})
        os.chmod(self.filename, 0o640)
        with patch.object(os, 'fsync', side_effect=OSError('fsync failed')):
            with self.assertRaisesRegex(OSError, 'fsync failed'):
                json_store.dump(self.filename, {'stable': False})
        self.assertEqual(os.listdir(self.temporary_directory), ['settings.json'])
        self.assertEqual(json_store.load(self.filename), {'stable': True})
        json_store.dump(self.filename, {'stable': False})
        self.assertEqual(os.stat(self.filename).st_mode & 0o777, 0o640)

    def test_legacy_publication_entry_points_delegate(self):
        with patch.object(file_publish, 'replace_file') as replace, \
                patch.object(file_publish, 'publish_file_no_replace') as no_replace:
            json_store.replace_file('source', 'destination')
            json_store.publish_file_no_replace('source', 'destination')
        replace.assert_called_once_with('source', 'destination')
        no_replace.assert_called_once_with('source', 'destination')

    def test_replace_failure_preserves_output_and_removes_staging(self):
        json_store.dump(self.filename, {'stable': True})
        with patch.object(file_publish, 'replace_file', side_effect=OSError('replace failed')):
            with self.assertRaisesRegex(OSError, 'replace failed'):
                json_store.dump(self.filename, {'stable': False})
        self.assertEqual(json_store.load(self.filename), {'stable': True})
        self.assertEqual(os.listdir(self.temporary_directory), ['settings.json'])


if __name__ == "__main__":
    unittest.main()
