"""Complete Fur/Aho/native-curve conversion callbacks and real admission helpers."""
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from arnold_name_list_source import name_helper

NODES = {
    'DCFurGen': 'src/modules/curve/DCurve/Generate/DCFurGen.cpp',
    'DCAhoGen': 'src/modules/curve/DCurve/Generate/DCAhoGen.cpp',
    'DCToCurves': 'src/modules/curve/DCurve/Convert/DCToCurves.cpp',
}


def readers(root):
    result = []
    for path, suffix, kind in (
            ('src/modules/point/DPoint/Common/DPointCommon.cpp', 'DP', 'DPData'),
            ('src/modules/curve/DCurve/DCurveUtil.cpp', 'DC', 'DCurveData')):
        source = (root / path).read_text()
        result.extend(extract(source, signature) for signature in (
            'MStatus getValidInput' + suffix + '(',
            'const ' + kind + '* getInput' + suffix + '('))
    return result


def bodies(root, overrides=None, sdk=False):
    overrides = overrides or {}
    result = readers(root)
    types = (root / 'src/modules/point/DPoint/Common/DPAttributeCommon.cpp').read_text()
    result += [extract(types, 'bool ' + name + '(const DP_ATTR_DATA_TYPE')
               for name in ('isSingleRealAttrType', 'isVector3DAttrType')]
    utility = (root / 'src/modules/curve/DCurve/DCurveUtil.cpp').read_text()
    result.append(extract(utility, 'void GenerateDCurveKnots(').replace('bool isLoop)', 'bool isLoop = false)'))
    if not sdk:
        names = (root / 'src/core/common/utility/DMayaNameList.h').read_text()
        result.append(name_helper(names))
        output = (root / 'src/core/common/utility/DMayaPluginDataOutput.h').read_text()
        plugin = (root / 'src/core/common/utility/DMayaPluginData.h').read_text()
        result += ['namespace dommy {namespace maya {']
        for source, name in (
                (plugin, 'MStatus createPluginData('),
                (output, 'MStatus createPluginDataObject('),
                (output, 'MStatus publishPluginDataObject('),
                (output, 'MStatus publishTypedPluginDataObject(')):
            result.append('template<typename DataType>\n' + extract(source, name))
        result += [extract((root / 'src/core/common/utility/DMayaObjectArrayOutput.h').read_text(),
                           'inline MStatus setObjectArrayOutput('), '}}']
    access = (root / 'src/modules/point/DPoint/Common/DPAttributeAccess.h').read_text()
    result += ['namespace dommy {namespace point {namespace detail {',
               'template<typename ValueType>\n' + extract(access, 'inline bool copyCheckedScalarAttribute('),
               'template<typename ValueType>\n' + extract(access, 'inline bool copyCheckedVectorAttribute('), '}}}']
    result.extend(extract(access, name) for name in (
        'inline bool getCheckedDScalarArrayFromAttribute(',
        'inline bool getCheckedMVectorArrayFromAttribute('))
    for name, path in NODES.items():
        source = overrides.get(path, (root / path).read_text())
        if name != 'DCToCurves':
            result.append(extract(source, 'namespace {'))
        if sdk:
            result.append('#include "' + path[4:-4] + '.h"')
        else:
            attrs = declarations(name, source, False)
            attrs[0] = attrs[0].replace('MStatus compute(',
                'MObject thisMObject()const{return {"node"};} MStatus compute(')
            result.extend(attrs)
        result += [extract(source, 'MStatus ' + name + '::compute('),
                   'DOMMY_COMPUTE_CATCH("' + name + '")']
    return '\n\n'.join(result)


def sdk_check(root, sdk):
    from check_group_transfer_safety import sdk_check as check_boundary
    code = '#include "' + str(root / 'tests/cpp/transform_curve_compute_sdk.h') + '"\n'
    code += bodies(root, sdk=True)
    mesh = (root / 'src/modules/mesh/DMesh/common/DMeshUtil.cpp').read_text()
    code += '\n' + extract(mesh, 'MStatus getValidInputDM(')
    transform = (root / 'src/core/DDataUtility/Utility/DPTransform.cpp').read_text()
    code += '\n#include "DDataUtility/Utility/DPTransform.h"\n'
    for signature in ('void* DPTransform::creator(', 'MStatus DPTransform::process(',
                      'MStatus DPTransform::compute('):
        code += extract(transform, signature) + '\n'
    code += 'DOMMY_COMPUTE_CATCH("DPTransform")\n'
    check_boundary(root, sdk, nodes={}, label='Transform and three complete curve callbacks',
                   boundary_source=code)
