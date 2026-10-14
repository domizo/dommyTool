"""Complete Bend callback using the existing injectable curve boundaries."""
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

SOURCE = 'src/modules/curve/DCurve/Utility/DCBend.cpp'


def bodies(root):
    result = [extract((root / 'src/modules/curve/DCurve/DCurveUtil.cpp').read_text(),
                      'MStatus getValidInputDC('),
              extract((root / 'src/modules/point/DPoint/Common/DPAttributeCommon.cpp').read_text(),
                      'bool isVector3DAttrType(const DP_ATTR_DATA_TYPE')]
    result.append('namespace dommy {namespace maya {')
    for file, signatures in (
            ('DMayaPluginData.h', ('MStatus createPluginData(',)),
            ('DMayaPluginDataOutput.h', ('MStatus publishPluginDataObject(', 'MStatus publishTypedPluginDataObject('))):
        source = (root / 'src/core/common/utility' / file).read_text()
        result += ['template<class DataType>\n' + extract(source, signature) for signature in signatures]
    result.append('}}')
    source = (root / SOURCE).read_text()
    result += declarations('DCBend', source, False)
    result += [extract(source, 'MStatus DCBend::compute('), 'DOMMY_COMPUTE_CATCH("DCBend")']
    return '\n'.join(result)


def execute(root):
    run(root, bodies(root), suite='curve_bend_compute', arguments=[])
    import subprocess
    from pathlib import Path
    source=bodies(root)
    old='DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, input));'
    assert old in source
    try:
        run(root, source.replace(old, 'if (!getValidInputDC(inDC, data, input)) return MS::kInvalidParameter;', 1),
            suite='curve_bend_compute', arguments=[], capture=True)
    except subprocess.CalledProcessError as error:
        assert Path(error.cmd[0]).name=='curve_bend_compute_tests', 'Compile failure: Bend input status'
        print('Detected mutation: Bend input status', flush=True)
    else:
        raise AssertionError('Undetected mutation: Bend input status')
