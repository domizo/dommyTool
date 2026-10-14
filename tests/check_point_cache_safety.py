#!/usr/bin/env python3
"""DPCache value codecs, allocation preflight and Maya API failure boundaries."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

SOURCE = 'src/modules/point/DPoint/Utility/DPCache.cpp'


def initializer(source, base=None):
    declared = declarations('DPCache', source, True)
    if base:
        declared[0] = declared[0].replace(':AttributeNode', ':' + base)
        declared[1:] = [re.sub(r'\{"[^"]*"\}', '', line) for line in declared[1:]]
    return '\n'.join(declared + [extract(source, 'MStatus DPCache::initialize(')])


def values(source):
    root = Path(__file__).resolve().parent.parent
    budget = (root / 'src/core/common/utility/DPointPayloadBudget.h').read_text()
    result = [extract(budget, 'namespace dommy {'),
              'using CacheWorkspace = dommy::point::PointPayloadBudget;',
              'template <typename Source, typename Destination>\n' +
              extract(source, 'bool copyCacheMatrix(')]
    for name in ('reserveCachePoints', 'preflightCacheInput',
                 'cacheReadAttributeBytes', 'preflightCacheRead'):
        prefix = 'std::size_t ' if name.endswith('Bytes') else 'void '
        result.append(extract(source, prefix + name + '('))
    for prefix, signature in (
        ('template <typename T>', 'void validateCacheValue('),
        ('template <>', 'void validateCacheValue<std::string>('),
        ('template <typename T>', 'void writeDPAttributeToOpnVDBPointAttribute('),
        ('template <>', 'void writeDPAttributeToOpnVDBPointAttribute<MMatrix>('),
        ('template <typename T>', 'void writeOpenVDBPointAttributeToDPAttribute('),
        ('template <>', 'void writeOpenVDBPointAttributeToDPAttribute<MMatrix>('),
    ):
        result.append(prefix + '\n' + extract(source, signature))
    return '\n\n'.join(result)


def inputs(source):
    root = Path(__file__).resolve().parent.parent
    result = [extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                      'MStatus getValidInputDP(')]
    readers = re.findall(r'const DPData\* inputDPData = nullptr;\s*'
                         r'DOMMY_RETURN_IF_FAILURE\(getValidInputDP\(inDP, data, inputDPData\)\);', source)
    assert len(readers) == 2
    for index, reader in enumerate(readers):
        result.append('MStatus read_points_' + str(index) + '(MDataBlock& data) try {\n' + reader +
                      '\nreturn inputDPData ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);\n}' +
                      ' DOMMY_COMPUTE_CATCH("DPCache input boundary")')
    # Extract the complete consecutive handle/status/accessor statements, so
    # moving or deleting a check is observable to the failure-injection tests.
    for name, target in (('enabled', 'isEnableCache'), ('names', 'attributeNames'),
                         ('path', 'srcFileName'), ('time', 'currTime'),
                         ('create', 'createMissingCache')):
        pattern = (r'const MDataHandle ' + name + r'Handle\s*=[\s\S]*?\b' +
                   target + r'\s*=\s*' + name + r'Handle\.as\w+\(\);')
        fragment = re.search(pattern, source).group()
        result.append('MStatus read_' + name + '(MDataBlock& data) try { MStatus stat;\n' +
                      fragment + '\n(void)' + target + ';return MS::kSuccess;\n}' +
                      ' DOMMY_COMPUTE_CATCH("DPCache")')
    split = re.search(r'DOMMY_RETURN_IF_FAILURE\(attributeNames\.split\([^;]+;', source).group()
    result.append('MStatus read_split(MDataBlock&) try { CacheString attributeNames("a b");'
                  'MStringArray attrNameArray;\n' + split +
                  '\nreturn MS::kSuccess; } DOMMY_COMPUTE_CATCH("DPCache")')
    path = re.search(r'DOMMY_RETURN_IF_FAILURE\(fileObj\.setRawFullName\([^;]+;', source).group()
    result.append('MStatus read_filename(MDataBlock&) try { MFileObject fileObj;'
                  'const std::string filename="cache.vdb";\n' + path +
                  '\nreturn MS::kSuccess; } DOMMY_COMPUTE_CATCH("DPCache")')
    return '\n'.join(result)


def guards(source):
    assert 'DOpenVDBDataHeaders.H' not in source
    assert not re.search(r'\*\w+(?:\[[^]]+\])?\[(?:[4-9]|1[0-5])\]', source)
    assert source.count('copyCacheMatrix(') == 5
    assert 'std::vector<MMatrix> values = ' not in source
    assert 'std::vector<T> values = ' not in source
    assert source.index('preflightCacheInput(*inputDPData)') < source.index(
        'std::vector<openvdb::Vec3R> positions')
    assert source.index('preflightCacheRead(static_cast') < source.index('DPointList pts(')
    assert 'preflightCacheInput(*inputData)' in source
    assert source.count('attrsets, cacheWorkspace)') == 14
    assert 'DOMMY_RETURN_IF_FAILURE(fileObj.setRawFullName(' in source
    assert not re.search(r'inputValue\([^;]+\)\.as', source)
    compute = extract(source, 'MStatus DPCache::compute(')
    missing = compute.index('if (!fileObj.exists())')
    first_read = compute.index('getValidInputDP(')
    cache_read = compute.index('preflightCacheRead(')
    last_read = compute.rindex('getValidInputDP(')
    assert missing < first_read < cache_read < last_read
    assert compute.count('getValidInputDP(') == 2
    assert 'getInputDP(' not in compute


def sdk_check(root, source, sdk):
    headers = ('MPxNode', 'MPxData', 'MFnNumericAttribute', 'MFnGenericAttribute', 'MFnTypedAttribute',
               'MFnStringData', 'MFnUnitAttribute', 'MTypeId', 'MObject', 'MMatrix',
               'MDataBlock', 'MDataHandle', 'MTime', 'MStringArray', 'MFileObject')
    preamble = '\n'.join('#include <maya/' + h + '.h>' for h in headers)
    preamble += '\n#include <cmath>\n#include "common/utility/DComputeGuard.h"\n'
    preamble += '#include "common/utility/DStatusOperation.h"\n'
    preamble += '#include "common/utility/DMayaPluginData.h"\n'
    preamble += 'struct DPData:MPxData {static const MTypeId id;bool hasValidPointTopology()const;};const MTypeId DPData::id(1);\n'
    preamble += 'void registerDPointCacheAttributeTypes();\n'
    matrix = 'template <typename Source, typename Destination>\n' + extract(source, 'bool copyCacheMatrix(')
    controls = inputs(source).replace('CacheString', 'MString')
    controls = re.sub(r'\b(inDP|aEnableCache|aAttributeNames|aCachePath|aCurrentTime|aCreateMissingCache)\b',
                      r'DPCache::\1', controls)
    body = preamble + initializer(source, 'MPxNode') + '\n' + matrix + '\n' + controls
    body += '\nbool convert(const MMatrix& a, MMatrix& b){return copyCacheMatrix(a,b);}\n'
    body += 'MStatus filePath(MFileObject& f,const MString& p){return f.setRawFullName(p);}\n'
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src/core')]
    with tempfile.TemporaryDirectory(prefix='dommy-cache-sdk-') as directory:
        path = Path(directory) / 'cache.cpp'
        path.write_text(body)
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
            '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(path)], check=True)
    print('DPCache: initializer, five controls, two checked source-input fragments, split/path and matrix helper passed SDK syntax/analysis')
    print('OpenVDB codecs and complete translation unit require the separate pinned dependency stack')


def mutations(root, source):
    cases = [
        ('matrix transpose', 'point_cache_values', values(source).replace(
            'source[row][column]', 'source[column][row]')),
        ('nonfinite matrix accepted', 'point_cache_values', values(source).replace(
            'if (!std::isfinite(value))', 'if (false)')),
        ('aggregate budget omitted', 'point_cache_values', values(source).replace(
            '!dommy::attribute::validArrayConversionBudget(1U, total)', 'false')),
        ('aggregate budget reset', 'point_cache_values', values(source).replace(
            'checkedSum(bytes_, bytes,', 'checkedSum(0U, bytes,')),
        ('matrix default omitted', 'point_cache_values', values(source).replace(
            'copyCacheMatrix(defaultValue, finDdefaultValue)', 'true')),
        ('bad matrix input type accepted', 'point_cache_values', values(source).replace(
            'if (!attribute)', 'if (false)')),
        ('failed input handle read', 'point_cache_inputs', inputs(source).replace(
            'DOMMY_RETURN_IF_FAILURE(stat);', '', 1)),
        ('split failure ignored', 'point_cache_inputs', inputs(source).replace(
            "DOMMY_RETURN_IF_FAILURE(attributeNames.split(' ', attrNameArray));",
            "(void)attributeNames.split(' ', attrNameArray);")),
        ('file path failure ignored', 'point_cache_inputs', inputs(source).replace(
            'DOMMY_RETURN_IF_FAILURE(fileObj.setRawFullName(filename.c_str()));',
            '(void)fileObj.setRawFullName(filename.c_str());')),
        ('initializer status ignored', 'point_cache_init', initializer(source).replace(
            'DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('initializer exception escapes', 'point_cache_init', initializer(source).replace(
            'return dommy::maya::reportEntryPointException(',
            'throw; return dommy::maya::reportEntryPointException(', 1)),
    ]
    for label, suite, body in cases:
        try:
            run(root, body, capture=True, suite=suite,
                arguments=None if suite.endswith('_init') else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or any(marker in (error.stderr or '') for marker in
                ('ERROR: AddressSanitizer', 'runtime error:')), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    source = (root / SOURCE).read_text()
    guards(source)
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_check(root, source, sys.argv[2])
    elif '--mutations' in sys.argv:
        mutations(root, source)
    else:
        run(root, initializer(source), suite='point_cache_init')
        run(root, values(source), suite='point_cache_values', arguments=[])
        run(root, inputs(source), suite='point_cache_inputs', arguments=[])
        body = inputs(source)
        old = 'DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, inputDPData));'
        matches = list(re.finditer(re.escape(old), body))
        assert len(matches) == 2
        for index, match in enumerate(matches):
            changed = body[:match.start()] + (
                'if (!getValidInputDP(inDP, data, inputDPData)) return MS::kFailure;') + body[match.end():]
            try:
                run(root, changed, capture=True, suite='point_cache_inputs', arguments=[])
            except subprocess.CalledProcessError as error:
                assert Path(error.cmd[0]).name == 'point_cache_inputs_tests', 'Compile failure'
                assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr
                print('Detected cache source-input status mutation:', index, flush=True)
            else:
                raise AssertionError('Cache source-input status mutation was not detected')
