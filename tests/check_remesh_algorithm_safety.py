#!/usr/bin/env python3
"""Real isotropic pipeline budgets and complete-node aggregate exhaustion."""
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from check_curve_path_safety import run
from check_geometry_state_transactions import extract
import check_mesh_remesh_safety

REMESHER = 'geometry/DMeshRemesher.h'
WORK_BUDGET = 'common/utility/DWorkBudget.h'


def validate(root):
    header = (root / 'src/core' / REMESHER).read_text()
    pipeline = extract(header, 'inline bool isotropicRemesh(')
    assert 'WorkBudget& budget' in pipeline and 'WorkBudget frameBudget' not in pipeline
    assert pipeline.index('!budget.consume(input.points.size())') < pipeline.index('std::vector<std::size_t> remap')
    assert 'areaTolerance, budget)' in pipeline
    node = extract((root / 'src/modules/mesh/DMesh/Utility/DMRemesh.cpp').read_text(),
                   'MStatus DMRemesh::compute(')
    assert node.count('PolygonWorkBudget polygonBudget;') == 1
    assert node.index('PolygonWorkBudget polygonBudget;') < node.index('for (auto mesh')
    assert 'kMaxGeneratedFaceCount,\n                    polygonBudget)' in node
    return header


def node_budget(root, source=None, capture=False):
    header = (root / 'src/core' / WORK_BUDGET).read_text()
    reduced = header.replace('std::size_t limit = kMaxWorkItemCount', 'std::size_t limit = 4096U')
    assert reduced != header
    return run(root, source or check_mesh_remesh_safety.bodies(root), suite='mesh_remesh', capture=capture,
               arguments=['--isotropic-work-budget'], header_overrides={WORK_BUDGET: reduced})


def mutations(root):
    header = validate(root)
    cases = (
        ('simultaneous smoothing validation omitted', header.replace(
            'if (!preservesTriangleOrientation(\n                    triangleNormal(triangle, mesh.points),',
            'if (false && !preservesTriangleOrientation(\n                    triangleNormal(triangle, mesh.points),')),
        ('component remap uncharged', header.replace('!budget.consume(input.points.size())', 'false')),
        ('compaction uncharged', header.replace('!budget.consumeProduct(mesh.points.size(), 5U)', 'false', 1)),
        ('ordered edge counting uncharged', header.replace(
            '!budget.consume(1U + 2U * generation::orderedContainerWork(edgeCounts.size()))',
            '(static_cast<void>(budget), false)')),
        ('whole smoothing face check uncharged', header.replace(
            'if (!budget.consume(8U)) return false;\n            if (!preservesTriangleOrientation(',
            'if (!preservesTriangleOrientation(')),
        ('compaction reserves all unused points', header.replace(
            'compactPoints.reserve(usedCount);',
            'compactPoints.reserve(std::max(usedCount, mesh.points.size()));')),
    )
    for label, changed in cases:
        assert changed != header, label
        try:
            run(root, '', suite='remesh_algorithm', arguments=[], capture=True,
                header_overrides={REMESHER: changed})
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    frame_path = 'geometry/DGeometryTypes.h'
    frame = (root / 'src/core' / frame_path).read_text()
    for label, changed in (
        ('unused vertices affect extent', frame.replace(
            'math::Bounds3d bounds;\n    std::size_t originIndex',
            'math::Bounds3d bounds;\n'
            '    for (const Point3& point : points) bounds.add(point.x, point.y, point.z);\n'
            '    std::size_t originIndex')),
        ('unused first vertex becomes origin', frame.replace('origin = points[originIndex];',
                                                            'origin = points.front();')),
        ('nonfinite unused payload accepted', frame.replace(
            'for (const Point3& point : points)\n        if (!isFinite(point)) return false;', '')),
    ):
        assert changed != frame, label
        try:
            run(root, '', suite='remesh_algorithm', arguments=[], capture=True,
                header_overrides={frame_path: changed})
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    component_path = 'geometry/DTriangleComponents.h'
    component = (root / 'src/core' / component_path).read_text()
    for label, changed in (
        ('disconnected components share one frame', component.replace(
            '    output.swap(candidate);',
            '    if (candidate.size() > 1U) {\n'
            '        for (std::size_t i = 1U; i < candidate.size(); ++i) {\n'
            '            candidate[0].points.insert(candidate[0].points.end(), candidate[i].points.begin(), candidate[i].points.end());\n'
            '            candidate[0].triangles.insert(candidate[0].triangles.end(), candidate[i].triangles.begin(), candidate[i].triangles.end());\n'
            '        }\n'
            '        candidate.resize(1U);\n'
            '    }\n'
            '    output.swap(candidate);')),
        ('workspace ignores point storage', component.replace('points, 256U', 'points, 0U')),
    ):
        assert changed != component, label
        try:
            run(root, '', suite='remesh_algorithm', arguments=[], capture=True,
                header_overrides={component_path: changed})
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    source = check_mesh_remesh_safety.bodies(root)
    for label, changed in (
        ('isotropic budget reset per mesh', source.replace(
             'if (!polygonBudget.consume(1U)) return MS::kFailure;',
             'polygonBudget = dommy::generation::WorkBudget();\n'
             'if (!polygonBudget.consume(1U)) return MS::kFailure;', 1)),
        ('isotropic uses fresh standalone budget', source.replace(
            'kMaxGeneratedFaceCount,\n                    polygonBudget)',
            'kMaxGeneratedFaceCount)')),
    ):
        assert changed != source, label
        try:
            node_budget(root, changed, capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--mutations' in sys.argv:
        mutations(root)
    else:
        validate(root)
        run(root, '', suite='remesh_algorithm', arguments=[])
        node_budget(root)
