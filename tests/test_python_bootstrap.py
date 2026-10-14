#!/usr/bin/env python3
"""Host-free idempotency test for the Maya bootstrap module."""

import importlib
import sys
import types
import unittest
from pathlib import Path
from unittest.mock import Mock, patch


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "python"))


class BootstrapTests(unittest.TestCase):
    def setUp(self):
        self.menus = set()
        self.callbacks = []
        self.plugin_loaded = False
        self.menu_labels = []
        self.menu_items = []

        maya = types.ModuleType("maya")
        maya.__path__ = []
        cmds = types.ModuleType("maya.cmds")
        mel = types.ModuleType("maya.mel")
        app = types.ModuleType("maya.app")
        app.__path__ = []
        general = types.ModuleType("maya.app.general")
        general.__path__ = []
        node_editor = types.ModuleType("maya.app.general.nodeEditorMenus")
        node_editor.customInclusiveNodeItemMenuCallbacks = self.callbacks

        def menu(name, **kwargs):
            if kwargs.get("query") and kwargs.get("exists"):
                return name in self.menus
            self.menus.add(name)
            return name

        def delete_ui(name, **kwargs):
            self.menus.discard(name)

        def plugin_info(name, **kwargs):
            return self.plugin_loaded

        def load_plugin(name, **kwargs):
            self.plugin_loaded = True
            return name

        cmds.menu = menu
        def menu_item(**kwargs):
            self.menu_labels.append(kwargs.get('label'))
            self.menu_items.append(dict(kwargs))
            return kwargs.get('label')
        cmds.menuItem = menu_item
        cmds.setParent = lambda *args, **kwargs: None
        cmds.warning = lambda message: None
        cmds.error = lambda message: (_ for _ in ()).throw(RuntimeError(message))
        cmds.about = lambda **kwargs: False
        cmds.pluginInfo = plugin_info
        cmds.loadPlugin = load_plugin
        cmds.deleteUI = delete_ui
        mel.eval = lambda expression: "MayaWindow"

        maya.cmds = cmds
        maya.mel = mel
        maya.app = app
        app.general = general
        general.nodeEditorMenus = node_editor

        callback_module = types.ModuleType("DPyTools.ui.customMarkingMenus")

        def marking_menu_callback(parent, node):
            return False

        marking_menu_callback.__module__ = "DPyTools.ui.customMarkingMenus"
        marking_menu_callback.__name__ = "createDToolNodeEdMarkingMenus"
        callback_module.createDToolNodeEdMarkingMenus = marking_menu_callback

        self.fake_modules = {
            "maya": maya,
            "maya.cmds": cmds,
            "maya.mel": mel,
            "maya.app": app,
            "maya.app.general": general,
            "maya.app.general.nodeEditorMenus": node_editor,
            "DPyTools.ui.customMarkingMenus": callback_module,
        }
        self.previous_modules = {
            name: sys.modules.get(name) for name in self.fake_modules
        }
        sys.modules.update(self.fake_modules)
        sys.modules.pop("DPyTools.bootstrap", None)
        self.bootstrap = importlib.import_module("DPyTools.bootstrap")

    def tearDown(self):
        sys.modules.pop("DPyTools.bootstrap", None)
        for name, previous in self.previous_modules.items():
            if previous is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = previous

    def test_main_is_idempotent_and_unload_cleans_python_state(self):
        self.assertTrue(self.bootstrap.main())
        self.assertTrue(self.bootstrap.main())
        self.assertEqual(self.menus, {"DPToolsMenu"})
        self.assertEqual(len(self.callbacks), 1)

        self.assertTrue(self.bootstrap.unload())
        self.assertEqual(self.menus, set())
        self.assertEqual(self.callbacks, [])

    def test_unimplemented_match_scale_is_not_in_default_menu(self):
        self.bootstrap.createDPyToolsMenu()
        self.assertNotIn('Match Scale Tool...', self.menu_labels)
        self.assertIn('Create BBox Measure', self.menu_labels)

    def test_misleading_joint_creation_and_empty_submenu_are_not_registered(self):
        self.bootstrap.createDPyToolsMenu()
        self.assertNotIn('Create Joint at Target Transform', self.menu_labels)
        self.assertNotIn('Joint', self.menu_labels)
        self.assertIn('Human Rig Tool', self.menu_labels)

    def test_system_report_labels_dispatch_lazily_to_canonical_implementations(self):
        target = Mock(return_value=None)
        with patch.object(self.bootstrap, '_resolve', return_value=target) as resolve:
            self.bootstrap.createDPyToolsMenu()
            resolve.assert_not_called()
            for label, name in (
                    ('Current Opened File Info', 'currentFileInfo'),
                    ('System Info', 'displayInfo'), ('Loaded Plugin Info', 'pluginInfo'),
                    ('OpenGL Info', 'displayOpenGLInfo')):
                with self.subTest(label=label):
                    items = [item for item in self.menu_items if item.get('label') == label]
                    self.assertEqual(len(items), 1)
                    self.assertIsNone(items[0]['command'](object()))
                    resolve.assert_called_once_with('DPyTools.features.system_info', name)
                    target.assert_called_once_with()
                    resolve.reset_mock()
                    target.reset_mock()

    def test_rivet_labels_keep_their_distinct_lazy_dispatch_targets(self):
        result = object()
        target = Mock(return_value=result)
        with patch.object(self.bootstrap, '_resolve', return_value=target) as resolve:
            self.bootstrap.createDPyToolsMenu()
            resolve.assert_not_called()
            for label, attribute in (
                    ('Polygon Edge Rivet', 'createEdgeRivet'),
                    ('Polygon Face Rivet', 'createFaceRivet')):
                with self.subTest(label=label):
                    items = [item for item in self.menu_items
                             if item.get('label') == label]
                    self.assertEqual(len(items), 1)
                    self.assertIs(items[0]['command'](object()), result)
                    resolve.assert_called_once_with(
                        'DPyTools.rigging.DConstrainOp', attribute)
                    target.assert_called_once_with()
                    resolve.reset_mock()
                    target.reset_mock()


if __name__ == "__main__":
    unittest.main()
