"""Complete motion compute with explicit host/history seams and real attributes."""
import re

from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations

SOURCE = 'src/modules/mesh/DMesh/Deform/DMMotionDeform.cpp'


def computes(root, overrides=None):
    overrides = overrides or {}
    def read(path):
        return overrides.get(path, (root / path).read_text())
    source = read(SOURCE)
    utility = read('src/modules/mesh/DMesh/common/DMeshUtil.h')
    # Legacy helpers remain executable for the before-fix and mutation probes.
    result = [extract(read('src/modules/point/DPoint/Common/DPAttributeCommon.cpp'),
                      'void AddAttributeList('),
              extract(read('src/modules/point/DPoint/Common/DPAttributeCommon.cpp'),
                      'bool isSingleRealAttrType(const DP_ATTR_DATA_TYPE'),
              'namespace dommy { namespace attribute {' +
              extract(read('src/core/common/utility/DMayaAttributeArray.h'),
                      'template <typename Source, typename Value>\nvoid requireTypedArray(') + '}}',
              'template <typename T>\n' + extract(utility,
                  'inline void getDMPSingleValueToFloat(const DMItem &mesh, const std::string &name, DScalarDataList'),
              extract(utility, 'inline void getDMPAttributeFromScalar('),
              extract(read('src/modules/mesh/DMesh/common/DMeshUtil.cpp'),
                      'MStatus getValidInputDM('),
              extract(read('src/modules/mesh/DMesh/common/DMeshUtil.cpp'),
                      'const DMeshData* getInputDM(')]
    math = read('src/core/math/dommyMath.cpp')
    result.append('namespace dommy { namespace math {' +
        re.search(r'^const double kRadiansToDegrees = [^;]+;', math, re.M).group(0) + '\n' +
        extract(math, 'double radiansToDegrees(') + '}}')
    values = read('src/core/common/utility/DMayaValueValidation.h')
    result.append('namespace dommy { namespace maya {')
    for name in ('isFinitePoint', 'isFiniteVector', 'isFloatRepresentablePoint',
                 'isFloatRepresentableVector', 'normalizedVector'):
        result.append(extract(values, 'inline bool ' + name + '('))
    publication = read('src/core/common/utility/DMayaPluginDataOutput.h')
    for name in ('publishPluginDataObject', 'publishTypedPluginDataObject'):
        result.append(extract(publication,
            'template <typename DataType>\nMStatus ' + name + '('))
    result.append('}}')
    declared = declarations('DMMotionDeform', source, False)
    declared[0] = declared[0].replace('MStatus compute(',
        'MObject thisMObject()const{return {"node"};}'
        'MStatus getDMeshFromTime(std::vector<std::vector<DPointList>>& out,'
        'const std::vector<std::size_t>& counts,std::size_t total,long count,'
        'double range,const MTime& time) {'
        'return sampleHistory(out,counts,total,count,range,time);}'
        'MStatus compute(')
    result.extend(declared)
    result.append(extract(source, 'MStatus DMMotionDeform::compute(') +
                  '\nDOMMY_COMPUTE_CATCH("DMMotionDeform")')
    return '\n\n'.join(result)
