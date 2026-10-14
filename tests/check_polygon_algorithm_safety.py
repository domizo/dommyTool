#!/usr/bin/env python3
"""Real polygon algorithms: bounded cleanup, dimensional tolerances and stages."""
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from check_curve_path_safety import run
from check_geometry_state_transactions import extract
import check_curve_partition_safety
import check_mesh_remesh_safety


def validate(root):
    header = (root / 'src/core/geometry/DPolygonTriangulator.h').read_text()
    cleanup = extract(header, 'inline bool cleanPolygon(')
    assert '.erase(' not in cleanup, 'Polygon cleanup must not shift a vector for each removed vertex'
    assert 'PolygonWorkBudget& budget' in cleanup and 'budget.consume' in cleanup
    for name, path, loop in (
        ('DCPartition', 'curve/DCurve/Utility/DCPartition.cpp', 'for (auto curve'),
        ('DMRemesh', 'mesh/DMesh/Utility/DMRemesh.cpp', 'for (auto mesh'),
        ('DMDebrisGen', 'mesh/DMesh/Utility/DMDebrisGen.cpp', 'for (auto mesh'),
    ):
        source = (root / ('src/modules/' + path)).read_text()
        compute = extract(source, 'MStatus ' + name + '::compute(')
        assert compute.count('PolygonWorkBudget polygonBudget;') == 1
        assert compute.index('PolygonWorkBudget polygonBudget;') < compute.index(loop)
        assert 'polygonBudget.consume(1U)' in compute
        if name == 'DCPartition':
            assert 'if (polygonBudget.exceeded()) return MS::kFailure;' in compute
    return header


def node_budgets(root, sources=None):
    sources = sources or {}
    header = (root / 'src/core/common/utility/DWorkBudget.h').read_text()
    reduced = header.replace('std::size_t limit = kMaxWorkItemCount',
                             'std::size_t limit = 256U')
    assert reduced != header
    for suite, source in (
        ('curve_partition', check_curve_partition_safety.bodies(root)),
        ('mesh_remesh', check_mesh_remesh_safety.bodies(root)),
    ):
        run(root, sources.get(suite, source), suite=suite, arguments=['--polygon-budget'],
            header_overrides={'common/utility/DWorkBudget.h': reduced})


def mutations(root):
    header = validate(root)
    cases = (
        ('cleanup work omitted', header.replace(
            'if (!budget.consumeProduct(input.size(), 4U)) return false;', '', 1)),
        ('area uses a distance threshold', header.replace('const double area = distance * extent;',
                                                         'const double area = distance;')),
        ('absolute-coordinate area cancellation', header.replace(
            'const Point2& origin = points.front();', 'const Point2 origin;')),
        ('whole-batch relaxation validation omitted', header[:header.index(
            '        // Safe individual proposals can invert a face')] +
         '        working.swap(next);\n' + header[header.index(
            '        if (accepted) working.swap(next);') +
            len('        if (accepted) working.swap(next);'):]),
        ('nonplanar projection accepted', header.replace(
            'std::fabs(residual) > maximumRelativePlaneDistance', 'false')),
        ('projection workspace admission omitted', header.replace(
            '!validTriangleWorkspaceStorage(points.size(), points.size() - 2U, extraStorageBytes)',
            '(static_cast<void>(extraStorageBytes), false)')),
        ('triangulation workspace admission omitted', header.replace(
            '!validTriangleWorkspaceStorage(input.size(), input.size() - 2U, extraStorageBytes)',
            '(static_cast<void>(extraStorageBytes), false)')),
        ('refinement growth admission omitted', header.replace(
            '!validTriangleWorkspaceStorage(std::min(plannedPoints, maximumPointCount),\n'
            '                                           plannedTriangles, extraStorageBytes)', 'false')),
    )
    for label, modified in cases:
        assert modified != header, label
        try:
            run(root, '', suite='polygon_algorithm', arguments=[], capture=True,
                header_overrides={'geometry/DPolygonTriangulator.h': modified})
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1, label + ': mutation must fail an executable contract'
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    relaxation = extract(header, 'inline bool relaxInteriorVertices(')
    relaxed = relaxation.replace(
        '!validTriangleWorkspaceStorage(triangulation.points.size(),\n'
        '                                       triangulation.triangles.size(), extraStorageBytes)',
        '(static_cast<void>(extraStorageBytes), false)')
    assert relaxed != relaxation
    try:
        run(root, '', suite='polygon_algorithm', arguments=[], capture=True,
            header_overrides={'geometry/DPolygonTriangulator.h': header.replace(relaxation, relaxed)})
    except subprocess.CalledProcessError as error:
        assert Path(error.cmd[0]).name.endswith('_tests'), 'Relaxation mutation must fail at runtime'
        print('Detected mutation: relaxation workspace admission omitted')
    else:
        raise AssertionError('Undetected mutation: relaxation workspace admission omitted')
    budget_header = (root / 'src/core/common/utility/DWorkBudget.h').read_text()
    modified_budget = budget_header.replace('if (exceeded_ || count > remaining_)',
                                            'if (count > remaining_)')
    assert modified_budget != budget_header
    try:
        run(root, '', suite='polygon_algorithm', arguments=[], capture=True,
            header_overrides={'common/utility/DWorkBudget.h': modified_budget})
    except subprocess.CalledProcessError as error:
        assert error.returncode != 1, 'Sticky budget mutation must fail an executable contract'
        print('Detected mutation: sticky exhaustion ignored')
    else:
        raise AssertionError('Undetected mutation: sticky exhaustion ignored')
    for suite, source in (
        ('curve_partition', check_curve_partition_safety.bodies(root)),
        ('mesh_remesh', check_mesh_remesh_safety.bodies(root)),
    ):
        for label, modified in (
            ('budget reset per primitive', source.replace(
                'if (!polygonBudget.consume(1U)) return MS::kFailure;',
                'polygonBudget = dommy::geometry::PolygonWorkBudget();\n'
                'if (!polygonBudget.consume(1U)) return MS::kFailure;')),
            ('exhaustion treated as an invalid shape', source.replace(
                'if (polygonBudget.exceeded()) return MS::kFailure;', '')),
        ):
            if modified == source:
                continue
            try:
                node_budgets(root, {suite: modified})
            except subprocess.CalledProcessError as error:
                assert error.returncode != 1, label
                print('Detected mutation: ' + suite + ' ' + label)
            else:
                raise AssertionError('Undetected mutation: ' + suite + ' ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--mutations' in sys.argv:
        mutations(root)
    else:
        validate(root)
        run(root, '', suite='polygon_algorithm', arguments=[])
        node_budgets(root)
