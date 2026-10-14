#!/usr/bin/env python3
"""Transform/split control flow and three initializers; native VDB is a boundary."""
from pathlib import Path
import hashlib
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
from check_active_cpp_safety import strip_comments_and_literals

FOLDER = 'src/modules/volume/VDB/Edit/'
NAMES = ('DOpenVDBTransform', 'DOpenVDBVectorSplit', 'DOpenVDBCopy')


def initializers(root):
    result = []
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        result.extend(declarations(name, source, True))
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n'.join(result)


def compute_bodies(root):
    util = (root / 'src/modules/volume/VDB/Common/DOpenVDBUtil.cpp').read_text()
    header = (root / 'src/modules/volume/VDB/Common/DVdbGridDispatch.h').read_text()
    result = [extract(util, 'bool matchesGridName('), extract(util, 'bool containsGrid('),
        extract(util, 'bool\ngetSelectedGrids('),
        extract(util, 'bool getSelectedGrids('),
        extract(header, 'inline int getVdbGridTupleSize(DVdbGridType type)')]
    for name in NAMES[:2]:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        if name == 'DOpenVDBTransform':
            result.append(extract(source, 'bool hasFiniteAffineMapCache('))
        result.extend(declarations(name, source, False))
        result.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_VDB_COMPUTE_CATCH("' + name + '")')
    return '\n'.join(result)


def token_hash(source):
    return hashlib.sha256(' '.join(re.findall(r'\w+|[^\s]',
        strip_comments_and_literals(source))).encode()).hexdigest()


def guards(root):
    for name in NAMES[:2]:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        body = extract(source, 'MStatus ' + name + '::compute(')
        assert not re.search(r'inputValue\([^;]+\)\.as', body)
        assert 'hasValidData()' in body
        assert 'publishOpenVDBOutput(' in body
        assert 'reportEntryPointException' in source
    split = (root / (FOLDER + 'DOpenVDBVectorSplit.cpp')).read_text()
    body = extract(split, 'MStatus DOpenVDBVectorSplit::compute(')
    assert body.index('outputCount > dommy::vdb::kMaxVdbGridCount') < body.index('VectorGridSplitter splitter(')
    # These native/unmodified bodies were compared to the pre-edit source.
    # Intentional future changes require a fresh compatibility review.
    assert token_hash(extract(split, 'class VectorGridSplitter')) == \
        '5ba1affda0c447a63c176619e1fe81aa6d6685c4453e509ced3ed866708101af'
    copy = (root / (FOLDER + 'DOpenVDBCopy.cpp')).read_text()
    assert token_hash(extract(copy, 'MStatus DOpenVDBCopy::compute(')) == \
        'e3cb4861783beaf93b3e7ee4a275ef95dd64ec8f9abccdbfc840c3f438fc7552'


def sdk_check(root, sdk):
    result = ['#include <maya/' + name + '.h>' for name in (
        'MPxNode', 'MFnNumericAttribute', 'MFnTypedAttribute', 'MFnStringData',
        'MTypeId', 'MDataBlock', 'MDataHandle', 'MFloatVector')]
    result += ['#include <string>', '#include "common/utility/DComputeGuard.h"',
               '#include "common/utility/DStatusOperation.h"',
               'struct DOpenVDBData {static const MTypeId id;}; const MTypeId DOpenVDBData::id(1);']
    reader_count = 0
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        declared = declarations(name, source, True)
        result += [declared[0].replace(':AttributeNode', ':MPxNode')] + [
            re.sub(r'\{"[^"]*"\}', '', item) for item in declared[1:]]
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
        if name == 'DOpenVDBCopy':
            continue
        pattern = (r'const MDataHandle (\w+) =\s*data\.inputValue\((\w+), &(status|stat)\);\s*'
                   r'(?:DOMMY_RETURN_IF_FAILURE\(\3\);|if \(!\3\)\s*return \3;)\s*'
                   r'const [^;]+\1\.as\w+\(\)(?:\.asChar\(\))?;')
        readers = list(re.finditer(pattern, source))
        assert len(readers) == (7 if name == 'DOpenVDBTransform' else 3), (name, len(readers))
        for match in readers:
            body = match.group().replace('inputValue(' + match.group(2),
                                        'inputValue(' + name + '::' + match.group(2))
            target = re.findall(r'const [\w:]+ (\w+) =', body)[-1]
            result.append('MStatus reader' + str(reader_count) + '(MDataBlock& data) try {MStatus ' +
                          match.group(3) + ';' + body + '\n(void)' + target +
                          ';return MS::kSuccess;} DOMMY_COMPUTE_CATCH("VDB edits")')
            reader_count += 1
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-unused-variable',
        '-Wno-deprecated-declarations', '-isystem', str(Path(sdk) / 'include'),
        '-I' + str(root / 'src/core')]
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-edit-sdk-') as directory:
        path = Path(directory) / 'edit.cpp'
        path.write_text('\n'.join(result))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                  '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(path)], check=True)
    print('VDB edits: three complete initializers and ten input readers passed Maya SDK syntax/analysis')
    print('Native VDB maps/kernels and complete compute translation units are not covered')


def mutations(root):
    init, body = initializers(root), compute_bodies(root)
    cases = [
        ('invalid input payload accepted', False, body.replace('!inputVdb || !inputVdb->hasValidData()', '!inputVdb')),
        ('transform handle status ignored', False, body.replace('DOMMY_RETURN_IF_FAILURE(status);', '', 1)),
        ('split handle status ignored', False, re.sub(r'if \(!stat\)\s*return stat;', '', body, count=1)),
        ('float multiplication before promotion', False,
         re.sub(r'static_cast<double>\(scale\[(\d)\]\) \* uniformScale', r'static_cast<double>(scale[\1] * uniformScale)', body)),
        ('inverse nonfinite accepted', False,
         body.replace('if (!dommy::numeric::isFiniteAffineTransform(inverse))', 'if (false)')),
        ('native cached values unchecked', False, re.sub(
            r'if \(!hasFiniteAffineMapCache\((?:map|\*compound)\)\)', 'if (false)', body)),
        ('compound validation omitted', False,
         re.sub(r'if \(!dommy::numeric::isFiniteAffineTransform\(compoundMatrix\) \|\|\s*'
                r'!dommy::numeric::isFiniteAffineTransform\(compoundMatrix.inverse\(\)\)\)', 'if (false)', body)),
        ('composition reversed', False, body.replace('sourceMap->getMat4() * map.getMat4()', 'map.getMat4() * sourceMap->getMat4()')),
        ('invert ignored', False, body.replace('if (invert)', 'if (false)')),
        ('source transformed in place', False, body.replace('const GridCPtr copied = (*it)->copyGrid();', 'const GridCPtr copied = *it;')),
        ('output create failure ignored', False,
         body.replace('DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, outputVdb));', '(void)createOpenVDBOutputData(pluginData, outputVdb);')),
        ('split limit checked too late', False, body.replace('outputCount > dommy::vdb::kMaxVdbGridCount', 'false')),
        ('split boundary off by one', False, body.replace('outputCount > dommy::vdb::kMaxVdbGridCount', 'outputCount >= dommy::vdb::kMaxVdbGridCount')),
        ('retained grids omitted from count', False, body.replace('checkedAdd(outputCount, retainedCount, outputCount)', 'checkedAdd(outputCount, (void(retainedCount), 0U), outputCount)')),
        ('same-name grids deleted', False,
         body.replace('splitSources.count(source.get()) != 0U',
                      'std::any_of(splitSources.begin(), splitSources.end(), [&source](const Grid* value) {return value->getName() == source->getName();})')),
        ('unsupported grid dispatched', False, body.replace('if (getVdbGridTupleSize(getVdbGridType(**it)) != 3)', 'if (false)')),
        ('dispatch failure ignored', False, re.sub(
            r'(if \(!processVdbVectorGrid\([^{}]+\)\) \{\s*)return MS::kInvalidParameter;',
            r'\1continue;', body)),
        ('initializer failure ignored', True, init.replace('DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('initializer exception escaped', True, init.replace('return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', 1)),
    ]
    for label, initialization, source in cases:
        assert source != (init if initialization else body), label
        try:
            run(root, source, capture=True, suite='vdb_edit_init' if initialization else 'vdb_edit_compute',
                arguments=None if initialization else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or any(marker in (error.stderr or '') for marker in
                ('ERROR: AddressSanitizer', 'runtime error:')), 'Compilation failure is not detection: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--emit' in sys.argv:
        run(root, initializers(root), suite='vdb_edit_init', arguments=['--emit'])
    elif '--sdk' in sys.argv:
        sdk_check(root, sys.argv[2])
    elif '--mutations' in sys.argv:
        guards(root)
        mutations(root)
    else:
        guards(root)
        run(root, initializers(root), suite='vdb_edit_init')
        run(root, compute_bodies(root), suite='vdb_edit_compute', arguments=[])
