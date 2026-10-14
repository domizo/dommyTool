#!/usr/bin/env python3
"""Compile the production Alembic attribute wrappers without the Alembic SDK."""
from pathlib import Path
import subprocess
import sys
import re
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

root = Path(__file__).resolve().parent.parent
source = (root / 'src/modules/curve/DCurve/Cmd/DDataExportUtil.h').read_text()
bodies = '\n'.join('template <typename T, typename U>\n' + extract(source, signature)
    for signature in ('inline void DGetAbcDPScalarAttribute(',
                      'inline void DGetAbcDCPScalarAttribute(',
                      'inline void DGetAbcDMPScalarAttribute(',
                      'inline void DGetAbcDMPFaceAttribute('))
primitive=extract((root/'src/modules/curve/DCurve/Cmd/Abc/DMeshWriter.cpp').read_text(),
                  'void DMeshWriter::getDMeshPrimAttribute(')
assert len(re.findall(r'DGetAbcDMPFaceAttribute<DA_\w+>',primitive))==12
assert 'attrListValue<' not in primitive and 'kUniformScope' in primitive
if '--mutations' in sys.argv:
    helper = extract((root/'src/core/common/utility/DAttributeExport.h').read_text(),
                     'namespace dommy {')
    for label, old, new in (
        ('missing partition compacted', 'offset += count;', 'offset += attribute ? count : 0U;'),
        ('declared default discarded', 'candidate(total, defaultValue)', 'candidate(total, Destination())'),
        ('partial export values published', 'std::vector<Destination> candidate(total, defaultValue);',
         'output.assign(total, defaultValue); std::vector<Destination> candidate(total, defaultValue);'),
        ('checked attribute conversion omitted',
         '!convertTypedArray<Source, Destination>(attribute, count, converted)', 'false'),
        ('primitive values not expanded per face',
         'candidate.insert(candidate.end(), count, primitives[index]);',
         'candidate.insert(candidate.end(), 1U, primitives[index]);'),
        ('primitive face budget omitted','generation::kMaxGeneratedFaceCount, faces',
         'std::numeric_limits<std::size_t>::max(), faces'),
        ('primitive face result lost','output.swap(candidate);','output.clear();'),
    ):
        assert old in helper, label
        try:
            run(root, helper.replace(old,new)+'\n'+bodies, suite='alembic_attribute', arguments=[],
                capture=True,extra_flags=('-DDOMMY_TEST_EXPORT_MUTATION=1',))
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or 'runtime error:' in (error.stderr or ''), 'Compile failure is not detection: '+label
            print('Detected mutation: '+label)
        else:
            raise AssertionError('Undetected mutation: '+label)
else:
    run(root, bodies, suite='alembic_attribute', arguments=[])
