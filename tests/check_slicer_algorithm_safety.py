#!/usr/bin/env python3
"""Real slicer algorithms: monotone contour traversal and evaluation budgets."""
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from check_curve_path_safety import run
from check_geometry_state_transactions import extract
import check_mesh_remesh_safety

SLICER = 'geometry/DTrianglePlaneSlicer.h'
NUMERIC = 'common/utility/DNumericValidation.h'
WORK_BUDGET = 'common/utility/DWorkBudget.h'


def validate(root):
    header = (root / 'src/core' / SLICER).read_text()
    node = (root / 'src/modules/mesh/DMesh/Utility/DMSlicer.cpp').read_text()
    compute = extract(node, 'MStatus DMSlicer::compute(')
    assert compute.count('WorkBudget sliceWorkBudget;') == 1
    assert compute.index('WorkBudget sliceWorkBudget;') < compute.index('for (auto mIt')
    assert 'remainingPointBudget, sliceWorkBudget)' in compute
    assert 'outDCrvs->insert(' not in compute
    assert compute.count('generatedCurves.publishTo(*outDCrvs)') == 1
    assert 'numeric::checkedSpatialCellIndex' in header
    return header


def node_budget(root, source=None, capture=False):
    header = (root / 'src/core' / WORK_BUDGET).read_text()
    reduced = header.replace('std::size_t limit = kMaxWorkItemCount', 'std::size_t limit = 2048U')
    assert reduced != header
    run(root, source or check_mesh_remesh_safety.bodies(root), suite='mesh_remesh', capture=capture,
        arguments=['--slice-work-budget'], header_overrides={WORK_BUDGET: reduced})


def mutations(root):
    header = validate(root)
    numeric = (root / 'src/core' / NUMERIC).read_text()
    cases = (
        ('tolerance depends on absolute coordinates', SLICER, header.replace(
            'extent, requestedTolerance', 'coordinateScale(points), requestedTolerance')),
        ('welding ignores local key origin', SLICER, header.replace(
            'cellKey(point - m_origin, m_inverseTolerance, key)',
            'cellKey(point, m_inverseTolerance, key)')),
        ('generated plane discards its origin', SLICER, header.replace(
            'plane = Plane3(unitNormal, offset, point);', 'plane = Plane3(unitNormal, offset);')),
        ('equation coefficients normalized before cancellation', SLICER, header.replace(
            'if (plane.hasOrigin) normalizedPlane.normal = normalizedNormal;',
            'normalizedPlane.normal = normalizedNormal;\n'
            '    normalizedPlane.offset = plane.offset / length(plane.normal);')),
        ('cursor restarts at every contour', SLICER, header.replace(
            'std::size_t& cursor = nextNeighbor[current];',
            'std::size_t& cursor = nextNeighbor[current]; cursor = 0U;')),
        ('triangle intersection work omitted', SLICER, header.replace(
            '!budget.consumeProduct(triangles.size(), 64U)', 'false')),
        ('spatial lookup work omitted', SLICER, header.replace(
            '!m_budget.consume(\n                            2U * generation::orderedContainerWork(m_buckets.size()))',
            'false')),
        ('unchecked spatial integer conversion', NUMERIC, numeric.replace(
            '!checkedNumericCast(std::floor(scaled), candidate)',
            '(candidate = static_cast<long long>(std::floor(scaled)), false)')),
        ('spatial neighbor headroom omitted', NUMERIC, numeric.replace(
            'candidate < std::numeric_limits<long long>::min() + 2LL ||\n'
            '        candidate > std::numeric_limits<long long>::max() - 2LL', 'false')),
    )
    for label, path, changed in cases:
        assert changed != (header if path == SLICER else numeric), label
        try:
            run(root, '', suite='slicer_algorithm', arguments=[], capture=True,
                header_overrides={path: changed})
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    source = check_mesh_remesh_safety.bodies(root)
    for label, marker in (
        ('slice budget reset per mesh', 'for (const PreparedMesh& mesh : prepared) {'),
        ('slice budget reset per cut', 'for (size_t i = 0; i < cutCount; ++i) {'),
    ):
        assert marker in source
        changed = source.replace(marker, marker + '\n'
                                 'sliceWorkBudget = dommy::generation::WorkBudget();', 1)
        try:
            node_budget(root, changed, capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    for label, changed, reduced in (
        ('slice uses a fresh standalone budget', source.replace(
            'remainingPointBudget, sliceWorkBudget)', 'remainingPointBudget)'), True),
        ('curves inserted one at a time', source.replace(
            'generatedCurves.append(curve)', 'outDCrvs->insert(curve)').replace(
            'generatedCurves.publishTo(*outDCrvs)', 'true'), False),
    ):
        assert changed != source, label
        try:
            if reduced:
                node_budget(root, changed, capture=True)
            else:
                run(root, changed, suite='mesh_remesh', capture=True)
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
        run(root, '', suite='slicer_algorithm', arguments=[])
        node_budget(root)
