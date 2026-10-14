#!/usr/bin/env python3
"""Execute actual string implementation and bounded metadata formatter."""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_path_reservation_exception_safety import disable_core_dumps

ROOT = Path(__file__).resolve().parent.parent
STRING = ROOT / 'src/core/common/string/DStringOp.cpp'
REPORT = ROOT / 'src/commands/Cmd/Metadata/DMetadataSelectCmd.cpp'
HELPER = ROOT / 'src/core/common/string/DStreamFormat.h'
SOURCE = ROOT / 'tests/cpp/diagnostic_text_tests.cpp'
ATTRIBUTE_INFO = ROOT / 'src/modules/point/DPoint/Common/DPAttributeCommon.cpp'
GROUP_INFO = ROOT / 'src/core/Data/DGroupCommon.cpp'
ATTRIBUTE_VALUE = ROOT / 'src/core/DDataUtility/Data/DPAttributeData.h'
VDB_INFO = ROOT / 'src/modules/volume/VDB/Display/DOpenVDBDisplayVP2.cpp'
MESH_INFO = ROOT / 'src/modules/volume/VDB/Generate/DOpenVDBFromMesh.cpp'


def run(strings=None, report=None, helper=None, mutation=False, analyze=False,
        sdk=None, section='all', descriptions=None):
    with tempfile.TemporaryDirectory(prefix='dommy-diagnostic-text-') as directory:
        temp = Path(directory)
        string_path = STRING
        if strings is not None:
            string_path = temp / 'DStringOp.cpp'
            string_path.write_text(strings)
        if helper is not None:
            shadow = temp / 'common/string/DStreamFormat.h'
            shadow.parent.mkdir(parents=True)
            shadow.write_text(helper)
        report_source = REPORT.read_text() if report is None else report
        report_class = extract(report_source, 'class BoundedMetadataReport') + ';'
        include_helper = '#include "common/string/DStreamFormat.h"\n' if HELPER.exists() else ''
        (temp / 'diagnostic_text_methods.inc').write_text(
            '#include "' + str(string_path) + '"\n' + include_helper + report_class)
        changes = descriptions or {}
        if ATTRIBUTE_VALUE in changes:
            shadow = temp / 'DDataUtility/Data/DPAttributeData.h'
            shadow.parent.mkdir(parents=True)
            shadow.write_text(changes[ATTRIBUTE_VALUE])
        (temp / 'description_methods.inc').write_text('\n'.join(
            extract(changes.get(path, path.read_text()), signature) for path, signature in (
                (ATTRIBUTE_INFO, 'DStringList GetAllAttributeListInfo('),
                (GROUP_INFO, 'void GetAllGroupListInfo('))))
        mesh = changes.get(MESH_INFO, MESH_INFO.read_text())
        fragments = [extract(changes.get(VDB_INFO, VDB_INFO.read_text()), 'MString buildVdbInfo(')]
        start = mesh.index('std::stringstream text;')
        end = mesh.index('auto output = data.outputValue(', start)
        fragments.append('MString meshResolutionText(const int (&resolution)[3]) {\n' +
                         mesh[start:end] + 'return resolutionText;}')
        start = mesh.index('std::stringstream info;')
        end = mesh.index('MStatus status;', start)
        fragments.append('MString meshNodeInfoText(const Count& points, const Count& primitives, '
                         'const DOpenVDBData* output, const Timer& timer) {\n' +
                         mesh[start:end] + 'return infoText;}')
        (temp / 'vdb_info_methods.inc').write_text('\n'.join(fragments))
        flags = ['-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                 '-I' + str(temp), '-I' + str(ROOT / 'src/core'),
                 '-I' + str(STRING.parent)]
        flags += ['-I' + str(ROOT / 'tests/cpp')]
        flags += ['-I' + str(ROOT / 'src/core/Utility/3d/Math')]
        if sdk:
            flags += ['-DDOMMY_DIAGNOSTIC_SDK_CHECK=1', '-isystem', str(sdk / 'include'),
                      '-I' + str(ROOT / 'src/modules/point/DPoint'),
                      '-idirafter', str(ROOT / 'tests/stubs')]
        else:
            flags += ['-I' + str(ROOT / 'tests/stubs')]
        command = shlex.split(os.environ.get('CXX', 'clang++')) + flags
        if sdk:
            subprocess.run(command + ['-fsyntax-only', str(SOURCE)], check=True)
        if analyze or sdk:
            subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(SOURCE)], check=True)
            print('Diagnostic text: Clang analysis' + (' and Maya SDK syntax' if sdk else '') + ': OK')
            return
        binary = temp / 'diagnostic_text_tests'
        command += ['-DDOMMY_DIAGNOSTIC_ALLOCATION_TESTS=1', '-fno-omit-frame-pointer',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all']
        subprocess.run(command + [str(SOURCE), '-o', str(binary)], check=True)
        options = {'preexec_fn': disable_core_dumps} if os.name == 'posix' else {}
        result = subprocess.run([str(binary), section], text=True, capture_output=True,
            cwd=str(temp), env=dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
            ('0' if sys.platform == 'darwin' else '1')), timeout=20, **options)
        if mutation:
            if result.returncode == 0 or 'Assertion' not in result.stderr:
                raise AssertionError('Mutation must fail at runtime:\n' + result.stderr)
        elif result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        else:
            print(result.stdout.strip(), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-only', action='store_true')
    parser.add_argument('--analyze', action='store_true')
    parser.add_argument('--sdk', type=Path)
    parser.add_argument('--section', choices=('all', 'report', 'merge', 'descriptions', 'vdb'), default='all')
    args = parser.parse_args()
    run(analyze=args.analyze, sdk=args.sdk, section=args.section)
    if args.baseline_only or args.analyze or args.sdk:
        return
    source = STRING.read_text()
    before = 'return dommy::text::checkedStreamText(stream, "String list formatting failed");'
    assert source.count(before) == 1
    run(strings=source.replace(before, 'return stream.str();'), mutation=True, section='merge')
    print('Detected mutation: string list accepts partial stream', flush=True)
    source = REPORT.read_text()
    before = 'return append(dommy::text::checkedStreamText(line, "Metadata formatting failed"));'
    assert source.count(before) == 1
    run(report=source.replace(before, 'return append(line.str());'), mutation=True, section='report')
    print('Detected mutation: metadata accepts partial stream', flush=True)
    before = 'const int inserted[] = {0, ((void)(line << rest), 0)...};'
    assert source.count(before) == 1
    run(report=source.replace(before, '(void)sizeof...(rest); const int inserted[] = {0};'),
        mutation=True, section='report')
    print('Detected mutation: metadata drops multi-field member content', flush=True)
    source = HELPER.read_text()
    before = 'if (!stream) throw std::runtime_error(context);'
    assert source.count(before) == 1
    run(helper=source.replace(before, '(void)context;'), mutation=True)
    print('Detected mutation: shared stream state check omitted', flush=True)
    for label, path, before, after in (
        ('attribute descriptions accept partial stream', ATTRIBUTE_INFO,
         'dommy::text::checkedStreamText(infoStr, "Attribute description formatting failed")', 'infoStr.str()'),
        ('group descriptions accept partial stream', GROUP_INFO,
         'dommy::text::checkedStreamText(infoStr, "Group description formatting failed")', 'infoStr.str()'),
        ('group description list loses transaction', GROUP_INFO,
         'DStringList candidate(groupInfos);', 'DStringList& candidate = groupInfos;'),
        ('typed attribute description accepts partial stream', ATTRIBUTE_VALUE,
         'dommy::text::checkedStreamText(ostr, "Attribute value description formatting failed")', 'ostr.str()'),
    ):
        source = path.read_text()
        assert source.count(before) == 1, label
        run(descriptions={path: source.replace(before, after)}, mutation=True, section='descriptions')
        print('Detected mutation: ' + label, flush=True)
    for label, path, before, after, count in (
        ('VDB display accepts partial info', VDB_INFO,
         'dommy::text::checkedStreamText(info, "VDB info formatting failed")', 'info.str()', 2),
        ('VDB resolution accepts partial info', MESH_INFO,
         'dommy::text::checkedStreamText(text, "VDB resolution formatting failed")', 'text.str()', 1),
        ('mesh-to-VDB accepts partial info', MESH_INFO,
         'dommy::text::checkedStreamText(info, "Mesh-to-VDB info formatting failed")', 'info.str()', 1),
    ):
        source = path.read_text()
        assert source.count(before) == count, label
        run(descriptions={path: source.replace(before, after)}, mutation=True, section='vdb')
        print('Detected mutation: ' + label, flush=True)
    mesh = MESH_INFO.read_text()
    assert mesh.index('const MString resolutionText(') < mesh.index('auto output = data.outputValue(')
    assert mesh.index('const MString infoText(') < mesh.index('auto outputHandle = data.outputValue(')
    assert mesh.index('const MString infoText(') < mesh.index('publishTypedPluginDataObject<DOpenVDBData>(')
    # The complete Maya command is separately syntax/analysis checked. Do not
    # let its multi-field member line bypass the formatter tested above.
    assert 'report.appendLine("structure name:", member->name(), \' \', value);' in REPORT.read_text()
    assert 'report.append(line.str())' not in REPORT.read_text()


if __name__ == '__main__':
    main()
