#!/usr/bin/env python3
"""Host-free validation checks for the archived DAnim grammar."""

import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / 'python'))

from DPyTools.features.animation import parser
from DPyTools.core import bounded_text


VALID_ANIMATION = """\
# DAnim example
anim translateX translateX |root|control 1 0 0;
animData {
  weighted 1;
  preInfinity constant;
  postInfinity linear;
  keys {
    1 2.5 linear linear 0 0 0;
    2 4.0 fixed fixed 1 1 0 10 1 20 1;
  }
}
static visibility visibility |root|control 1 True
"""


class AnimationParserTests(unittest.TestCase):
    def test_valid_records_are_counted(self):
        result = parser.validate_animation_lines(VALID_ANIMATION.splitlines(True))
        self.assertEqual(result, {
            'animation_count': 1,
            'static_count': 1,
            'key_count': 2,
        })

    def test_valid_records_are_parsed_once_into_typed_values(self):
        document = parser.parse_animation_lines(
            VALID_ANIMATION.splitlines(True))
        animation = document.records[0]
        static = document.records[1]

        self.assertIsInstance(animation, parser.AnimationRecord)
        self.assertEqual(animation.attribute, 'translateX')
        self.assertEqual(animation.node, '|root|control')
        self.assertTrue(animation.has_parent)
        self.assertTrue(animation.weighted)
        self.assertEqual(animation.keys[0].time, 1.0)
        self.assertEqual(animation.keys[1].in_angle, 10.0)
        self.assertEqual(animation.keys[1].out_weight, 1.0)
        self.assertIsInstance(static, parser.StaticRecord)
        self.assertIs(static.value, True)

    def test_truncated_block_is_rejected(self):
        truncated = VALID_ANIMATION.rsplit('}', 2)[0]
        with self.assertRaises(parser.AnimationFormatError):
            parser.validate_animation_lines(truncated.splitlines(True))

    def test_non_finite_key_value_is_rejected(self):
        invalid = VALID_ANIMATION.replace('1 2.5 linear', '1 nan linear')
        with self.assertRaises(parser.AnimationFormatError):
            parser.validate_animation_lines(invalid.splitlines(True))

    def test_malformed_key_shape_is_rejected(self):
        invalid = VALID_ANIMATION.replace(
            '1 2.5 linear linear 0 0 0;',
            '1 2.5 fixed linear 0 0 0;')
        with self.assertRaises(parser.AnimationFormatError):
            parser.validate_animation_lines(invalid.splitlines(True))

    def test_unsupported_maya_values_are_rejected(self):
        for invalid in (
                VALID_ANIMATION.replace('preInfinity constant',
                                        'preInfinity unknown'),
                VALID_ANIMATION.replace('1 2.5 linear linear',
                                        '1 2.5 custom custom')):
            with self.subTest(invalid=invalid):
                with self.assertRaises(parser.AnimationFormatError):
                    parser.parse_animation_lines(invalid.splitlines(True))

    def test_empty_key_block_is_rejected(self):
        invalid = VALID_ANIMATION.replace(
            '    1 2.5 linear linear 0 0 0;\n'
            '    2 4.0 fixed fixed 1 1 0 10 1 20 1;\n', '')
        with self.assertRaises(parser.AnimationFormatError):
            parser.parse_animation_lines(invalid.splitlines(True))

    def test_changed_file_cannot_bypass_read_budget(self):
        payload = '# comment over the admitted byte budget\n'
        stream = io.BytesIO(payload.encode('ascii'))
        text = io.StringIO(payload)
        with patch.object(parser, 'MAX_FILE_SIZE', 8), \
                patch.object(parser.os.path, 'getsize', return_value=1), \
                patch('builtins.open', side_effect=lambda filename, mode: stream if 'b' in mode else text):
            with self.assertRaises(parser.AnimationFormatError):
                parser.parse_animation_file('disposable-fixture.danim')
        self.assertTrue(stream.closed or text.closed)

    def test_in_memory_comments_count_toward_byte_budget(self):
        with patch.object(parser, 'MAX_FILE_SIZE', 8):
            with self.assertRaises(parser.AnimationFormatError):
                parser.parse_animation_lines(('#1234\n', '#1234\n'))

    def test_file_read_requests_are_bounded_before_line_allocation(self):
        requests = []

        class ObservedStream(io.BytesIO):
            def readline(self, size=-1):
                requests.append(size)
                return super().readline(size)

        stream = ObservedStream(b'#' + b'x' * 40)
        with patch.object(parser, 'MAX_LINE_LENGTH', 8), \
                patch.object(parser.os.path, 'getsize', return_value=1), \
                patch('builtins.open', return_value=stream):
            with self.assertRaises(parser.AnimationFormatError):
                parser.parse_animation_file('disposable-fixture.danim')
        self.assertEqual(requests, [9])
        self.assertTrue(stream.closed)

    def test_file_round_trip_handles_crlf_and_utf8_node_names(self):
        with tempfile.TemporaryDirectory() as directory:
            filename = Path(directory) / 'fixture.danim'
            filename.write_bytes('static visibility visibility |root|\u5236\u5fa1 1 True\r\n'.encode('utf-8'))
            document = parser.parse_animation_file(str(filename))
        self.assertEqual(document.records[0].node, '|root|\u5236\u5fa1')

    def test_exact_byte_budget_allows_eof_and_counts_utf8_bytes(self):
        for payload in (b'#1234567', b'#123456\n'):
            stream = io.BytesIO(payload)
            with patch.object(parser, 'MAX_FILE_SIZE', 8), \
                    patch.object(parser, 'MAX_LINE_LENGTH', 8), \
                    patch.object(parser.os.path, 'getsize', return_value=1), \
                    patch('builtins.open', return_value=stream):
                self.assertEqual(parser.parse_animation_file('fixture').counts()['key_count'], 0)
            self.assertTrue(stream.closed)
        with patch.object(parser, 'MAX_FILE_SIZE', 4):
            with self.assertRaises(parser.AnimationFormatError):
                parser.parse_animation_lines(('#\u3042\n',))

    def test_overlong_unicode_line_is_rejected_before_encoding(self):
        class LargeLine(str):
            def encode(self, *args, **kwargs):
                raise AssertionError('Overlong input should not be encoded')

        with patch.object(parser, 'MAX_LINE_LENGTH', 8):
            with self.assertRaises(parser.AnimationFormatError):
                parser.parse_animation_lines((LargeLine('#12345678'),))

    def test_invalid_shared_budgets_fail_before_stream_access(self):
        for limit in (0, -1, True, None, 1.5):
            stream = io.BytesIO(b'# bounded fixture\n')
            with self.subTest(limit=limit), patch.object(stream, 'readline') as read:
                with self.assertRaises(parser.AnimationFormatError):
                    list(bounded_text.read_stream_lines(stream, limit, 8, parser.AnimationFormatError))
                read.assert_not_called()
            stream.close()

    def test_record_limit_is_checked_before_parsing_another_record(self):
        lines = ('static visibility visibility control 0 True\n',) * 2
        with patch.object(parser, 'MAX_RECORDS', 1), \
                patch.object(parser, '_parse_static', wraps=parser._parse_static) as parse:
            with self.assertRaisesRegex(parser.AnimationFormatError, 'record-count limit'):
                parser.parse_animation_lines(lines)
        self.assertEqual(parse.call_count, 1)

    def test_remaining_global_key_budget_is_checked_before_next_key(self):
        animation_only = VALID_ANIMATION.split('static ')[0]
        with patch.object(parser, 'MAX_KEYS', 2), \
                patch.object(parser, '_parse_key', wraps=parser._parse_key) as parse:
            with self.assertRaisesRegex(parser.AnimationFormatError, 'key-count limit'):
                parser.parse_animation_lines((animation_only * 2).splitlines(True))
        self.assertEqual(parse.call_count, 2)


if __name__ == '__main__':
    unittest.main()
