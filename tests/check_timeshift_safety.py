#!/usr/bin/env python3
"""Run DPTimeShift control flow and historical readers with API fault injection.

Snapshots are explicit point payloads, not a Maya dependency graph or NURBS
implementation. Scalar conversion uses the real numeric helper and native-width
test array elements, not a replacement conversion. SDK checks are separate.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

NODE = 'src/modules/point/DPoint/Utility/DPTimeShift.cpp'
SHARED = 'src/core/common/object/DNodeOperation.cpp'


def bodies(root, overrides=None):
    overrides = overrides or {}
    source = overrides.get(NODE, (root / NODE).read_text())
    shared = overrides.get(SHARED, (root / SHARED).read_text())
    header = (root / 'src/modules/point/DPoint/Utility/DPTimeShift.h').read_text()
    attrs = re.findall(r'^MObject DPTimeShift::(\w+);', source, re.M)
    result = [extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                      'MStatus getValidInputDP('),
              extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                      'const DPData* getInputDP('),
              extract((root / 'src/modules/point/DPoint/Common/DPAttributeCommon.cpp').read_text(),
                      'void AddAttributeList('),
              header[header.index('enum {'):header.index('class DPTimeShift')]]
    result.append('struct DPTimeShift:AttributeNode {static MStatus initialize();'
                  'MStatus compute(const MPlug&,MDataBlock&);'
                  'MStatus evalParams(MDataBlock&,DPTimeShiftParams&);'
                  'MObject thisMObject()const{return {"node"};}'
                  'MString name()const{return "DPTimeShift";}' +
                  ''.join('static MObject '+a+';' for a in attrs) + '};')
    result.extend('MObject DPTimeShift::'+a+'{"'+a+'"};' for a in attrs)
    result += [extract(shared, 'bool validHistoricalTimes('),
               extract(shared, 'MStatus preflightHistoricalGroups(')]
    for name in ('DP', 'Curve', 'Nurbs', 'Mesh', 'Matrix', 'DPPP', 'CurvePP', 'NurbsPP', 'MeshPP'):
        result.append(extract(shared, 'MStatus DGetPlugTime'+name+'('))
    result.append(source[source.index('DScalarDataList timeRandomValues('):
                         source.index('MStatus DPTimeShift::initialize()')])
    result.append(extract(source, 'MStatus DPTimeShift::initialize()') +
                  '\nDOMMY_COMPUTE_CATCH("DPTimeShift::initialize")')
    return '\n\n'.join(result)


def check_sdk(root, sdk):
    includes = ['-I' + str(directory) for directory, _, _ in os.walk(root / 'src')]
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')] + includes + [
        '-isystem', str(root / 'third_party/libnoise/source'),
        '-idirafter', str(root / 'tests/stubs')]
    for path in (NODE, SHARED):
        subprocess.run(command + ['-fsyntax-only', str(root / path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                 '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null',
                                 str(root / path)], check=True)
    print('DPTimeShift and historical readers: complete SDK syntax/analysis passed')


def check(root, source=None, capture=False, arguments=None, header_source=None):
    if arguments is None:
        arguments = [str(root / 'tests/fixtures/timeshift_initialization_contract.txt'),
                     str(root / 'tests/fixtures/timeshift_numeric_contract.txt')]
    overrides = {NODE: source} if source is not None else None
    return run(root, bodies(root, overrides), suite='timeshift',
               arguments=arguments, capture=capture,
               header_overrides={'math/DIndexOrder.h': header_source}
               if header_source is not None else None)


def ordering_mutations(root):
    source = (root / NODE).read_text()
    header = (root / 'src/core/math/DIndexOrder.h').read_text()
    cases = (
        ('group order reversed', header,
         'const ActiveGroup current = active[index];',
         'const ActiveGroup current = active[active.size() - 1U - index];'),
        ('flat offsets forgotten', header,
         'visit(current.group, item, current.offset + item);',
         'visit(current.group, item, item);'),
        ('last item omitted', header,
         'if (item < groupLengths[current.group] - 1U)',
         'if (item + 1U < groupLengths[current.group] - 1U)'),
        ('frontier not compacted', header,
         'active[retained++] = current;', '++retained;'),
        ('point work guard removed', source,
         '!dommy::generation::checkedProduct(longestGroup, srclist.size(),\n'
         '             dommy::generation::kMaxWorkItemCount, work)',
         'false'),
        ('attribute work guard removed', source,
         '!dommy::generation::checkedProduct(longest, srclist.size(),\n'
         '                dommy::generation::kMaxWorkItemCount, work)',
         'false'),
    )
    for label, original, old, new in cases:
        assert original.count(old) == 1, label
        changed = original.replace(old, new, 1)
        try:
            check(root, changed if original is source else None, capture=True,
                  header_source=changed if original is header else None)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'timeshift_tests', 'Compile failure: ' + label
            assert ('Assertion' in error.stderr or 'runtime error:' in error.stderr or
                    'AddressSanitizer' in error.stderr), label
            print('Detected TimeShift ordering mutation:', label, flush=True)
        else:
            raise AssertionError('Undetected TimeShift ordering mutation: ' + label)


def mutations(root):
    check(root)
    source = (root / NODE).read_text()
    signature = 'MStatus getTimeValueArrayFromData('
    reader = extract(source, signature)
    conversion = '''if (!dommy::numeric::checkedScalarArrayCopy(
                dataArray, dataArray.length(), count, timeOffsetArray))
            return MS::kInvalidParameter;'''
    assert reader.count(conversion) == 3
    data = (root / 'src/core/common/data/DDataOp.h').read_text()
    old_signature = 'void DScalarMayaArrayToVectorList('
    template = data.rfind('template <', 0, data.index(old_signature))
    legacy = data[template:data.index(old_signature)] + extract(data, old_signature)
    staged_legacy = '''DScalarDataList candidate;
        DScalarMayaArrayToVectorList(dataArray, candidate);
        for (float value : candidate)
            if (!std::isfinite(value)) return MS::kInvalidParameter;
        timeOffsetArray.swap(candidate);'''
    count_guard = 'if (dataArray.length() != count) return MS::kFailure;'
    budget_guard = 'if (!dommy::sampling::validTimeStorage(count, 0U)) return MS::kInvalidParameter;'
    assert reader.count(count_guard) == reader.count(budget_guard) == 3
    variants = (
        ('post-cast finite-only admission', legacy + '\n' + reader.replace(conversion, staged_legacy)),
        ('failure clears prior output', reader.replace(conversion, 'timeOffsetArray.clear();\n        ' + conversion)),
        ('last scalar skipped', reader.replace(
            'dataArray, dataArray.length(), count, timeOffsetArray',
            'dataArray, dataArray.length() ? dataArray.length() - 1U : 0U, count ? count - 1U : 0U, timeOffsetArray')),
        ('conversion failure ignored', reader.replace(conversion,
            '(void)dommy::numeric::checkedScalarArrayCopy(dataArray, dataArray.length(), count, timeOffsetArray);')),
        ('storage preflight removed', reader.replace(budget_guard, '')),
        ('metadata cardinality ignored', reader.replace(count_guard, '').replace(
            'dataArray, dataArray.length(), count, timeOffsetArray',
            'dataArray, dataArray.length(), dataArray.length(), timeOffsetArray')),
    )
    for label, changed in variants:
        try:
            check(root, source.replace(reader, changed, 1), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'timeshift_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected mutation:', label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    for label, old, new in (
        ('offset context replaced by plug read', 'timeOffsetArray, data, inTimeOffsetData',
         'timeOffsetArray, inTimeOffsetDataPlug'),
        ('length context replaced by plug read', 'timeLengthArray, data, inTimeLengthData',
         'timeLengthArray, inTimeLengthDataPlug'),
        ('connection failure ignored', 'DOMMY_RETURN_IF_FAILURE(stat);\n        const bool objectConnected',
         'const bool objectConnected'),
    ):
        assert old in source,label
        try:
            check(root,source.replace(old,new),capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name=='timeshift_tests','Compile failure: '+label
            print('Detected mutation:',label,flush=True)
        else:
            raise AssertionError('Undetected mutation: '+label)


def attribute_mutations(root):
    source = (root / NODE).read_text()
    old = ('if (!dommy::attribute::commitStagedAttribute(\n'
           '                        dpdata, params.m_time_attr_name, candidate, dpdata.numberOfPoints()))\n'
           '                    throw std::runtime_error("Time attribute publication failed");')
    cases = [
        ('same-name typed outputs accepted',
         'params.m_time_attr_name == params.m_parent_id_attr_name', 'false'),
    ]
    old_read = 'DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, inputDPData));'
    cases.append(('mandatory input status erased', old_read,
                  'if (!getValidInputDP(inDP, data, inputDPData)) return MS::kFailure;'))
    for name in ('m_time_offset_name', 'm_time_length_name'):
        old_read = ('if (!dommy::attribute::convertScalarAttribute(\n'
                    '                            inputDPData->getAttr(params.' + name +
                    '), count, ' + ('timeOffsetArray' if name == 'm_time_offset_name' else 'timeLengthArray') +
                    '))\n                        return MS::kInvalidParameter;')
        cases.append((name + ' silently keeps defaults', old_read, '(void)0;'))
    for field, message in (('m_time_attr_name', 'Time'), ('m_parent_id_attr_name', 'Parent ID')):
        block = old.replace('m_time_attr_name', field).replace('Time attribute', message + ' attribute')
        assert source.count(block) == 2, field
        for index in range(2):
            positions = [match.start() for match in re.finditer(re.escape(block), source)]
            position = positions[index]
            changed = source[:position] + ('(void)dommy::attribute::commitStagedAttribute('
                'dpdata, params.' + field + ', candidate, dpdata.numberOfPoints());') + source[position+len(block):]
            cases.append((message + ' write result ignored (' + str(index) + ')', source, changed))
    for label, original, changed in cases:
        assert original in source, label
        try:
            check(root, source.replace(original, changed, 1), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'timeshift_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected TimeShift attribute mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected TimeShift mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        check_sdk(root, sys.argv[2])
    elif sys.argv[1:] == ['--mutations']:
        mutations(root)
    else:
        check(root, arguments=sys.argv[1:] or None)
        if not sys.argv[1:]:
            attribute_mutations(root)
            ordering_mutations(root)
