#!/usr/bin/env python3
"""Group input boundaries and checked point/curve/mesh attribute transfer."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

NODES = {
    'DPGroup': 'src/core/DDataUtility/Group/DPGroup.cpp',
    'DCGroup': 'src/modules/curve/DCurve/Group/DCGroup.cpp',
    'DMGroup': 'src/modules/mesh/DMesh/Data/DMGroup.cpp',
    'DPAttributeTransfer': 'src/core/DDataUtility/Attribute/DPAttributeTransfer.cpp',
    'DCAttributeTransfer': 'src/modules/curve/DCurve/Attribute/DCAttributeTransfer.cpp',
    'DMAttributeTransfer': 'src/modules/mesh/DMesh/Attribute/DMAttributeTransfer.cpp',
}


def initializers(root):
    result = []
    # Keep enum values coupled to production; the frozen trace detects schema drift.
    for path, types in (
            ('src/modules/point/DPoint/DType.h', ('DComponentType',)),
            ('src/modules/curve/DCurve/DCurveUtil.h', ('DC_VECTOR_TYPE',)),
            ('src/core/Data/DGroupCommon.h',
             ('DGroupNumericType', 'DGroupBoundType', 'DGroupExistActionType'))):
        source = (root / path).read_text()
        for name in types:
            match = re.search(r'\benum\s*\{[^}]+\};\s*typedef\s+short\s+' + name + r';', source)
            assert match is not None, name
            result.append(match.group(0))
    result.extend([extract((root / 'src/core/Data/DGroupCommon.h').read_text(), 'struct DGroupParams') + ';',
                   'MStatus validateGroupParams(const DGroupParams& p, bool) {'
                   'return p.isBound && p.boundingType==DGRP_BOUND_OBJECT && !p.m_objHnd.valid'
                   '?MStatus(MS::kInvalidParameter):MStatus(MS::kSuccess);}'])
    for name, path in NODES.items():
        source = (root / path).read_text()
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        group = name.endswith('Group')
        if group:
            result.append('struct ' + name + 'Manip {static MStatus addToManipConnectTable(int)'
                          ' {return operation("manip.connect");}};')
        result.append('struct ' + name + ':AttributeNode {static const int typeId=1;'
                      'static MStatus initialize(); MObject thisMObject() const {return {"node"};}' +
                      ('MStatus evalParms(MDataBlock&,DGroupParams&);' if group else '') +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + '{"' + a + '"};' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
        if group:
            result.append(extract(source, 'MStatus ' + name + '::evalParms('))
    return '\n\n'.join(result)


def compute_bodies(root):
    result = ['namespace dommy { namespace transfer {\n' + extract(
        (root / 'src/core/common/utility/DAttributeTransferPlan.h').read_text(),
        'inline bool buildAttributeApplicationBudget(') + '\n} }']
    result.extend(extract((root / ('src/core/common/utility/' + header)).read_text(), 'namespace dommy {')
                  for header in ('DMayaNameList.h', 'DMayaAttributeTransfer.h'))
    result.append(extract((root / 'src/modules/point/DPoint/Common/DPointAttributeTransfer.h').read_text(),
                          'namespace dommy {'))
    for path, function in (
            ('src/modules/mesh/DMesh/common/DMeshUtil.cpp', 'getValidInputDM'),
            ('src/modules/curve/DCurve/DCurveUtil.cpp', 'getValidInputDC')):
        result.append(extract((root / path).read_text(), 'MStatus ' + function + '('))
    for name, path in NODES.items():
        if not name.endswith('Transfer'):
            continue
        source = (root / path).read_text()
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ' {MStatus compute(const MPlug&, MDataBlock&);'
                      'MObject thisMObject() const {return {"node"};}' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + '{"' + a + '"};' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def sdk_check(root, sdk, nodes=None, label='Group/transfer', boundary_source=None):
    nodes = NODES if nodes is None else nodes
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')]
    command += ['-I' + str(path) for path, _, _ in os.walk(root / 'src')]
    command += ['-isystem', str(root / 'third_party/libnoise/source'), '-idirafter', str(root / 'tests/stubs')]
    if boundary_source is not None:
        for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                        '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
            subprocess.run(command + flags + ['-x', 'c++', '-'], input=boundary_source,
                           text=True, check=True)
        print(label + ': production helper SDK syntax/analysis passed; no host runtime')
    if not nodes:
        return
    dependency = Path(os.environ.get('DOMMY_LEGACY_DEPENDENCY_ROOT',
                      str(Path(os.environ.get('TMPDIR', '/tmp')) / 'dommytool-legacy-dependencies')))
    nanoflann = dependency / 'nanoflann-f2efec0110eb68edf8cae90fce394a5044bf3db8/include'
    seexpr = dependency / 'SeExpr-a5f02bb03199630759b0b94a64f37ce56c08675a/src/SeExpr'
    if not (nanoflann / 'nanoflann.hpp').is_file() or not (seexpr / 'SeExpression.h').is_file():
        print('SKIP ' + label + ' SDK units: pinned SeExpr/nanoflann headers unavailable')
        return
    command += ['-isystem', str(nanoflann), '-isystem', str(seexpr)]
    for path in nodes.values():
        source = root / path
        subprocess.run(command + ['-fsyntax-only', str(source)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                  '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(source)], check=True)
    print('%s: %d complete SDK units passed syntax/analysis (limited Imath test double)' %
          (label, len(nodes)))


def object_boundary_sdk(root, sdk):
    source = (root / NODES['DPGroup']).read_text()
    group = (root / 'src/core/Data/DGroupCommon.h').read_text()
    types = re.search(r'enum\s*\{[^}]+\};\s*typedef short DGroupBoundType;', group).group(0)
    code = '''
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MPlug.h>
#include <maya/MObject.h>
#include "common/utility/DStatusOperation.h"
MObject thisMObject();
struct ObjectParams {bool isBound; short boundingType; MDataHandle m_objHnd;};
'''
    code += types + '\nMStatus readGroupObject(MDataBlock& data, const MObject& aInObject, ObjectParams& parms) {'
    code += 'MStatus stat;' + extract(source, 'if (parms.isBound && parms.boundingType == DGRP_BOUND_OBJECT)')
    code += 'return MS::kSuccess;}'
    sdk_check(root, sdk, nodes={}, label='Group object acquisition', boundary_source=code)


def mutations(root):
    init, compute = initializers(root), compute_bodies(root)
    cases = [
        ('object connection failure ignored', 'group_transfer_init', init.replace(
            'const bool objectConnected = objPlug.isConnected(&stat);\n        DOMMY_RETURN_IF_FAILURE(stat);',
            'const bool objectConnected = objPlug.isConnected(&stat);', 1)),
        ('group handle read before status', 'group_transfer_init', init.replace(
            'DOMMY_RETURN_IF_FAILURE(stat);\n    parms.m_grp_name = aGroupNameHandle.asString().asChar();',
            'parms.m_grp_name = aGroupNameHandle.asString().asChar();\n    DOMMY_RETURN_IF_FAILURE(stat);', 1)),
        ('initializer failure ignored', 'group_transfer_init', init.replace(
            'DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('manipulator registration failure ignored', 'group_transfer_init', init.replace(
            'DOMMY_RETURN_IF_FAILURE(DPGroupManip::addToManipConnectTable(typeId));',
            '(void)DPGroupManip::addToManipConnectTable(typeId);')),
        ('initializer exception escapes', 'group_transfer_init', init.replace(
            'return dommy::maya::reportEntryPointException(',
            'throw; return dommy::maya::reportEntryPointException(', 1)),
        ('source acquisition failure ignored', 'attribute_transfer_compute', compute.replace(
            'const MDataHandle sourceHandle = data.inputValue(aTargetDP, &status);\n        DOMMY_RETURN_IF_FAILURE(status);',
            'const MDataHandle sourceHandle = data.inputValue(aTargetDP, &status);')),
        ('failed split consumed', 'attribute_transfer_compute', compute.replace(
            "DOMMY_RETURN_IF_FAILURE(value.split(' ', names));", "(void)value.split(' ', names);")),
        ('duplicate field applications', 'attribute_transfer_compute', compute.replace(
            'if (!attribute || !seen.insert(name).second) continue;', 'if (!attribute) continue;')),
        ('aggregate application work omitted', 'attribute_transfer_compute', re.sub(
            r'\(!validNames.empty\(\) && !dommy::transfer::buildAttributeApplicationBudget\(\s*'
            r'plan, validNames.size\(\), components, [^\n]+, workItems\)\)', 'false', compute)),
        ('vector components counted as scalars', 'attribute_transfer_compute', compute.replace(
            'case DP_ATTR_VEC3I: case DP_ATTR_VEC3F: case DP_ATTR_VEC3D: return 3U;',
            'case DP_ATTR_VEC3I: case DP_ATTR_VEC3F: case DP_ATTR_VEC3D: return 1U;')),
        ('partition visits unbounded', 'attribute_transfer_compute', compute.replace(
            '!generation::checkedProduct(partitionCount, nameCount,\n'
            '            generation::kMaxWorkItemCount, fieldVisits)', 'false')),
    ]
    for label, suite, source in cases:
        assert source != (init if suite == 'group_transfer_init' else compute), label
        try:
            run(root, source, capture=True, suite=suite,
                arguments=None if suite == 'group_transfer_init' else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or 'ERROR: AddressSanitizer' in (error.stderr or ''), label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_check(root, sys.argv[2])
        object_boundary_sdk(root, sys.argv[2])
    elif len(sys.argv) == 2 and sys.argv[1] == '--mutations':
        mutations(root)
    elif len(sys.argv) == 2 and sys.argv[1] == '--emit':
        run(root, initializers(root), suite='group_transfer_init', arguments=['--emit'])
    else:
        run(root, initializers(root), suite='group_transfer_init')
        run(root, compute_bodies(root), suite='attribute_transfer_compute', arguments=[])
