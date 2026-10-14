#!/usr/bin/env python3
"""DPRay input, vector-field, tolerance and initialization contracts."""
from pathlib import Path
import re
import subprocess
import sys
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

NODE = 'src/modules/point/DPoint/Utility/DPRay.cpp'
UTILITY = 'src/core/common/utility/DPRayUtility.cpp'

def enums(root):
    return re.search(r'enum\s*\{[^}]+\};\s*typedef short DPRayType;',
        (root / UTILITY).with_suffix('.h').read_text()).group(0)

def initializers(root):
    source = (root / NODE).read_text()
    return '\n\n'.join([enums(root)] + declarations('DPRay', source, True) +
        [extract(source, 'MStatus DPRay::initialize(')])

def compute_bodies(root):
    header = (root / UTILITY).with_suffix('.h').read_text()
    result = [enums(root), extract(header, 'struct DPRayParams') + ';',
              extract(header, 'namespace dommy {')]
    validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    result.append('namespace dommy { namespace maya {\n' + '\n'.join(
        extract(validation, 'inline bool ' + name + '(') for name in (
            'isFinitePoint', 'isFloatRepresentableVector', 'normalizedVector')) + '\n}}')
    result += [extract((root / 'src/core/common/utility/DPointForceBudget.h').read_text(), 'namespace dommy {'),
               extract((root / 'src/modules/point/DPoint/Common/DPointForceOutput.h').read_text(), 'namespace dommy {')]
    result.append('namespace dommy { namespace group {\ntemplate<typename Iterator>\n' +
        extract((root / 'src/core/common/utility/DGroupPublication.h').read_text(),
                'bool replacePointGroupPartitions(') + '\n}}')
    utility = (root / UTILITY).read_text()
    result.append(utility[utility.index('namespace {'):])
    source = (root / NODE).read_text()
    declared = declarations('DPRay', source, False)
    declared[0] = declared[0].replace(' {', ' {MObject thisMObject()const{return {"node"};}' +
        'MString name()const{return "DPRay";}MStatus evalRayParms(MDataBlock&,DPRayParams&);', 1)
    result += declared + [extract(source, 'MStatus DPRay::evalRayParms('),
                         extract(source, 'MStatus DPRay::compute(') + '\nDOMMY_COMPUTE_CATCH("DPRay")']
    return '\n\n'.join(result)

def mutations(root):
    init, compute = initializers(root), compute_bodies(root)
    cases = [
        ('failed control handle consumed', False, compute.replace(
            'if (!stat) return stat;\n    parms.m_type = aMethodHandle.asShort();',
            'parms.m_type = aMethodHandle.asShort();')),
        ('failed source handle consumed', False, compute.replace(
            'if (!stat) return stat;\n    dommy::point::PointForceInput input;',
            'dommy::point::PointForceInput input;')),
        ('invalid direction replaced with zero vectors', False, compute.replace(
            'dommy::attribute::convertVector3Attribute(\n'
            '                                (*cIt)->getAttr(vecAttrName),\n'
            '                                (*cIt)->numberOfPoints(), directions)',
            '(directions.assign((*cIt)->numberOfPoints(), DVec3Data()), true)')),
        ('mixed direction fallback removed', False, compute.replace(
            'if (vecList.size() == outDMeshes->numberOfMesh())', 'if (!vecList.empty())')),
        ('mesh tolerance preflight omitted', False, compute.replace(
            'if (!dommy::numeric::checkedFloatCast(params.mRayTolerance, meshTolerance))',
            'if (false && !dommy::numeric::checkedFloatCast(params.mRayTolerance, meshTolerance))')),
        ('mesh tolerance kernel check omitted', False, compute.replace(
            'if (!dommy::numeric::checkedFloatCast(params.mRayTolerance, tolerance) ||\n'
            '        tolerance < 0.0f)',
            'tolerance = static_cast<float>(params.mRayTolerance);\n    if (false)')),
        ('double tolerance range restricted in other modes', False, compute.replace(
            'if (params.m_type == DP_RAY_PROJECTION && params.m_objectType == MFnData::kMesh) {',
            'if (true) {')),
        ('final clean failure ignored', False, compute.replace(
            'return stat ? data.setClean(plug) : stat;',
            'if (stat) (void)data.setClean(plug); return stat;')),
        ('initializer failure ignored', True, init.replace(
            'DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('initializer exception escapes', True, init.replace(
            'return dommy::maya::reportEntryPointException(',
            'throw; return dommy::maya::reportEntryPointException(', 1)),
    ]
    for label, initializer, source in cases:
        assert source != (init if initializer else compute), label
        try:
            run(root, source, capture=True, suite='ray_init' if initializer else 'ray_compute',
                arguments=None if initializer else [])
        except subprocess.CalledProcessError as error:
            # Only an executed test failure counts, never a compiler failure.
            assert error.returncode != 1 or 'ERROR: AddressSanitizer' in (error.stderr or ''), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)

if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        from check_group_transfer_safety import sdk_check
        sdk_check(root, sys.argv[2], {'DPRay': NODE, 'DPRayUtility': UTILITY}, 'Ray')
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, initializers(root), suite='ray_init',
            arguments=['--emit'] if '--emit' in sys.argv else None)
        if '--emit' not in sys.argv:
            run(root, compute_bodies(root), suite='ray_compute', arguments=[])
