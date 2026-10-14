#!/usr/bin/env python3
"""Execute renderer mesh metadata acquisition, not an Arnold/MtoA runtime."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract

ROOT = Path(__file__).resolve().parent.parent
SOURCE = 'src/modules/mesh/DMesh/Translator/DMeshTranslatorAi.cpp'


def bodies(overrides=None):
    source = (overrides or {}).get(SOURCE, (ROOT/SOURCE).read_text())
    code = source[source.index('namespace {'):source.index('} // namespace')+len('} // namespace')]
    start = source.index('std::vector<GroupMatteRow> matteRows;')
    end = source.index('for (const GroupMatteRow& row : matteRows)', start)
    code += '\nvoid matteAdmission(AtNode* node, const MeshOwner* dMeshes) {\n'
    code += 'const std::size_t totalFaceCount=fixtureFaces;const char* visParam="visibility";\n'
    code += source[start:end]+'mattePublished=matteRows.size();}\n'
    start = source.index('MStringArray creaseEdgeGroupArray;', end)
    end = source.index('//output arnold', start)
    code += '\nvoid creaseAdmission(AtNode* node, const MeshOwner* dMeshes) {\n'
    code += 'const MString creaseEdgeGroupNames=fixtureCreaseNames;const char* visParam="visibility";\n'
    code += source[start:end]+'creasePublished=edgeVtxIdList.size();}}\n'
    # Instantiate the actual templated native-Maya mesh path in SDK mode too.
    code += 'MStatus checkNativeCreases(const MeshOwner& m,DPIndexList& o){return collectCreaseIndices(m,"selected",o);}\n'
    return code


def run(overrides=None, capture=False, sdk=None):
    with tempfile.TemporaryDirectory(prefix='dommy-arnold-metadata-') as directory:
        temp = Path(directory)
        (temp/'metadata_methods.inc').write_text(bodies(overrides))
        flags = ['-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                 '-Wno-unused-parameter', '-Wno-unknown-pragmas',
                 '-I'+str(temp), '-I'+str(ROOT/'tests/cpp'), '-I'+str(ROOT/'src/core'),
                 '-I'+str(ROOT/'src')]
        compiler = shlex.split(os.environ.get('CXX', 'clang++'))
        if sdk:
            flags += ['-DDOMMY_SDK_CHECK=1', '-isystem', str(Path(sdk)/'include')]
            unit = temp/'sdk.cpp'
            unit.write_text('#include "arnold_mesh_metadata_host.h"\n#include "metadata_methods.inc"\n')
            subprocess.run(compiler+flags+['-fsyntax-only', str(unit)], check=True)
            subprocess.run(compiler+flags+['--analyze', '-Xanalyzer', '-analyzer-output=text',
                '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(unit)], check=True)
            print('Arnold metadata acquisition/native crease API: Maya SDK syntax/analysis passed; data/renderer boundaries declared')
            return
        flags += ['-I'+str(ROOT/'tests/stubs'), '-fsanitize=address,undefined,float-cast-overflow',
                  '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
        binary = temp/'test'
        subprocess.run(compiler+flags+[str(ROOT/'tests/cpp/arnold_mesh_metadata_tests.cpp'), '-o', str(binary)],
                       check=True, capture_output=capture, text=True)
        return subprocess.run([str(binary)], capture_output=capture, text=True, timeout=30,
            env=dict(os.environ, ASAN_OPTIONS='detect_leaks='+('0' if sys.platform == 'darwin' else '1')))


def main():
    if '--sdk' in sys.argv:
        run(sdk=sys.argv[sys.argv.index('--sdk')+1])
    elif '--mutations' in sys.argv:
        for label, old, new in (
            ('matte query status', 'DOMMY_RETURN_IF_FAILURE(status);', '(void)status;'),
            ('matte scalar status', 'DOMMY_RETURN_IF_FAILURE(readPlugValue(enable, enabled));', 'readPlugValue(enable, enabled);'),
            ('matte count admission', 'count > dommy::generation::kMaxGeneratedStringCount', 'false'),
            ('matte string length admission', 'nameValue.length() > dommy::generation::kMaxGeneratedStringByteCount ||\n            groupValue.length() > dommy::generation::kMaxGeneratedStringByteCount', 'false'),
            ('group parse length admission', 'value.length() > dommy::generation::kMaxGeneratedStringByteCount', 'false'),
            ('matte table staging', 'std::vector<GroupMatteRow> candidate;', 'std::vector<GroupMatteRow> candidate;output.clear();'),
            ('matte append status', 'DOMMY_RETURN_IF_FAILURE(names.append(MString(token.c_str())));', 'names.append(MString(token.c_str()));'),
            ('matte storage admission', '!dommy::generation::checkedProduct(faces, 24U * 4U, maximum, part)', 'false'),
            ('matte caller status', 'if (!matteStatus)', 'if (!matteStatus && false)'),
            ('crease topology storage', '!dommy::mesh::validMeshOperationStorage(geometry) ||', ''),
            ('crease mesh data status', 'DOMMY_RETURN_IF_FAILURE(status);\n            if (owner.isNull())', '(void)status;\n            if (owner.isNull())'),
            ('crease mesh creation status', 'DOMMY_RETURN_IF_FAILURE(status);\n            if (created.isNull())', '(void)status;\n            if (created.isNull())'),
            ('crease edge count status', 'DOMMY_RETURN_IF_FAILURE(status);\n            if (edges < 0', '(void)status;\n            if (edges < 0'),
            ('crease mask cardinality', 'selected.size() != static_cast<std::size_t>(edges)', 'false'),
            ('crease endpoint query', 'DOMMY_RETURN_IF_FAILURE(nativeMesh.getEdgeVertices(edge, vertices));', 'nativeMesh.getEdgeVertices(edge, vertices);'),
            ('crease endpoint bound', 'static_cast<std::size_t>(vertices[1]) >= points', 'false'),
            ('crease caller status', 'if (!creaseStatus)', 'if (!creaseStatus && false)'),
        ):
            source = (ROOT/SOURCE).read_text()
            assert old in source, 'Mutation target missing: '+label
            result = run({SOURCE: source.replace(old, new, 1)}, capture=True)
            assert result.returncode != 0, 'Undetected mutation: '+label
            print('Detected mutation: '+label, flush=True)
    else:
        result = run()
        assert result.returncode == 0, 'Renderer mesh metadata regression failed'
        print('Arnold matte/crease acquisition: status/exception faults, sparse/disabled rows, budgets, offsets, retained output and retry: OK')


if __name__ == '__main__':
    main()
