#!/usr/bin/env python3
"""Actual Asset Renamer callback exercised against stable model DAG identities."""

import ast
import contextlib
import sys
import unittest
import warnings
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock

with warnings.catch_warnings():
    warnings.simplefilter('ignore', DeprecationWarning)
    from lib2to3.refactor import RefactoringTool, get_fixers_from_package

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'python'))
from DPyTools.pipeline.assets import rename_plan
from test_python_rig_safety import functions


class Node:
    def __init__(self, path):
        self.path = path

    def nodeName(self):
        return self.path.rsplit('|', 1)[-1]


class AssetRenamerCallbackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        filename = ROOT / 'python/DPyTools/utility/DAssetRenamer.py'
        converter = RefactoringTool(get_fixers_from_package('lib2to3.fixes'))
        tree = ast.parse(str(converter.refactor_string(
            filename.read_text(encoding='utf-8') + '\n', str(filename))))
        ui = next(node for node in tree.body
                  if isinstance(node, ast.ClassDef) and node.name == 'DAssetRenamer')
        body = [node for node in ui.body
                if isinstance(node, ast.FunctionDef) and node.name == 'runRename']
        module = ast.fix_missing_locations(ast.Module(body=body, type_ignores=[]))
        notify = functions('python/DPyTools/pipeline/assets/browser.py',
                           {'contextmanager': contextlib.contextmanager})['notify']
        cls.namespace = {'rename_plan': rename_plan, 'browser': SimpleNamespace(notify=notify)}
        exec(compile(module, str(filename), 'exec'), cls.namespace)
        cls.callback = staticmethod(cls.namespace['runRename'])

    def setUp(self):
        self.parent = Node('|root|parent')
        self.child = Node('|root|parent|child')
        self.nodes = [self.parent, self.child]
        self.sources = [node.path for node in self.nodes]
        self.targets = ['new_parent', 'new_child']
        self.events = []
        self.warnings = []
        self.fail_at = None
        self.calls = 0

        def resolve(source):
            self.events.append(('resolve', source))
            matches = [node for node in self.nodes
                       if node.path == source or node.nodeName() == source]
            if len(matches) != 1:
                raise RuntimeError('Missing or ambiguous node: ' + source)
            return matches[0]

        def rename(source, target):
            self.calls += 1
            if self.calls == self.fail_at:
                raise RuntimeError('Injected rename failure')
            node = source if isinstance(source, Node) else resolve(source)
            old = node.path
            self.events.append(('rename', old, target))
            new = old.rsplit('|', 1)[0] + '|' + target
            for item in self.nodes:
                if item.path == old or item.path.startswith(old + '|'):
                    item.path = new + item.path[len(old):]
            return node

        self.pm = SimpleNamespace(
            textScrollList=lambda widget, **kw: self.sources if widget == 'source' else self.targets,
            PyNode=resolve, rename=rename, warning=self.warnings.append,
            objExists=lambda name: any(node.nodeName() == name for node in self.nodes))
        self.namespace['pm'] = self.pm
        self.ui = SimpleNamespace(srcTextScroll='source', targetTextScroll='target',
                                  changeSelectObject=Mock())

    def test_parent_then_child_uses_retained_identity(self):
        self.assertTrue(self.callback(self.ui))
        self.assertEqual(self.parent.path, '|root|new_parent')
        self.assertEqual(self.child.path, '|root|new_parent|new_child')
        self.assertEqual([event[0] for event in self.events],
                         ['resolve', 'resolve', 'rename', 'rename'])
        self.ui.changeSelectObject.assert_called_once_with()

    def test_child_then_parent_and_unicode_names_are_supported(self):
        self.sources.reverse()
        self.targets = ['担当者_child', '担当者_parent']
        self.assertTrue(self.callback(self.ui))
        self.assertEqual(self.child.path, '|root|担当者_parent|担当者_child')

    def test_missing_later_source_prevents_all_mutation(self):
        self.sources[1] = '|root|missing'
        self.assertFalse(self.callback(self.ui))
        self.assertEqual(self.parent.path, '|root|parent')
        self.assertEqual(self.calls, 0)
        self.ui.changeSelectObject.assert_not_called()

    def test_two_names_for_same_node_are_rejected_before_mutation(self):
        self.sources = ['|root|parent', 'parent']
        self.assertFalse(self.callback(self.ui))
        self.assertEqual(self.calls, 0)
        self.assertTrue(self.warnings)

    def test_collision_and_invalid_plan_preserve_scene(self):
        self.targets[1] = 'parent'
        self.assertFalse(self.callback(self.ui))
        self.targets = ['new_parent']
        self.assertFalse(self.callback(self.ui))
        self.assertEqual(self.calls, 0)

    def test_rename_failure_reports_partial_count_and_refreshes(self):
        self.fail_at = 2
        self.assertFalse(self.callback(self.ui))
        self.assertEqual(self.parent.path, '|root|new_parent')
        self.assertEqual(self.child.path, '|root|new_parent|child')
        self.assertTrue(any('after 1 completed' in message for message in self.warnings))
        self.ui.changeSelectObject.assert_called_once_with()

    def test_noop_sources_are_still_resolved_before_success(self):
        self.targets = ['parent', 'child']
        self.assertTrue(self.callback(self.ui))
        self.assertEqual(self.calls, 0)
        self.assertEqual(len(self.events), 2)
        self.sources = ['missing']
        self.targets = ['missing']
        self.assertFalse(self.callback(self.ui))
        self.assertEqual(self.calls, 0)

    def test_query_failure_cannot_mutate(self):
        self.pm.textScrollList = Mock(side_effect=RuntimeError('UI query failed'))
        self.assertFalse(self.callback(self.ui))
        self.assertEqual(self.calls, 0)

    def test_refresh_failure_is_reported_after_completed_work(self):
        self.ui.changeSelectObject.side_effect = RuntimeError('UI refresh failed')
        self.assertFalse(self.callback(self.ui))
        self.assertEqual(self.child.path, '|root|new_parent|new_child')
        self.assertTrue(any('refresh' in message for message in self.warnings))

    def test_guarded_refresh_false_cannot_be_reported_as_complete_success(self):
        self.ui.changeSelectObject.return_value = False
        self.assertFalse(self.callback(self.ui))
        self.assertEqual(self.child.path, '|root|new_parent|new_child')
        self.ui.changeSelectObject.assert_called_once_with()

    def test_interrupt_is_not_hidden_or_automatically_undone(self):
        self.pm.rename = Mock(side_effect=KeyboardInterrupt())
        with self.assertRaises(KeyboardInterrupt):
            self.callback(self.ui)
        self.assertEqual(self.parent.path, '|root|parent')


if __name__ == '__main__':
    unittest.main()
