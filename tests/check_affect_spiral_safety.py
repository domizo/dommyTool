#!/usr/bin/env python3
"""Affect/spiral contracts, bounded workloads and failure-injectable boundaries."""
from pathlib import Path
import re
import subprocess
import sys
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

NODES = {name: 'src/modules/point/DPoint/Utility/' + name + '.cpp'
         for name in ('DPAffect', 'DPSpiralField')}


def enums(root):
    source = (root / 'src/modules/point/DPoint/Utility/DPAffect.h').read_text()
    return re.search(r'enum\s*\{[^}]+\};typedef short AffectType;', source).group(0)


def initializers(root):
    result = [enums(root)]
    for name, path in NODES.items():
        source = (root / path).read_text()
        result.extend(declarations(name, source, True))
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n\n'.join(result)


def compute_bodies(root):
    result = [enums(root)]
    result.append('namespace dommy { namespace math {\ntemplate<class T>\n' +
        extract((root / 'src/core/math/dommyMath.h').read_text(), 'inline void orderBounds(') + '\n}}')
    validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    result.append('namespace dommy { namespace maya {\n' + '\n'.join(
        extract(validation, 'inline bool ' + name + '(') for name in (
            'isFinitePoint', 'isFloatRepresentableVector', 'normalizedVector', 'normalizedVectorOrZero')) +
        extract((root / 'src/core/common/utility/DMayaRampSampling.h').read_text(),
                'inline bool sampleFiniteRamp(') + '\n}}')
    for header in ('AffectParams', 'SpiralParams'):
        name = 'DPAffect' if header == 'AffectParams' else 'DPSpiralField'
        result.append(extract((root / NODES[name]).with_suffix('.h').read_text(), 'struct ' + header) + ';')
    result += [extract((root / 'src/core/common/utility/DPointForceBudget.h').read_text(), 'namespace dommy {'),
               extract((root / 'src/modules/point/DPoint/Common/DPointForceOutput.h').read_text(), 'namespace dommy {'),
               extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(), 'void checkDPAttribute(')]
    source = (root / 'src/core/Data/DDataUtility.cpp').read_text()
    for signature in ('void checkDDataPointAttribute(', 'void checkDDataAttribute('):
        start = 0
        for _ in range(2):
            start = source.index(signature, start)
            result.append(extract(source[start:], signature))
            start += len(signature)
    result.append(extract((root / 'src/modules/mesh/DMesh/common/DMeshUtil.cpp').read_text(),
                          'void checkDMeshPointAttribute('))
    for name, path in NODES.items():
        source = (root / path).read_text()
        helpers = extract(source, 'namespace {')
        if name == 'DPAffect':
            for cls in ('AffectCurveOp', 'AffectSurfaceOp', 'AffectMeshOp'):
                helpers = helpers.replace(extract(helpers, 'class ' + cls) + ';', '')
        result.append(helpers)
        declared = declarations(name, source, False)
        method, params = ('evalAffectParms', 'AffectParams') if name == 'DPAffect' else ('evalParms', 'SpiralParams')
        declared[0] = declared[0].replace(' {', ' {MObject thisMObject()const{return {"node"};}' +
            'MStatus ' + method + '(MDataBlock&, ' + params + '&);', 1)
        result.extend(declared)
        result.append(extract(source, 'MStatus ' + name + '::' + method + '('))
        result.append(extract(source, 'MStatus ' + name + '::compute('))
    return '\n\n'.join(result)


def mutations(root):
    init, compute = initializers(root), compute_bodies(root)
    cases = [
        ('mesh primitive type replacement omitted', False, compute.replace(
            'checkDDataAttribute(outAttrName, output, true);\n                        output.addDPAttributeValues',
            'output.addDPAttributeValues')),
        ('incomplete primitive centers accepted', False, compute.replace(
            'if (centers.size() != queryCount) return MS::kFailure;', '')),
        ('nonfinite curve parameter accepted', False, compute.replace(
            'if (!std::isfinite(parameter)) return MS::kFailure;', 'if (!std::isfinite(parameter)) return status;')),
        ('failed control handle consumed', False, compute.replace(
            'if (!stat) return stat;\n    parms.m_type = aAffectTypeHandle.asShort();',
            'parms.m_type = aAffectTypeHandle.asShort();')),
        ('quadratic affect work omitted', False, compute.replace(
            'checkedProduct(points, targets, kMaxWorkItemCount, work)', 'true')),
        ('combined target workspace omitted', False, compute.replace(
            'checkedSum(bytes, targetBytes, maximum, bytes)', 'true')),
        ('affect compute preflight omitted', False, compute.replace(
            'if (!validAffectBudget(queryCount, input.partitionCount, targetCount,\n'
            '                               primitive ? input.pointCount : 0U))', 'if (false)')),
        ('affect dispatcher preflight omitted', False, compute.replace(
            'if (!validAffectBudget(queryCount, m_pts->size(), targets))', 'if (false)')),
        ('curve source partitions not accumulated', False, compute.replace(
            'checkedSum(pointCount, (*it)->numberOfPoints(),', 'checkedSum(0U, (*it)->numberOfPoints(),', 1)),
        ('mesh source partitions not accumulated', False, compute.replace(
            'checkedSum(pointCount, (*it)->numberOfPoints(),', 'checkedSum(0U, (*it)->numberOfPoints(),')),
        ('spiral cache omitted from workspace', False, compute.replace(
            'sizeof(SpiralCacheData) + sizeof(DVec3Data)', 'sizeof(DVec3Data)')),
        ('spiral compute preflight omitted', False, compute.replace(
            'if (!validSpiralBudget(count, count == 0U ? 0U : 1U))', 'if (false)')),
        ('spiral dispatcher preflight omitted', False, compute.replace(
            'if (!validSpiralBudget(count, m_pts->size()))', 'if (false)')),
        ('final clean failure ignored', False, compute.replace(
            'return data.setClean(plug);', '(void)data.setClean(plug); return MS::kSuccess;')),
        ('Affect float span admission restored', False, compute.replace(
            'parms.valueRangeMax = maxVal;',
            'parms.valueRangeMax = maxVal; float oldSpan = 0.0f; '
            'if (!dommy::numeric::checkedFloatCast(double(maxVal)-minVal,oldSpan)) return MS::kInvalidParameter;')),
        ('Affect full-value output scale omitted', False, compute.replace(
            'std::array<double, 2>{{mapped, outputScale}}',
            'std::array<double, 2>{{mapped, outputScale*0.0+1.0}}')),
        ('Affect DPoint delta scale omitted', False, compute.replace(
            'double(rampVal), double(mParam->m_scaleArray[j]), candidate',
            'double(rampVal), double(mParam->m_scaleArray[j])*0.0+1.0, candidate')),
        ('initializer failure ignored', True, init.replace(
            'DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('initializer exception escapes', True, init.replace(
            'return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', 1)),
    ]
    for label, initializer, source in cases:
        assert source != (init if initializer else compute), label
        try:
            run(root, source, capture=True, suite='affect_spiral_init' if initializer else 'affect_spiral_compute',
                arguments=None if initializer else [])
        except subprocess.CalledProcessError as error:
            # Compilation failures do not count as detected regressions.
            assert error.returncode != 1 or 'ERROR: AddressSanitizer' in (error.stderr or ''), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        from check_group_transfer_safety import sdk_check
        sdk_check(root, sys.argv[2], NODES, 'Affect/spiral')
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, initializers(root), suite='affect_spiral_init',
            arguments=['--emit'] if '--emit' in sys.argv else None)
        if '--emit' not in sys.argv:
            run(root, compute_bodies(root), suite='affect_spiral_compute', arguments=[])
