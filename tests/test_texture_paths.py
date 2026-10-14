#!/usr/bin/env python3
"""Actual retained texture-path plans and failure/interruption restoration."""

import contextlib
import os
import sys
import unittest
from types import SimpleNamespace as NS
from unittest.mock import Mock

from test_attribute_batches import Graph, Port
from test_python_rig_safety import functions
from DPyTools.core import strings


class TexturePort(Port):
    def __str__(self):
        return self.owner.name() + '.' + self.label


class TextureGraph:
    def __init__(self):
        self.graph = Graph()
        self.nodes = [self.graph.create('file', name=name) for name in ('first', 'second')]
        for node, path in zip(self.nodes, ('old1.png', 'old2.png')):
            node.ports['fileTextureName'] = TexturePort(node, 'fileTextureName', 'string', path)
        self.pm = self.graph.pm
        self.pm.ls = Mock(side_effect=lambda **kw: self.nodes)
        self.pm.filePathEditor = Mock(return_value=None)
        self.mapping = {'old1.png': 'new1.png', 'old2.png': 'new2.png'}
        self.scope = functions('python/DPyTools/pipeline/lookdev/materials.py', {
            'pm': self.pm, 'sys': sys, 'contextmanager': contextlib.contextmanager,
            'strings': strings, 'parameters': self.graph.parameters,
            'dpath': NS(append=os.path.join, lastComponent=lambda path: os.path.basename(path))})
    def values(self):
        return tuple(node.attr('fileTextureName').value for node in self.nodes)
    def temporary(self):
        return self.scope['temporary_file_paths'](path_mapping=self.mapping, verbose=False)


class TexturePathTests(unittest.TestCase):
    def setUp(self):
        self.fixture = TextureGraph()
    def test_persistent_compatibility_returns_old_named_paths_and_keeps_new_values(self):
        result = self.fixture.scope['replaceFilePath'](path_mapping=self.fixture.mapping, verbose=False)
        self.assertEqual(result, {'first.fileTextureName': 'old1.png', 'second.fileTextureName': 'old2.png'})
        self.assertEqual(self.fixture.values(), ('new1.png', 'new2.png'))
    def test_change_then_throw_restores_the_failed_port_as_well_as_prior_prefix(self):
        native = self.fixture.pm.setAttr.side_effect
        def write(port, value, **kw):
            native(port, value, **kw)
            if value == 'new2.png': raise RuntimeError('changed then failed')
        self.fixture.pm.setAttr.side_effect = write
        with self.assertRaisesRegex(RuntimeError, 'changed then failed'):
            self.fixture.scope['replaceFilePath'](path_mapping=self.fixture.mapping, verbose=False)
        self.assertEqual(self.fixture.values(), ('old1.png', 'old2.png'))
    def test_interrupt_during_apply_or_context_body_restores_and_propagates(self):
        native = self.fixture.pm.setAttr.side_effect
        def write(port, value, **kw):
            native(port, value, **kw)
            if value == 'new2.png': raise KeyboardInterrupt()
        self.fixture.pm.setAttr.side_effect = write
        with self.assertRaises(KeyboardInterrupt):
            with self.fixture.temporary(): self.fail('must not enter')
        self.assertEqual(self.fixture.values(), ('old1.png', 'old2.png'))
        self.fixture.pm.setAttr.side_effect = native
        with self.assertRaises(KeyboardInterrupt):
            with self.fixture.temporary(): raise KeyboardInterrupt()
        self.assertEqual(self.fixture.values(), ('old1.png', 'old2.png'))
    def test_success_or_native_body_failure_restores_exact_values(self):
        with self.fixture.temporary():
            self.assertEqual(self.fixture.values(), ('new1.png', 'new2.png'))
        self.assertEqual(self.fixture.values(), ('old1.png', 'old2.png'))
        error = ValueError('export failed')
        with self.assertRaises(ValueError) as caught:
            with self.fixture.temporary(): raise error
        self.assertIs(caught.exception, error)
        self.assertEqual(self.fixture.values(), ('old1.png', 'old2.png'))
    def test_restore_failure_attempts_other_ports_and_preserves_primary(self):
        native = self.fixture.pm.setAttr.side_effect
        self.fixture.pm.setAttr.side_effect = lambda port, value, **kw: (_ for _ in ()).throw(RuntimeError('restore failed')) if value == 'old2.png' else native(port, value, **kw)
        error = ValueError('primary')
        with self.assertRaises(ValueError) as caught:
            with self.fixture.temporary(): raise error
        self.assertIs(caught.exception, error)
        self.assertEqual(self.fixture.values()[0], 'old1.png')
        self.fixture.pm.warning.assert_called_once()
    def test_restore_failure_after_success_is_not_silently_accepted(self):
        native = self.fixture.pm.setAttr.side_effect
        self.fixture.pm.setAttr.side_effect = lambda port, value, **kw: (_ for _ in ()).throw(RuntimeError('restore failed')) if value.startswith('old') else native(port, value, **kw)
        with self.assertRaisesRegex(RuntimeError, 'restoration was incomplete'):
            with self.fixture.temporary(): pass
    def test_warning_failure_cannot_mask_primary_interruption(self):
        native = self.fixture.pm.setAttr.side_effect
        self.fixture.pm.warning.side_effect = RuntimeError('warning failed')
        self.fixture.pm.setAttr.side_effect = lambda port, value, **kw: (_ for _ in ()).throw(RuntimeError('restore failed')) if value.startswith('old') else native(port, value, **kw)
        with self.assertRaises(KeyboardInterrupt):
            with self.fixture.temporary(): raise KeyboardInterrupt()
    def test_late_locked_referenced_connected_wrong_type_or_missing_mapping_precedes_writes(self):
        for kind in ('locked', 'referenced', 'connected', 'type', 'mapping'):
            self.setUp()
            node, port = self.fixture.nodes[1], self.fixture.nodes[1].attr('fileTextureName')
            if kind == 'locked': port.locked = True
            elif kind == 'referenced': node.referenced = True
            elif kind == 'connected': port.incoming = [self.fixture.nodes[0].attr('gain')]
            elif kind == 'type': port.kind = 'double'
            else: self.fixture.mapping.pop('old2.png')
            with self.subTest(kind=kind), self.assertRaises((TypeError, ValueError)):
                with self.fixture.temporary(): pass
            self.fixture.pm.setAttr.assert_not_called()
    def test_empty_file_nodes_and_empty_native_lists_are_noops(self):
        self.fixture.nodes[1].attr('fileTextureName').value = ''
        with self.fixture.temporary(): self.assertEqual(self.fixture.values(), ('new1.png', ''))
        self.assertEqual(self.fixture.values(), ('old1.png', ''))
        self.fixture.pm.ls.side_effect = None
        self.fixture.pm.ls.return_value = None
        self.fixture.pm.setAttr.reset_mock()
        with self.fixture.temporary(): pass
        self.fixture.pm.setAttr.assert_not_called()
    def test_retained_attributes_survive_renames_without_retargeting_names(self):
        with self.fixture.temporary(): self.fixture.nodes[0].label = 'renamed'
        self.assertEqual(self.fixture.values(), ('old1.png', 'old2.png'))
    def test_empty_editor_queries_return_lists_and_boolean_false(self):
        self.assertFalse(self.fixture.scope['isUnResolveFiles']())
        for name in ('getUnResolveFiles', 'getAllFileTypeNode', 'getListFileDirs', 'getListFileDirsStatus'):
            self.assertEqual(self.fixture.scope[name](), [])
        for value in ('not-a-list', [None] * 10001):
            self.fixture.pm.filePathEditor.return_value = value
            with self.assertRaises(ValueError): self.fixture.scope['isUnResolveFiles']()
    def test_malformed_mapping_or_texture_text_precedes_edits(self):
        for mapping in ([], {'old1.png': 'new1.png', 'old2.png': None}, {'old1.png': 'new1.png', 'old2.png': 'bad\x00path'}):
            with self.assertRaises(ValueError):
                with self.fixture.scope['temporary_file_paths'](path_mapping=mapping, verbose=False): pass
        self.fixture.pm.setAttr.assert_not_called()
    def test_restore_refuses_new_connections_without_forcing_or_unlocking(self):
        with self.assertRaisesRegex(RuntimeError, 'restoration was incomplete'):
            with self.fixture.temporary():
                self.fixture.nodes[1].attr('fileTextureName').incoming = [self.fixture.nodes[0].attr('gain')]
        self.assertEqual(self.fixture.values()[0], 'old1.png')


if __name__ == '__main__': unittest.main()
