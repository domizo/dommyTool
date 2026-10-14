"""Complete Blend callback; real readers, conversions, math and publication.

Native arrays and DG/payload ownership are explicit injectable boundaries.
The SDK check uses native Maya declarations, not a host execution/link.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

BLEND = 'src/core/DDataUtility/Attribute/DPAttributeBlend.cpp'
ARRAY = 'common/utility/DAttributeArrayConversion.h'


def bodies(root, overrides=None, sdk=False):
    overrides = overrides or {}
    def read(path):
        return overrides.get(path, (root / path).read_text())
    common = read('src/modules/point/DPoint/Common/DPointCommon.cpp')
    templates = read('src/modules/point/DPoint/Common/DPointCommon.h')
    attrs = read('src/modules/point/DPoint/Common/DPAttributeCommon.cpp')
    blend = read(BLEND)
    result = [extract(common, 'MStatus getValidInputDP(')]
    for name in ('isSingleRealAttrType', 'isVector2DAttrType', 'isVector3DAttrType', 'isMatrixAttrType'):
        result.append(extract(attrs, 'bool ' + name + '(const DP_ATTR_DATA_TYPE'))
    result += ['template<typename T>\n' + extract(templates, 'inline void getDPSingleValueToDouble('),
               'void getDDoubleArrayFromAttribute(const DPData&,const std::string&,DDoubleArray&);',
               extract(attrs, 'void getMDoubleArrayFromAttribute('),
               extract(attrs, 'void getDDoubleArrayFromAttribute(')]
    if not sdk:
        publication = read('src/core/common/utility/DMayaPluginDataOutput.h')
        result.append('namespace dommy {namespace maya {')
        for name in ('createPluginDataObject', 'publishPluginDataObject'):
            result.append('template<typename DataType>\n' + extract(publication, 'MStatus ' + name + '('))
        result.append('}}')
    result.append(extract(blend, 'namespace {'))
    result += declarations('DPAttributeBlend', blend, False)
    result += [extract(blend, 'MStatus DPAttributeBlend::compute('),
               'DOMMY_COMPUTE_CATCH("DPAttributeBlend")']
    code = '\n\n'.join(result)
    return re.sub(r'\{"[^\"]+"\}', '{}', code) if sdk else code


def execute(root, source=None, capture=False):
    source = source or bodies(root)
    flags = ['-I' + str(root / 'src/core/common/utility')]
    run(root, source, suite='attribute_blend_compute', arguments=[], capture=capture, extra_flags=flags)
    header = (root / 'src/core' / ARRAY).read_text()
    reduced = header.replace('maximum = 256U * 1024U * 1024U', 'maximum = 2048U')
    assert reduced != header
    run(root, source, suite='attribute_blend_compute', arguments=['budget'], capture=capture,
        extra_flags=flags, header_overrides={ARRAY: reduced,
            'common/utility/DMayaAttributeArray.h':
                (root / 'src/core/common/utility/DMayaAttributeArray.h').read_text()})


def mutations(root):
    source = bodies(root)
    for label, old, new in (
        ('Blend required input status', 'DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, input));',
         'if (!getValidInputDP(inDP, data, input)) return MS::kInvalidParameter;'),
        ('Blend weight type', '!isSingleRealAttrType(weightAttribute->getAttrType())', 'false'),
        ('Blend copied metadata budget',
         'dommy::point::preflightPointPayload(*input, sizeof(DPoint) + 2U * sizeof(double), workspace);', ''),
        ('Blend generated field budget',
         'dommy::point::preflightGeneratedPointAttribute(\n        pointCount, outputName, dommy::point::pointAttributeStorageBytes(type), workspace);', ''),
        ('Blend weight cardinality', 'if (weights.length() != pointCount) return MS::kFailure;', ''),
        ('Blend publication failure',
         'DOMMY_RETURN_IF_FAILURE(\n        dommy::maya::publishPluginDataObject(outputHandle, outputObject, *output));',
         '(void)dommy::maya::publishPluginDataObject(outputHandle, outputObject, *output);'),
    ):
        assert old in source, label
        try:
            execute(root, source.replace(old, new, 1), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'attribute_blend_compute_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def sdk_boundary(root, sdk):
    code = '#include "attribute_blend_compute_sdk.h"\n' + bodies(root, sdk=True)
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-variable', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src/core'),
        '-I' + str(root / 'src'), '-I' + str(root / 'tests/cpp'),
        '-I' + str(root / 'src/core/common/utility'), '-idirafter', str(root / 'tests/stubs'),
        '-x', 'c++', '-']
    for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                    '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
        subprocess.run(command + flags, input=code, text=True, check=True)
    print('Complete Blend callback/readers/conversions/math/publication passed native Maya header syntax/analysis; custom payload declarations and Imath test seam')
