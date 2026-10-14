#!/usr/bin/env python3
"""Host-free validation tests for the archived ``.elip`` format."""

import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / 'python'))

from DPyTools.features.rigging.face import lip_edges


class LipEdgeTests(unittest.TestCase):
    def test_parse_and_serialize_round_trip(self):
        data = lip_edges.parse_lines(('TLIP 1 2 3\n', 'BLIP 9 8 7\n'))
        self.assertEqual(data.top, (1, 2, 3))
        self.assertEqual(data.bottom, (9, 8, 7))
        self.assertEqual(
            lip_edges.serialize(data), 'TLIP 1 2 3\nBLIP 9 8 7\n')

    def test_missing_duplicate_and_negative_data_are_rejected(self):
        invalid_documents = (
            ('TLIP 1 2\n',),
            ('TLIP 1 2\n', 'TLIP 3 4\n', 'BLIP 5 6\n'),
            ('TLIP 1 1\n', 'BLIP 2 3\n'),
            ('TLIP -1 2\n', 'BLIP 3 4\n'),
            ('UNKNOWN 1\n', 'BLIP 2\n'),
        )
        for lines in invalid_documents:
            with self.subTest(lines=lines):
                with self.assertRaises(lip_edges.LipEdgeFormatError):
                    lip_edges.parse_lines(lines)

    def test_save_replaces_complete_file(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / 'mouth.elip'
            destination.write_text('old', encoding='utf-8')
            lip_edges.save(str(destination), (4, 5), (8, 9))
            self.assertEqual(
                destination.read_text(encoding='utf-8'),
                'TLIP 4 5\nBLIP 8 9\n')

    def test_invalid_save_preserves_existing_file(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / 'mouth.elip'
            destination.write_text('old', encoding='utf-8')
            with self.assertRaises(lip_edges.LipEdgeFormatError):
                lip_edges.save(str(destination), (), (8, 9))
            self.assertEqual(destination.read_text(encoding='utf-8'), 'old')

    def test_noninteger_edges_are_rejected_instead_of_normalized(self):
        for value in (True, False, 1.9, 1.0, float('inf'), float('nan'), object()):
            with self.subTest(value_type=type(value).__name__):
                with self.assertRaises(lip_edges.LipEdgeFormatError):
                    lip_edges.create((value,), (2,))
        self.assertEqual(lip_edges.create(('01', '+2'), (3,)).top, (1, 2))

    def test_edge_count_is_checked_while_consuming_generator(self):
        consumed = []

        def edges():
            for value in range(5):
                consumed.append(value)
                yield value

        with patch.object(lip_edges, 'MAX_EDGE_COUNT', 2):
            with self.assertRaises(lip_edges.LipEdgeFormatError):
                lip_edges.create(edges(), (6,))
        self.assertEqual(consumed, [0, 1, 2])

    def test_changed_file_cannot_bypass_read_budget(self):
        payload = '# comment over budget\nTLIP 1\nBLIP 2\n'
        stream, text = io.BytesIO(payload.encode('ascii')), io.StringIO(payload)
        with patch.object(lip_edges, 'MAX_FILE_SIZE', 8), \
                patch.object(lip_edges.os.path, 'getsize', return_value=1), \
                patch('builtins.open', side_effect=lambda filename, mode: stream if 'b' in mode else text):
            with self.assertRaises(lip_edges.LipEdgeFormatError):
                lip_edges.load('disposable-fixture.elip')
        self.assertTrue(stream.closed or text.closed)

    def test_comments_count_toward_in_memory_budget(self):
        with patch.object(lip_edges, 'MAX_FILE_SIZE', 32):
            with self.assertRaises(lip_edges.LipEdgeFormatError):
                lip_edges.parse_lines(('# comment exceeding budget\n', 'TLIP 1\n', 'BLIP 2\n'))

    def test_edge_index_domain_rejects_oversized_decimal_before_conversion(self):
        maximum = lip_edges.MAX_EDGE_INDEX
        self.assertEqual(lip_edges.create((0, maximum), (1,)).top, (0, maximum))
        self.assertEqual(lip_edges.create(('0' * 10000 + '1',), (2,)).top, (1,))
        for value in (maximum + 1, str(maximum + 1), '9' * 10000, '1.0', 'True'):
            with self.subTest(value_type=type(value).__name__):
                with self.assertRaises(lip_edges.LipEdgeFormatError):
                    lip_edges.create((value,), (1,))

    def test_file_read_is_bounded_and_handle_closes_on_error(self):
        requests = []

        class ObservedStream(io.BytesIO):
            def readline(self, size=-1):
                requests.append(size)
                return super().readline(size)

        stream = ObservedStream(b'#' + b'x' * 30)
        with patch.object(lip_edges, 'MAX_LINE_LENGTH', 8), \
                patch.object(lip_edges.os.path, 'getsize', return_value=1), \
                patch('builtins.open', return_value=stream):
            with self.assertRaises(lip_edges.LipEdgeFormatError):
                lip_edges.load('fixture.elip')
        self.assertEqual(requests, [9])
        self.assertTrue(stream.closed)

    def test_crlf_file_round_trip_and_exact_byte_budget(self):
        payload = b'TLIP 1\r\nBLIP 2\r\n'
        with tempfile.TemporaryDirectory() as directory:
            filename = Path(directory) / 'fixture.elip'
            filename.write_bytes(payload)
            with patch.object(lip_edges, 'MAX_FILE_SIZE', len(payload)):
                self.assertEqual(lip_edges.load(str(filename)), lip_edges.create((1,), (2,)))

    def test_oversized_save_preserves_previous_readable_file(self):
        with tempfile.TemporaryDirectory() as directory:
            filename = Path(directory) / 'fixture.elip'
            previous = lip_edges.save(str(filename), (1,), (2,))
            old = filename.read_bytes()
            with self.assertRaises(lip_edges.LipEdgeFormatError):
                lip_edges.save(str(filename), range(lip_edges.MAX_EDGE_COUNT),
                               range(lip_edges.MAX_EDGE_COUNT, lip_edges.MAX_EDGE_COUNT * 2))
            self.assertEqual(filename.read_bytes(), old)
            self.assertEqual(lip_edges.load(str(filename)), previous)
            self.assertEqual([p.name for p in Path(directory).iterdir()], ['fixture.elip'])

    def test_save_obeys_exact_file_and_line_budgets(self):
        data = lip_edges.create((10, 11), (20, 21))
        payload = 'TLIP 10 11\nBLIP 20 21\n'
        with tempfile.TemporaryDirectory() as directory:
            filename = Path(directory) / 'fixture.elip'
            with patch.object(lip_edges, 'MAX_FILE_SIZE', len(payload)), \
                    patch.object(lip_edges, 'MAX_LINE_LENGTH', len('TLIP 10 11\n')):
                self.assertEqual(lip_edges.save(str(filename), data.top, data.bottom), data)
                self.assertEqual(filename.read_bytes(), payload.encode('ascii'))
                self.assertEqual(lip_edges.load(str(filename)), data)
            for limits in ({'MAX_FILE_SIZE': len(payload) - 1},
                           {'MAX_LINE_LENGTH': len('TLIP 10 11\n') - 1}):
                with patch.multiple(lip_edges, **limits):
                    with self.assertRaises(lip_edges.LipEdgeFormatError):
                        lip_edges.save(str(filename), data.top, data.bottom)
                self.assertEqual(filename.read_bytes(), payload.encode('ascii'))


if __name__ == '__main__':
    unittest.main()
