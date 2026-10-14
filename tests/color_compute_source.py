"""Complete color compute extraction, independent of native shading/SeExpr."""
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
import re

COLORS = {
    'DPColor': 'src/modules/point/DPoint/Utility/DPColor.cpp',
    'DCColor': 'src/modules/curve/DCurve/Utility/DCColor.cpp',
    'DMColor': 'src/modules/mesh/DMesh/Utility/DMColor.cpp',
}


def bodies(root, overrides=None, sdk=False):
    overrides = overrides or {}
    result = []
    if not sdk:
        aliases = (root / 'src/modules/point/DPoint/DType.h').read_text()
        # Redeclarations couple the fixture's host-free aliases to production;
        # a storage-type drift cannot silently pass only the fixture's types.
        for name in ('DColor', 'DColorList', 'DVec3Data', 'DVec3DataList'):
            result.append(re.search(r'^typedef[^\n]+\b' + name + r';$', aliases, re.M).group(0))
    for path, names in (
            ('src/modules/point/DPoint/Common/DPointCommon.cpp',
             ('MStatus getValidInputDP(', 'const DPData* getInputDP(')),
            ('src/modules/curve/DCurve/DCurveUtil.cpp',
             ('MStatus getValidInputDC(', 'const DCurveData* getInputDC(')),
            ('src/modules/mesh/DMesh/common/DMeshUtil.cpp',
             ('MStatus getValidInputDM(', 'const DMeshData* getInputDM('))):
        source = (root / path).read_text()
        result.extend(extract(source, name) for name in names)
    for header, signature in (
            ('src/modules/point/DPoint/Common/DPointCommon.h',
             'inline void getDPSingleValueToFloat(const DPData&'),
            ('src/modules/point/DPoint/Common/DPointCommon.h',
             'inline void getDPTwoArray(const DPData&'),
            ('src/modules/curve/DCurve/DCurveUtil.h', 'inline void getDCPTwoArray('),
            ('src/modules/mesh/DMesh/common/DMeshUtil.h', 'inline void getDMPSingleValueToFloat('),
            ('src/modules/mesh/DMesh/common/DMeshUtil.h', 'inline void getDMPTwoArray(')):
        result.append('template <typename T>\n' + extract((root / header).read_text(), signature))
    curve = (root / 'src/modules/curve/DCurve/DCurveUtil.h').read_text()
    result.append(extract(curve, 'inline void getDCPAttributeMDoubleArray('))
    types = (root / 'src/modules/point/DPoint/Common/DPAttributeCommon.cpp').read_text()
    for name in ('isSingleValueAttrType', 'isVector2DAttrType', 'isVector3DAttrType'):
        result.append(extract(types, 'bool ' + name + '(const DP_ATTR_DATA_TYPE'))
    noise_header = (root / 'src/core/common/object/DNoiseOp.h').read_text()
    result.append('template <typename T>\n' + extract(noise_header, 'void DGenerateRandomValue('))
    noise = (root / 'src/core/common/object/DNoiseOp.cpp').read_text()
    for kind in ('float3', 'double3', 'MVector'):
        start = noise.index('void DGenerateRandomValue3(int seed, const ' + kind)
        if kind in ('float3', 'double3'):
            # The first overload is the Maya vector destination, not the
            # color storage destination consumed by these nodes.
            start = noise.index('void DGenerateRandomValue3(int seed, const ' + kind, start + 1)
        result.append(extract(noise[start:], 'void DGenerateRandomValue3('))
    if not sdk:
        palette = (root / 'src/core/common/utility/DColorPalette.h').read_text()
        result.append(extract(palette, 'namespace dommy {'))
        ramp = (root / 'src/core/common/utility/DMayaRampSampling.h').read_text()
        result.extend(['namespace dommy {namespace maya {',
                       extract(ramp, 'inline bool sampleFiniteColorRamp('), '}}'])
        output = (root / 'src/core/common/utility/DMayaPluginDataOutput.h').read_text()
        result.extend(['namespace dommy {namespace maya {',
                       'template <typename DataType>\n' + extract(output, 'MStatus publishPluginDataObject('),
                       'template <typename DataType>\n' + extract(output, 'MStatus publishTypedPluginDataObject('), '}}'])
    for name, path in COLORS.items():
        source = overrides.get(path, (root / path).read_text())
        if sdk:
            result.append('#include "' + path[4:-4] + '.h"')
        else:
            attrs = declarations(name, source, False)
            attrs[0] = attrs[0].replace('MStatus compute(',
                'MObject thisMObject()const{return {"node"};}'
                'MString name()const{return "' + name + '";} MStatus compute(')
            result.extend(attrs)
        if name == 'DMColor':
            result.append(extract(source, 'namespace {'))
        result.append(extract(source, 'MStatus ' + name + '::compute('))
        result.append('DOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(result)
