#!/usr/bin/env python3
"""Vector merge control-flow checks, with explicit non-native VDB boundaries."""
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

NODE = 'DOpenVDBVectorMerge'
SOURCE = 'src/modules/volume/VDB/Edit/' + NODE + '.cpp'


def initializers(root):
    source = (root / SOURCE).read_text()
    return '\n'.join(declarations(NODE, source, True)) + '\n' + extract(
        source, 'MStatus ' + NODE + '::initialize(')


def compute_bodies(root):
    source = (root / SOURCE).read_text()
    util = (root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text()
    header = (root / 'src/modules/volume/VDB/Common/DVdbGridDispatch.h').read_text()
    string = (root / 'src/core/common/string/DStringOp.cpp').read_text()
    result = [extract(util, 'bool matchesGridName('), extract(util, 'bool containsGrid('),
              extract(util, 'bool getSelectedGrids(GridCPtrVec& grids, const std::string& selection, const DOpenVDBData& inputVdb)'),
              extract(string, 'void removeLastExtention('),
              extract(header, 'template<class Grid> const Grid* vdbGridCast('),
              extract(source, 'bool validMergeInputs(')]
    checked = re.search(r'template\s*<typename ScalarGridT>\s*const typename ScalarGridT::TreeType\* checkedMergeTree', source)
    result.append(extract(source, checked.group()))
    start = source.index('if (!mInGrid[0]')
    end = source.index('// Get the type of the output vector tree.', start)
    result.append('template <typename ScalarGridT> void readMergeTrees(const Grid* mInGrid[3]) {' +
                  source[start:end] + '\n(void)inTree;}')
    declared = declarations(NODE, source, False)
    declared[0] = declared[0].replace(' {',
        ' {static void addWarningMessage(DOpenVDBVectorMerge*,const char*){}', 1)
    result.extend(declared)
    result.append(extract(source, 'MStatus ' + NODE + '::compute(') +
                  '\nDOMMY_VDB_COMPUTE_CATCH("' + NODE + '")')
    return '\n'.join(result)


def guards(root):
    source = (root / SOURCE).read_text()
    compute = extract(source, 'MStatus ' + NODE + '::compute(')
    assert 'deepCopyGrid(' not in source
    assert not re.search(r'inputValue\([^;]+\)\.as', compute)
    assert 'SrcToGroup' not in source
    assert len(re.findall(r'inTree\[\d\] = checkedMergeTree<ScalarGridT>', source)) == 3
    assert compute.index('validMergeInputs(') < compute.index('ScalarGridMerger merger(')
    assert compute.index('merger.getGrid()') < compute.index('createOpenVDBOutputData(')
    assert 'DOMMY_VDB_COMPUTE_CATCH' in source and 'reportEntryPointException' in source


def mutations(root):
    init, body = initializers(root), compute_bodies(root)
    cases = [
        ('invalid VDB data accepted', False,
         body.replace('!inputVdb || !inputVdb->hasValidData()', '!inputVdb')),
        ('mixed types accepted before dispatch', False,
         body.replace('if (!validMergeInputs(xGrid, yGrid, zGrid))', 'if (false)')),
        ('inner typed cast unchecked', False,
         body.replace('if (!grid->template isType<ScalarGridT>())', 'if (false)')),
        ('Z tree uses Y', False,
         body.replace('inTree[2] = checkedMergeTree<ScalarGridT>(mInGrid[2])',
                      'inTree[2] = checkedMergeTree<ScalarGridT>(mInGrid[1])')),
        ('unselected same-name grid removed', False,
         body.replace('mergedSources.count(source.get()) != 0U',
             'std::any_of(mergedSources.begin(), mergedSources.end(), [&source](const Grid* g) '
             '{return g->getName() == source->getName();})')),
        ('legacy selector ignored', False,
         body.replace('word.compare(0, 6, "@name=") == 0', 'false')),
        ('numeric name treated as index', False,
         body.replace('matchesGridName(word.substr(6), gridName)',
                      'containsGrid({word.substr(6)}, gridName, gridIndex)')),
        ('UseXName uses Z', False, body.replace('std::string xName = xGrid->getName()',
                                               'std::string xName = zGrid->getName()')),
        ('input read failure ignored', False, body.replace('DOMMY_RETURN_IF_FAILURE(status);', '', 1)),
        ('bad vector type accepted', False,
         body.replace('vectorType < 0 || vectorType >= openvdb::NUM_VEC_TYPES', 'false')),
        ('dispatch result ignored', False,
         body.replace('!processVdbScalarGrid(getVdbGridType(*xGrid), *xGrid, merger)',
                      '(processVdbScalarGrid(getVdbGridType(*xGrid), *xGrid, merger), false)')),
        ('null merge result used', False, re.sub(r'if \(!merged\)\s*return MS::kFailure;', '', body)),
        ('output create failure ignored', False,
         body.replace('DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, outputVdb));',
                      '(void)createOpenVDBOutputData(pluginData, outputVdb);')),
        ('initializer failure ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);',
                                                           'CHECK_MSTATUS(stat);', 1)),
        ('initializer exception escaped', True, init.replace('return dommy::maya::reportEntryPointException(',
                                                            'throw; return dommy::maya::reportEntryPointException(', 1)),
    ]
    for label, initialization, source in cases:
        assert source != (init if initialization else body), label
        try:
            run(root, source, capture=True,
                suite='vdb_vector_merge_init' if initialization else 'vdb_vector_merge_compute',
                arguments=None if initialization else [], extra_flags=['-fsanitize=vptr'])
        except subprocess.CalledProcessError as error:
            # Compilation failures are NOT successful mutation detections.
            assert error.returncode != 1 or any(marker in (error.stderr or '') for marker in
                ('ERROR: AddressSanitizer', 'runtime error:')), label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def sdk_check(root, sdk):
    headers = ('MPxNode', 'MFnNumericAttribute', 'MFnEnumAttribute', 'MFnTypedAttribute',
               'MFnStringData', 'MTypeId', 'MDataBlock', 'MDataHandle')
    result = ['#include <maya/' + h + '.h>' for h in headers]
    result += ['#include <string>', '#include "common/utility/DComputeGuard.h"',
               '#include "common/utility/DStatusOperation.h"',
               'struct DOpenVDBData {static const MTypeId id;};const MTypeId DOpenVDBData::id(1);',
               'namespace openvdb {enum VecType {V0,V1,V2,V3,V4}; const int NUM_VEC_TYPES=5;'
               'struct GridBase {static std::string vecTypeExamples(VecType);};}']
    source = (root / SOURCE).read_text()
    declared = declarations(NODE, source, True)
    result += [declared[0].replace(':AttributeNode', ':MPxNode')] + [
        re.sub(r'\{"[^"]*"\}', '', item) for item in declared[1:]]
    result.append(extract(source, 'MStatus ' + NODE + '::initialize('))
    pattern = (r'const MDataHandle (\w+) = data\.inputValue\((\w+), &status\);\s*'
               r'DOMMY_RETURN_IF_FAILURE\(status\);\s*const [^;]+\1\.as\w+\(\)(?:\.asChar\(\))?;')
    readers = list(re.finditer(pattern, source))
    assert len(readers) == 8, len(readers)
    for index, match in enumerate(readers):
        body = match.group().replace('inputValue(' + match.group(2),
                                    'inputValue(' + NODE + '::' + match.group(2))
        target = re.findall(r'const [\w:]+ (\w+) =', body)[-1]
        result.append('MStatus reader' + str(index) + '(MDataBlock& data) try {MStatus status;'
                      + body + '\n(void)' + target +
                      ';return MS::kSuccess;} DOMMY_COMPUTE_CATCH("VectorMerge")')
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-unused-variable',
        '-Wno-deprecated-declarations', '-isystem', str(Path(sdk) / 'include'),
        '-I' + str(root / 'src/core')]
    with tempfile.TemporaryDirectory(prefix='dommy-vector-merge-sdk-') as directory:
        path = Path(directory) / 'merge.cpp'; path.write_text('\n'.join(result))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                  '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(path)], check=True)
    print('VectorMerge: complete initializer and eight input readers passed Maya SDK syntax/analysis')
    print('External VDB enum declarations only; native merge kernels and full compute unit are not covered')


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--emit' in sys.argv:
        run(root, initializers(root), suite='vdb_vector_merge_init', arguments=['--emit'])
    elif '--sdk' in sys.argv:
        sdk_check(root, sys.argv[2])
    elif '--mutations' in sys.argv:
        guards(root)
        mutations(root)
    else:
        guards(root)
        run(root, initializers(root), suite='vdb_vector_merge_init')
        run(root, compute_bodies(root), suite='vdb_vector_merge_compute', arguments=[],
            extra_flags=['-fsanitize=vptr'])
