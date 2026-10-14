#!/usr/bin/env python3
"""Execute the complete root/selection bridge and the real bounded root index."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract

ROOT = Path(__file__).resolve().parent.parent
UTILITY = 'src/modules/curve/DCurve/Cmd/DDataExportUtil.cpp'
HEADER = 'src/core/common/utility/DAbcRootAdmission.h'
COMMAND = 'src/modules/curve/DCurve/Cmd/DExportAbcCmd.cpp'

def run(changes=None, capture=False, sdk=None):
    changes = changes or {}
    source = changes.get(UTILITY, (ROOT / UTILITY).read_text())
    methods = 'namespace ddata_export_util {\n' + '\n'.join(extract(source, signature) for signature in (
        'void requireExportQuery(', 'std::string exportDagPathName(', 'unsigned int exportDagDepth(',
        'bool readExportDagSelectionItem(', 'MString exportObjectName(',
        'void insertExportRoot(', 'void prepareExportSelection(',
        'void requireUniqueExportRootNames(')) + '}\n'
    shared = (ROOT / 'src/modules/curve/DCurve/Cmd/DDataExportUtil.h').read_text()
    methods = 'namespace ddata_export_util {\n' + '\n'.join(extract(shared, signature) for signature in (
        'inline MString exportNodeName(', 'inline MObject exportDagNode(')) + '}\n' + methods
    methods += extract(source, 'MString ddata_export_util::stripNamespaces(') + '\n'
    methods += extract(source, 'bool ddata_export_util::isAncestorDescendentRelationship(')
    command = changes.get(COMMAND, (ROOT / COMMAND).read_text())
    job = (ROOT / 'src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp').read_text()
    assert 'isAncestorDescendentRelationship(' not in command
    assert command.count('ddata_export_util::insertExportRoot(') == 2
    assert command.count('admitSelection(numRoots)') == 2
    assert 'admitSelection(activeList.length())' in command
    assert 'RootAdmission>(rootWork)' in command
    assert 'prepareExportSelection(\n            mSList, mArgs.dagPaths, activeList, *mArgs.exportRootAdmission)' in job
    start = command.index('unsigned int numRoots = sel.length();')
    end = command.index('\n\t\t\t\t}', command.index(
        'jobArgs.dagPaths, path, *jobArgs.exportRootAdmission);', start))
    methods += '\nMStatus collectExplicitRoots(RootJobArgs& jobArgs,const MSelectionList& sel) {\n'
    methods += 'const MString root("explicit roots");\n' + command[start:end] + '\nreturn MS::kSuccess;\n}\n'
    assert 'requireUniqueExportRootNames(\n            mArgs.dagPaths, mArgs.stripNamespace, *mArgs.exportRootAdmission)' in job
    with tempfile.TemporaryDirectory(prefix='dommy-alembic-roots-') as directory:
        temp = Path(directory)
        header = (ROOT / 'src/modules/curve/DCurve/Cmd/DDataExportUtil.h').read_text()
        (temp / 'root_declarations.inc').write_text(extract(header, 'struct cmpDag') + ';')
        (temp / 'root_methods.inc').write_text(methods)
        if HEADER in changes:
            path = temp / 'common/utility/DAbcRootAdmission.h'
            path.parent.mkdir(parents=True)
            path.write_text(changes[HEADER])
        flags = ['-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I' + str(temp),
                 '-I' + str(ROOT / 'tests/cpp'), '-I' + str(ROOT / 'src/core'),
                 '-I' + str(ROOT / 'src/core/common/utility')]
        compiler = shlex.split(os.environ.get('CXX', 'clang++'))
        if sdk:
            flags += ['-DDOMMY_SDK_CHECK=1', '-isystem', str(Path(sdk) / 'include')]
            unit = temp / 'sdk.cpp'
            unit.write_text('#include "alembic_root_host.h"\n#include "root_methods.inc"\n')
            subprocess.run(compiler + flags + ['-fsyntax-only', str(unit)], check=True)
            subprocess.run(compiler + flags + ['--analyze', '-Xanalyzer', '-analyzer-werror',
                           '-Xanalyzer', '-analyzer-output=text', '-o', os.devnull, str(unit)], check=True)
            print('Complete root/selection bridge: Maya SDK syntax/analysis passed; native host execution not linked')
            return
        flags += ['-I' + str(ROOT / 'tests/stubs'), '-fsanitize=address,undefined',
                  '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
        binary = temp / 'test'
        subprocess.run(compiler + flags + [str(ROOT / 'tests/cpp/alembic_root_tests.cpp'), '-o', str(binary)],
                       check=True, capture_output=capture, text=True)
        return subprocess.run([str(binary), str(temp / 'existing.abc')], capture_output=capture, text=True,
                              timeout=30, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                                                   ('0' if sys.platform == 'darwin' else '1')))

if __name__ == '__main__':
    if '--sdk' in sys.argv:
        run(sdk=sys.argv[sys.argv.index('--sdk') + 1])
    elif '--mutations' in sys.argv:
        for label, path, old, new in (
            ('path-name status', UTILITY, 'requireExportQuery(status, "Unable to read an Alembic root path name");', '(void)status;'),
            ('path-depth status', UTILITY, 'requireExportQuery(status, "Unable to read an Alembic root path depth");', '(void)status;'),
            ('selection expansion status', UTILITY, 'requireExportQuery(expanded.add(path, MObject::kNullObj, true),', '(void)expanded.add(path, MObject::kNullObj, true); requireExportQuery(MS::kSuccess,'),
            ('selection admission', UTILITY, 'admission.admitSelection(count);', '(void)count;'),
            ('ancestor prefix check', HEADER, 'if (descendant->compare(0U, prefix.size(), prefix) == 0)', 'if (false)'),
            ('storage allowance', HEADER, '!generation::checkedProduct(storage, 4U, 256U * 1024U * 1024U, storage)', 'false'),
            ('work admission', HEADER, 'if (!work_->consume(count))', 'if ((void)count, false)'),
            ('path depth allowance', HEADER, 'if (depth > kMaxAbcDagDepth)', 'if ((void)depth, false)'),
            ('DAG acquisition failure', UTILITY, 'requireExportQuery(status, "Unable to read a selected Alembic DAG path");', '(void)status; return false;'),
            ('classification status', UTILITY, 'requireExportQuery(selection.getDependNode(index, object),', '(void)selection.getDependNode(index, object); requireExportQuery(MS::kSuccess,'),
            ('explicit root caller', COMMAND, '!ddata_export_util::readExportDagSelectionItem(sel, j, path)', 'sel.getDagPath(j, path) != MS::kSuccess'),
            ('namespace split status', UTILITY, 'requireExportQuery(nodeName.split(\':\', parts),', '(void)nodeName.split(\':\', parts); requireExportQuery(MS::kSuccess,'),
            ('root output naming', UTILITY, 'const std::string name = exportObjectName(node, stripDepth).asChar();', 'const std::string name = pathName; (void)stripDepth;'),
            ('root namespace stripping', UTILITY, 'const std::string name = exportObjectName(node, stripDepth).asChar();', 'const std::string name = exportNodeName(node).asChar(); (void)stripDepth;'),
            ('root output duplicate', UTILITY, 'if (!inserted.second)', 'if (false)'),
            ('combined temporary storage', HEADER, '!memory::checkedAdd(storage, bytes, storage) ||\n            !generation::checkedProduct(storage, 4U, 256U * 1024U * 1024U, storage)', '!memory::checkedAdd(storage, bytes, storage) || false'),
        ):
            source = (ROOT / path).read_text()
            assert old in source, label
            result = run({path: source.replace(old, new, 1)}, capture=True)
            assert result.returncode != 0, 'Undetected mutation: ' + label
            print('Detected mutation: ' + label, flush=True)
    else:
        assert run().returncode == 0
