#!/usr/bin/env python3
"""Shared curve/mesh metadata operations and frozen node initialization."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys
import contextlib
import io
import tempfile
from types import SimpleNamespace
from unittest.mock import patch

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

NODES = {
    'DCGroupFromDP': 'src/modules/curve/DCurve/Group/DCGroupFromDP.cpp',
    'DCGroupDelete': 'src/modules/curve/DCurve/Group/DCGroupDelete.cpp',
    'DMAttributeCopy': 'src/modules/mesh/DMesh/Attribute/DMAttributeCopy.cpp',
    'DCAttributeCopy': 'src/modules/curve/DCurve/Attribute/DCAttributeCopy.cpp',
}


def initializers(root):
    result = []
    for name, path in NODES.items():
        source = (root / path).read_text()
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ':AttributeNode {static MStatus initialize();' +
                      ''.join('static MObject ' + attr + ';' for attr in attrs) + '};')
        result.extend('MObject ' + name + '::' + attr + ';' for attr in attrs)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n\n'.join(result)


def compute_bodies(root):
    result = [extract((root / ('src/core/common/utility/' + header)).read_text(), 'namespace dommy {')
              for header in ('DGroupGenerationPlan.h', 'DMayaNameList.h', 'DAttributePartitionCopy.h')]
    for path, function in (
            ('src/modules/mesh/DMesh/common/DMeshUtil.cpp', 'getValidInputDM'),
            ('src/modules/curve/DCurve/DCurveUtil.cpp', 'getValidInputDC')):
        result.append(extract((root / path).read_text(), 'MStatus ' + function + '('))
    for name, path in NODES.items():
        source = (root / path).read_text()
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ' {MStatus compute(const MPlug&, MDataBlock&);' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + '{"' + a + '"};' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)


def dependency_boundaries(root):
    umbrella=(root/'src/modules/curve/DCurve/DCurveDataHeaders.h').read_text()
    assert 'DCurveSeExpr.h' not in umbrella,'Curve data headers must not require an expression SDK'
    for name in ('DCAttributeCopy','DMAttributeCopy'):
        source=(root/NODES[name]).read_text()
        assert '#include "DPData.h"' in source and 'DPointDataHeaders.h' not in source,name
    for path in ('src/modules/mesh/mesh/DPolyWire.cpp','src/modules/mesh/DMesh/Generate/DMWireGen.cpp'):
        assert '#include "DCurveSeExpr.h"' in (root/path).read_text(),path

def sdk_check(root, sdk):
    dependency = Path(os.environ.get('DOMMY_LEGACY_DEPENDENCY_ROOT',
                      str(Path(os.environ.get('TMPDIR', '/tmp')) / 'dommytool-legacy-dependencies')))
    nanoflann = dependency / 'nanoflann-f2efec0110eb68edf8cae90fce394a5044bf3db8/include'
    seexpr = dependency / 'SeExpr-a5f02bb03199630759b0b94a64f37ce56c08675a/src/SeExpr'
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')]
    command += ['-I' + str(path) for path, _, _ in os.walk(root / 'src')]
    command += ['-isystem', str(root / 'third_party/libnoise/source'),
                '-idirafter', str(root / 'tests/stubs')]
    nodes = dict(NODES)
    skipped=[]
    if (nanoflann / 'nanoflann.hpp').is_file() and (seexpr / 'SeExpression.h').is_file():
        command += ['-isystem', str(nanoflann), '-isystem', str(seexpr)]
    else:
        del nodes['DCGroupFromDP']
        skipped.append('DCGroupFromDP')
        print('SKIP complete DCGroupFromDP SDK unit: pinned nanoflann/SeExpr unavailable')
    passed,failed=[],[]
    for name,path in nodes.items():
        source = root / path
        syntax=subprocess.run(command + ['-fsyntax-only', str(source)])
        if syntax.returncode:
            failed.append(name);print('FAIL SDK syntax:',name,flush=True);continue
        analysis=subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                  '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(source)])
        if analysis.returncode:
            failed.append(name);print('FAIL SDK analysis:',name,flush=True);continue
        passed.append(name)
    print('Primitive metadata SDK: passed=%d skipped=%d failed=%d (limited Imath test double)' %
          (len(passed),len(skipped),len(failed)),flush=True)
    if skipped:print('Dependency skips are not successful native verification:',', '.join(skipped))
    if failed:raise SystemExit(1)

def report_tests(root):
    # Exercise orchestration only, not compilation. Actual SDK checks remain
    # separate; a failed unit must not hide later units or become a success.
    with tempfile.TemporaryDirectory(prefix='dommy-sdk-report-') as dependency:
        for failure in ('none','syntax','analysis'):
            calls=[]
            def compile_double(command,**kwargs):
                calls.append(command)
                selected=command[-1].endswith('/DCGroupDelete.cpp')
                failed=selected and (failure=='syntax' and '-fsyntax-only' in command or
                                    failure=='analysis' and '--analyze' in command)
                return SimpleNamespace(returncode=1 if failed else 0)
            output=io.StringIO();exit_code=0
            with patch.dict(os.environ,{'DOMMY_LEGACY_DEPENDENCY_ROOT':dependency}),\
                 patch.object(subprocess,'run',compile_double),contextlib.redirect_stdout(output):
                try:sdk_check(root,'declared-sdk-boundary')
                except SystemExit as error:exit_code=error.code
            assert exit_code==(0 if failure=='none' else 1)
            expected='passed=3 skipped=1 failed=0' if failure=='none' else 'passed=2 skipped=1 failed=1'
            assert expected in output.getvalue()
            for name in ('DMAttributeCopy','DCAttributeCopy'):
                for mode in ('-fsyntax-only','--analyze'):
                    assert any(command[-1].endswith('/'+name+'.cpp') and mode in command for command in calls)
    print('SDK report orchestration: success/skip/failure remain distinct; later units still checked')


def mutations(root):
    init, compute = initializers(root), compute_bodies(root)
    cases = [
        ('default value discarded', 'primitive_metadata_compute', compute.replace(
            '(partition, defaultValue)', '(partition)')),
        ('partition size not validated', 'primitive_metadata_compute', compute.replace(
            'if (count != totalCount) return false;', '')),
        ('dynamic defaults not budgeted', 'primitive_metadata_compute', compute.replace(
            '(partitions > 1U && !addDynamicValueBudget(defaultValue, candidateBudget))', 'false')),
        ('copy work budget ignored', 'primitive_metadata_compute', compute.replace(
            '!generation::checkedSum(candidateBudget.workItems, totalCount,\n'
            '            generation::kMaxWorkItemCount, candidateBudget.workItems)', 'false')),
        ('failed commit ignored', 'primitive_metadata_compute', compute.replace(
            'if (!commitStagedAttribute(**owner, name, candidates[index], (*owner)->numberOfPoints()))',
            'if (false && !commitStagedAttribute(**owner, name, candidates[index], (*owner)->numberOfPoints()))')),
        ('group budget skipped', 'primitive_metadata_compute', compute.replace(
            'if (!validGeneratedGroupBudget(input, primitiveCount, candidate)) return false;',
            'if (false && !validGeneratedGroupBudget(input, primitiveCount, candidate)) return false;')),
        ('split failure ignored', 'primitive_metadata_compute', compute.replace(
            "DOMMY_RETURN_IF_FAILURE(value.split(' ', names));", "(void)value.split(' ', names);")),
        ('delete work budget ignored', 'primitive_metadata_compute', compute.replace(
            'domain == 0U ? 1U : curveCount,', '1U,')),
        ('target read failure ignored', 'primitive_metadata_compute', compute.replace(
            'const MDataHandle targetHandle = data.inputValue(aTargetDP, &status);\n        DOMMY_RETURN_IF_FAILURE(status);',
            'const MDataHandle targetHandle = data.inputValue(aTargetDP, &status);')),
        ('initializer failure ignored', 'primitive_metadata_init', init.replace(
            'DOMMY_RETURN_IF_FAILURE(status);', 'CHECK_MSTATUS(status);', 1)),
        ('initializer exception escapes', 'primitive_metadata_init', init.replace(
            'return dommy::maya::reportEntryPointException(',
            'throw; return dommy::maya::reportEntryPointException(', 1)),
    ]
    for label, suite, source in cases:
        assert source != (init if suite.endswith('_init') else compute), label
        try:
            run(root, source, capture=True, suite=suite,
                arguments=None if suite.endswith('_init') else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or 'ERROR: AddressSanitizer' in (error.stderr or ''), label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    dependency_boundaries(root)
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_check(root, sys.argv[2])
    elif len(sys.argv) == 2 and sys.argv[1] == '--mutations':
        mutations(root)
    else:
        report_tests(root)
        run(root, initializers(root), suite='primitive_metadata_init')
        run(root, compute_bodies(root), suite='primitive_metadata_compute', arguments=[])
