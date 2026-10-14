#!/usr/bin/env python3
"""Actual mesh/API/UV/follicle adapters with controlled world-space stubs."""

import sys
import unittest
from types import SimpleNamespace
from unittest.mock import Mock

from test_python_rig_safety import functions
from DPyTools.core.geometry import position3
from DPyTools.core.components import component_index
from DPyTools.core.numeric import finite_number
from DPyTools.core.strings import leaf_name


class Shape:
    def __init__(self, path='|root|mesh|shape', kind='mesh'):
        self.path, self.kind = path, kind
        self.intermediate = False
        self.instanced = False
        self.shapes = []

    def name(self):
        return self.path

    def longName(self):
        return self.path

    def nodeType(self):
        return self.kind

    def getShapes(self, **kwargs):
        return self.shapes

    def getAttr(self, name):
        return self.intermediate

    def isInstanced(self):
        return self.instanced


class ShapeResolutionTests(unittest.TestCase):
    def setUp(self):
        self.shape = Shape()
        self.transform = Shape('|root|mesh', 'transform')
        self.transform.shapes = [self.shape]
        self.scope = functions('python/DPyTools/scene/selection.py',
                               {'pm': SimpleNamespace(PyNode=lambda node: node)})

    def test_shape_and_single_shape_transform_resolve(self):
        for node in (self.shape, self.transform):
            self.assertIs(self.scope['resolve_shape'](node), self.shape)

    def test_empty_or_ambiguous_transform_is_rejected(self):
        for shapes in ([], [self.shape, Shape()]):
            self.transform.shapes = shapes
            with self.assertRaises(ValueError):
                self.scope['resolve_shape'](self.transform)

    def test_wrong_type_intermediate_and_instances_are_rejected(self):
        for field, value in (('kind', 'nurbsCurve'), ('intermediate', True),
                             ('instanced', True)):
            shape = Shape()
            setattr(shape, field, value)
            with self.assertRaises(ValueError):
                self.scope['resolve_shape'](shape)

    def test_nurbs_surface_is_only_admitted_when_explicit(self):
        surface = Shape(kind='nurbsSurface')
        with self.assertRaises(ValueError):
            self.scope['resolve_shape'](surface)
        self.assertIs(self.scope['resolve_shape'](surface, ('mesh', 'nurbsSurface')), surface)


class Point:
    def __init__(self, x=0, y=0, z=0):
        self.x, self.y, self.z = x, y, z


class Float2Owner:
    def createFromList(self, values, count):
        self.values = list(values)
        assert count == 2

    def asFloat2Ptr(self):
        return self.values

    @staticmethod
    def getFloat2ArrayItem(pointer, row, coordinate):
        return pointer[coordinate]


class MeshAdapterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tools = functions('python/DPyTools/features/meshes/tools.py',
                               {'position3': position3, 'component_index': component_index})
        cls.uv = functions('python/DPyTools/features/meshes/uv.py',
                            {'position3': position3, 'finite_number': finite_number})
        cls.api = functions('python/DPyTools/features/meshes/api.py', {})

    def setUp(self):
        self.shape = Shape()
        self.entries = [(self.shape.path, 'vertex', [1, 2])]
        self.position_spaces = []
        self.iterator_count = 0
        self.added_paths = []
        self.uv_calls = []
        self.closest_calls = []
        outer = self

        class Path:
            def fullPathName(self):
                return self.name

        class Component:
            def isNull(self):
                return self.kind is None
            def hasFn(self, kind):
                return self.kind == kind

        class Selection:
            def length(self):
                return len(outer.entries)
            def add(self, path):
                outer.added_paths.append(path)
            def getDagPath(self, index, path, component=None):
                path.name = outer.entries[index][0]
                if component is not None:
                    component.kind, component.ids = outer.entries[index][1:]

        class Iterator:
            def __init__(self, path, component):
                outer.iterator_count += 1
                self.ids = component.ids
                self.offset = 0
            def isDone(self):
                return self.offset == len(self.ids)
            def index(self):
                return self.ids[self.offset]
            def position(self, space='object'):
                outer.position_spaces.append(space)
                return Point(self.index() + (100 if space == 'world' else 0), 2, 3)
            def next(self):
                self.offset += 1
            # lib2to3 translates the historical native .next() call to next().
            __next__ = next

        def get_uv(point, pointer, space):
            self.uv_calls.append((point.x, space))
            pointer[:] = [(point.x - 100) / 10, 0.5]

        def closest(point, output, space):
            self.closest_calls.append((point.x, space))
            output.x, output.y, output.z = point.x, point.y, point.z

        self.mesh_fn = SimpleNamespace(getUVAtPoint=Mock(side_effect=get_uv),
                                       getClosestPoint=Mock(side_effect=closest))
        self.om = SimpleNamespace(
            MSelectionList=Selection, MDagPath=Path, MObject=Component,
            MGlobal=SimpleNamespace(getActiveSelectionList=lambda selection: None),
            MItMeshVertex=Iterator, MFn=SimpleNamespace(kMeshVertComponent='vertex'),
            MSpace=SimpleNamespace(kWorld='world'), MPoint=Point,
            MScriptUtil=Float2Owner, MFnMesh=Mock(return_value=self.mesh_fn))

        def resolve(value, unused=('mesh',)):
            if isinstance(value, Shape):
                return value
            return Shape(value)

        for scope in (self.tools, self.uv, self.api):
            scope.update(om=self.om, resolve_shape=resolve)
        self.api['pm'] = SimpleNamespace(PyNode=lambda node: node)
        self.get_mesh = Mock(side_effect=self.api['getMeshFn'])
        for scope in (self.tools, self.uv):
            scope['api'] = SimpleNamespace(getMeshFn=self.get_mesh)

    def test_vertices_are_world_space_for_translated_mesh(self):
        ids, positions = self.tools['getSelectedMeshIds'](self.shape)
        self.assertEqual(ids, [1, 2])
        self.assertEqual(positions, [(101, 2, 3), (102, 2, 3)])
        self.assertEqual(self.position_spaces, ['world', 'world'])

    def test_all_same_mesh_selection_blocks_are_collected(self):
        self.entries.append((self.shape.path, 'vertex', [3]))
        self.assertEqual(self.tools['getSelectedMeshIds']()[0], [1, 2, 3])

    def test_empty_selection_is_a_noop(self):
        self.entries = []
        self.assertEqual(self.tools['getSelectedMeshIds'](), ([], []))
        self.assertEqual(self.iterator_count, 0)

    def test_transform_and_wrong_components_are_not_whole_mesh_selection(self):
        for kind in (None, 'edge', 'face'):
            self.entries = [(self.shape.path, kind, [1])]
            with self.assertRaises(ValueError):
                self.tools['getSelectedMeshIds']()
        self.assertEqual(self.iterator_count, 0)

    def test_mixed_or_wrong_target_mesh_is_rejected_before_positions(self):
        self.entries.append(('|other|shape', 'vertex', [1]))
        with self.assertRaises(ValueError):
            self.tools['getSelectedMeshIds']()
        self.entries = self.entries[:1]
        with self.assertRaises(ValueError):
            self.tools['getSelectedMeshIds'](Shape('|other|shape'))
        self.assertEqual(self.iterator_count, 0)

    def test_duplicate_vertex_ids_do_not_return_partial_data(self):
        self.entries.append((self.shape.path, 'vertex', [1]))
        with self.assertRaises(ValueError):
            self.tools['getSelectedMeshIds']()

    def test_actual_api_uses_shape_full_dag_path(self):
        self.assertIs(self.api['getMeshFn'](self.shape), self.mesh_fn)
        self.assertEqual(self.added_paths, [self.shape.path])

    def test_uv_queries_use_same_world_points(self):
        positions = self.tools['getSelectedMeshIds']()[1]
        self.assertEqual(self.uv['getUVfromPoint'](self.shape, positions),
                         [[0.1, 0.5], [0.2, 0.5]])
        self.assertEqual(self.uv_calls, [(101, 'world'), (102, 'world')])

    def test_later_bad_input_is_rejected_before_any_uv_or_closest_call(self):
        for query in (self.uv['getUVfromPoint'], self.tools['getClosestPointFromPoint']):
            with self.assertRaises(ValueError):
                query(self.shape, [(101, 2, 3), (float('nan'), 2, 3)])
        self.get_mesh.assert_not_called()

    def test_uv_output_is_reinitialized_per_point_and_never_reuses_old_uv(self):
        first = self.mesh_fn.getUVAtPoint.side_effect
        calls = [0]
        def incomplete(point, pointer, space):
            calls[0] += 1
            if calls[0] == 1:
                first(point, pointer, space)
        self.mesh_fn.getUVAtPoint.side_effect = incomplete
        with self.assertRaises(ValueError):
            self.uv['getUVfromPoint'](self.shape, [(101, 2, 3), (102, 2, 3)])

    def test_closest_queries_are_world_space_and_output_is_checked(self):
        self.assertEqual(self.tools['getClosestPointFromPoint'](self.shape, [(101, 2, 3)]),
                         [[101, 2, 3]])
        self.assertEqual(self.closest_calls, [(101, 'world')])
        self.mesh_fn.getClosestPoint.side_effect = lambda *args: None
        with self.assertRaises(ValueError):
            self.tools['getClosestPointFromPoint'](self.shape, [(101, 2, 3)])

    def test_native_failure_is_not_converted_to_valid_results(self):
        self.mesh_fn.getUVAtPoint.side_effect = RuntimeError('UV failure')
        with self.assertRaises(RuntimeError):
            self.uv['getUVfromPoint'](self.shape, [(101, 2, 3)])


class Plug:
    def __init__(self, name, connections):
        self.name, self.connections = name, connections
        self.set = Mock()
    def __getitem__(self, index):
        return Plug(self.name + '[{}]'.format(index), self.connections)
    def connect(self, other):
        self.connections.append((self.name, other.name))


class FollicleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scope = functions('python/DPyTools/features/rigging/follicles.py',
                              {'sys': sys, 'finite_number': finite_number,
                               'leaf_name': leaf_name})

    def setUp(self):
        self.connections = []
        self.shape = Shape()
        self.parent = SimpleNamespace()
        self.follicle = SimpleNamespace(getParent=lambda: self.parent)
        for obj, names in ((self.shape, ('outMesh', 'worldMesh', 'local', 'worldMatrix')),
                           (self.parent, ('inheritsTransform', 'rotate', 'translate')),
                           (self.follicle, ('inputMesh', 'inputSurface', 'inputWorldMatrix',
                                            'outRotate', 'outTranslate', 'parameterU', 'parameterV'))):
            for name in names:
                setattr(obj, name, Plug(name, self.connections))
        self.pm = SimpleNamespace(createNode=Mock(return_value=self.follicle),
                                  delete=Mock(), warning=Mock())
        self.scope.update(pm=self.pm, resolve_shape=lambda node, kinds: node)

    def test_mesh_uses_local_output_and_explicit_world_matrix_index(self):
        self.assertIs(self.scope['create_follicle'](self.shape, [0.2, 0.3]), self.follicle)
        self.assertIn(('outMesh', 'inputMesh'), self.connections)
        self.assertNotIn(('worldMesh', 'inputMesh'), self.connections)
        self.assertIn(('worldMatrix[0]', 'inputWorldMatrix'), self.connections)
        self.parent.inheritsTransform.set.assert_called_once_with(False)
        self.pm.delete.assert_not_called()

    def test_nurbs_surface_uses_local_surface(self):
        self.shape.kind = 'nurbsSurface'
        self.scope['create_follicle'](self.shape)
        self.assertIn(('local', 'inputSurface'), self.connections)

    def test_tiled_finite_uvs_remain_supported(self):
        self.scope['create_follicle'](self.shape, [-2, 3])
        self.follicle.parameterU.set.assert_called_once_with(-2.0)
        self.follicle.parameterV.set.assert_called_once_with(3.0)

    def test_bad_uv_fails_before_geometry_query_or_creation(self):
        self.scope['resolve_shape'] = Mock()
        for uv in ([0], [0, 1, 2], [True, 0], [float('nan'), 0], '12'):
            with self.assertRaises((TypeError, ValueError)):
                self.scope['create_follicle'](self.shape, uv)
        self.pm.createNode.assert_not_called()
        self.scope['resolve_shape'].assert_not_called()

    def test_invalid_shape_returns_false_before_creating_nodes(self):
        self.scope['resolve_shape'] = Mock(side_effect=ValueError('bad shape'))
        self.assertFalse(self.scope['create_follicle'](self.shape))
        self.pm.createNode.assert_not_called()

    def test_incomplete_follicle_is_cleaned_and_primary_error_preserved(self):
        error = RuntimeError('connection failed')
        self.shape.outMesh.connect = Mock(side_effect=error)
        with self.assertRaises(RuntimeError) as caught:
            self.scope['create_follicle'](self.shape)
        self.assertIs(caught.exception, error)
        self.pm.delete.assert_called_once_with(self.parent)

    def test_cleanup_failure_does_not_hide_connection_failure(self):
        self.shape.outMesh.connect = Mock(side_effect=RuntimeError('primary'))
        self.pm.delete.side_effect = RuntimeError('cleanup')
        with self.assertRaisesRegex(RuntimeError, 'primary'):
            self.scope['create_follicle'](self.shape)
        self.pm.warning.assert_called_once()


if __name__ == '__main__':
    unittest.main()
