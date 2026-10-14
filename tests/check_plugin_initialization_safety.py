#!/usr/bin/env python3
"""Test complete initialize bodies, frozen schemas, and optional SDK signatures.

This validates control flow/serialized attribute declarations, not Maya runtime
registration. Integration and experimental nodes stay behind their feature gates.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_active_cpp_safety import strip_comments_and_literals
from check_initializer_coverage import self_test as coverage_self_test, validate as validate_coverage


def generate(root, contract, baseline=None, sdk=False):
    result = []
    if sdk:
        for header in ('MFnNumericAttribute', 'MFnGenericAttribute', 'MFnTypedAttribute',
                       'MFnStringData', 'MFnEnumAttribute', 'MFnCompoundAttribute',
                       'MFnMatrixAttribute', 'MFnUnitAttribute', 'MFnMessageAttribute',
                       'MFnStringArrayData', 'MFnVectorArrayData', 'MRampAttribute', 'MMatrix', 'MTime', 'MTypeId'):
            result.append('#include <maya/' + header + '.h>')
        result.append('#include "common/utility/DStatusOperation.h"\n#include "common/utility/DMayaEntryPointGuard.h"')
        result.extend('struct '+name+' { static const MTypeId id; };\nconst MTypeId '+name+'::id('+str(value)+');'
                      for name, value in (('DPData',101),('DCurveData',102),('DMeshData',103),('DOpenVDBData',104)))
        result.append('namespace dommy { namespace generation { const int kMaxGeneratedTransformCount=1000000; } }')
        result.append('struct AttributeNode { static MObject outputGeom, message, mOutputForce; static MStatus addAttribute(MObject); static MStatus attributeAffects(MObject,MObject); };')
    result.append('enum { ' + ', '.join(name+'='+str(i+10) for i, name in enumerate(contract['constants'])) + ' };')
    for node in contract['nodes']:
        source = (root / node['path']).read_text()
        if not baseline and node.get('type_id'):
            actual_id = re.search(r'const\s+MTypeId\s+'+node['name']+r'::typeId\s*\([^;]+;', source)
            if not actual_id or ' '.join(actual_id.group().split()) != node['type_id']:
                raise AssertionError('Changed node ID declaration: '+node['name'])
        body = baseline[node['name']]['body'] if baseline else extract(source, 'MStatus '+node['name']+'::initialize(')
        result.append('struct '+node['name']+':AttributeNode { static MStatus initialize(); '+
                      ''.join('static MObject '+attr+';' for attr in node['attrs'])+' };')
        result.extend('MObject '+node['name']+'::'+attr+';' for attr in node['attrs'])
        result.append(body)
    if not sdk:
        result.append('struct Initializer { const char* name; MStatus (*initialize)(); std::uint64_t schema; int operations; };')
        result.append('const Initializer initializers[] = {')
        for node in contract['nodes']:
            result.append('{"'+node['name']+'", '+node['name']+'::initialize, 0x'+node.get('schema','0')+'ULL, '+str(node.get('operations',0))+'},')
        result.append('};')
    return '\n\n'.join(result)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--maya-sdk', type=Path)
    parser.add_argument('--mutations', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    coverage_self_test()
    validate_coverage(root)
    contract = json.loads((root / 'tests/fixtures/plugin_initializer_contract.json').read_text())
    for path in (root/'src').rglob('*.cpp'):
        source = path.read_text()
        for match in re.finditer(r'MStatus\s+\w+::initialize\s*\(', source):
            body = extract(source, match.group())
            if re.search(r'\bCHECK_MSTATUS\s*\(', strip_comments_and_literals(body)):
                raise AssertionError('Diagnostic-only initializer check returned: '+str(path))
    with tempfile.TemporaryDirectory(prefix='dommy-plugin-initialization-') as directory:
        temp = Path(directory)
        body = generate(root, contract)
        include = temp / 'plugin_initializers.inc'
        include.write_text(body)
        binary = temp / 'test'
        command = shlex.split(os.environ.get('CXX','clang++')) + [
            '-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',
            '-Wno-unused-variable','-Wno-unknown-pragmas',
            '-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all',
            '-I'+str(temp),'-I'+str(root/'tests/stubs'),'-I'+str(root/'src/core'),
            str(root/'tests/cpp/plugin_initialization_fixture.cpp'),'-o',str(binary)]
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1'))
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True, env=env)
        if args.mutations:
            mutations = (
                ('ignored create failure', body.replace('DOMMY_RETURN_IF_FAILURE(stat);','CHECK_MSTATUS(stat);',1)),
                ('ignored setter failure', body.replace('DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));','CHECK_MSTATUS(gAttr.setReadable(false));',1)),
                ('ignored addAttribute failure', body.replace('DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));','CHECK_MSTATUS(addAttribute(inDD));',1)),
                ('changed default value', body.replace('nAttr.setDefault(0.0,0.0,0.0)','nAttr.setDefault(1.0,0.0,0.0)',1)),
                ('VDB input dependency omitted', body.replace('DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, mOutputForce));', '', 1)),
                ('VDB grid selection dependency omitted', body.replace('DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, mOutputForce));', '', 1)),
                ('VDB dependency status ignored', body.replace('DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, mOutputForce));', '(void)attributeAffects(aVdbInput, mOutputForce);', 1)),
            )
            for label, changed in mutations:
                if changed == body: raise AssertionError('Mutation did not modify source: '+label)
                include.write_text(changed)
                subprocess.run(command,check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                failed = subprocess.run([str(binary)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                if failed.returncode == 0: raise AssertionError('Undetected mutation: '+label)
                print('Rejected mutation:',label)
        if args.maya_sdk:
            sdk_source=temp/'sdk_initializers.cpp'
            sdk_source.write_text(generate(root,contract,sdk=True))
            subprocess.run(shlex.split(os.environ.get('CXX','clang++')) + [
                '-std=c++14','-Wall','-Wextra','-Wpedantic','-Werror',
                '-Wno-unused-variable','-Wno-unknown-pragmas','-Wno-deprecated-declarations',
                '-isystem',str(args.maya_sdk/'include'),'-I'+str(root/'src/core'),
                '-fsyntax-only',str(sdk_source)],check=True)
            print('All initializer signatures compile against supplied Maya SDK.')


if __name__ == '__main__':
    main()
