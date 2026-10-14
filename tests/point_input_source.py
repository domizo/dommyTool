"""Source extraction shared by the optional-input callback fixtures."""
from check_geometry_state_transactions import extract
import subprocess
from pathlib import Path
from check_curve_path_safety import run


def point_input_bodies(root, overrides=None):
    path = 'src/modules/point/DPoint/Common/DPointCommon.cpp'
    source = (overrides or {}).get(path, (root / path).read_text())
    return '\n\n'.join(extract(source, signature) for signature in (
        'MStatus getValidInputDP(', 'const DPData* getInputDP(',
        'MStatus getOptionalInputDP('))


def optional_point_sdk_body(root):
    """Extract real consumer headers/bodies with payload storage declared.

    Native Maya declarations replace the runtime host doubles. Custom payload
    storage and the Imath vector dependency remain declaration seams. This is
    neither a plug-in link nor a check of the Maya 2019 ABI or dynamics behavior.
    """
    code = ['#include "procedural_api_sdk.h"',
            '#include "modules/point/DPoint/Common/DPointDataAccess.h"',
            '#include "common/object/DParticleAttributeUtils.h"',
            '#include "common/utility/DMayaAttributeArray.h"',
            '#include "common/utility/DParticleFieldMath.h"',
            '#include "common/utility/DFluidGridPlan.h"',
            '#include <maya/MArrayDataBuilder.h>',
            '#include <maya/MFnVectorArrayData.h>',
            '#include <maya/MFnDoubleArrayData.h>',
            '#include <maya/MTransformationMatrix.h>',
            '#include <maya/MColor.h>',
            '#include <set>', point_input_bodies(root)]
    access = (root / 'src/modules/point/DPoint/Common/DPAttributeAccess.h').read_text()
    # Only the payload declaration is substituted; the consumed accessor
    # implementations and their production converters remain unchanged.
    code.append(extract(access, 'namespace dommy {'))
    code.extend(extract(access, 'inline bool getChecked' + kind + 'ArrayFromAttribute(')
                for kind in ('DScalar', 'MVector', 'MDouble', 'MInt', 'MString'))
    folder = 'src/core/DDataUtility/Particle/'
    code.append('#include "' + folder + 'DPEmitter.h"')
    emitter = (root / (folder + 'DPEmitter.cpp')).read_text()
    code.extend([extract(emitter, 'namespace {'),
                 extract(emitter, 'MStatus DPEmitter::compute('),
                 'DOMMY_COMPUTE_CATCH("DPEmitter")'])
    code.append('#include "' + folder + 'emitter/DPFluidEmitter.h"')
    fluid = (root / (folder + 'emitter/DPFluidEmitter.cpp')).read_text()
    code.extend([extract(fluid, 'struct DPFluidEmitter::EmissionValues') + ';',
                 extract(fluid, 'namespace {')])
    for name in ('fluidEmitter', 'emitFromPoints', 'emitFromGridAttributes'):
        code.append(extract(fluid, 'MStatus DPFluidEmitter::' + name + '('))
        if name == 'fluidEmitter':
            code.append('DOMMY_COMPUTE_CATCH("DPFluidEmitter")')
    for name in ('DPGoalField', 'DPFluidVectorField', 'DPFluidNoiseField'):
        code.append('#include "' + folder + 'field/' + name + '.h"')
        source = (root / (folder + 'field/' + name + '.cpp')).read_text()
        code.extend([extract(source, 'MStatus ' + name + '::compute('),
                     'DOMMY_COMPUTE_CATCH("' + name + '")'])
    return '\n\n'.join(code)


def optional_point_mutations(root, bodies, suite, callbacks, shared=False):
    cases = []
    for path, signature in callbacks:
        source = (root / path).read_text()
        callback = extract(source, signature)
        declares_status = 'MStatus status = getOptionalInputDP(' in callback
        old = ('const DPData* inputData = nullptr;\n    ' +
               ('MStatus ' if declares_status else '') +
               'status = getOptionalInputDP(thisMObject(), inDP, data, inputData);\n'
               '    if (!status) return status;')
        assert callback.count(old) == 1, signature
        restored = ('const DPData* const inputData = getInputDP(inDP, data);' +
                    ('\n    MStatus status;' if declares_status else ''))
        for label, replacement in (
                ('legacy nullable input', restored),
                ('optional input status ignored', old.replace('if (!status) return status;', '(void)status;'))):
            cases.append((signature + ': ' + label, path,
                          source.replace(callback, callback.replace(old, replacement, 1), 1)))
    if shared:
        path = 'src/modules/point/DPoint/Common/DPointCommon.cpp'
        source = (root / path).read_text()
        helper = extract(source, 'MStatus getOptionalInputDP(')
        for label, old, new in (
                ('acquisition status ignored',
                 'MDataHandle inputHandle = data.inputValue(attribute, &status);\n    if (!status) return status;',
                 'MDataHandle inputHandle = data.inputValue(attribute, &status);'),
                ('connection status ignored',
                 'const bool connected = inputPlug.isConnected(&status);\n        if (!status) return status;',
                 'const bool connected = inputPlug.isConnected(&status);'),
                ('binding status ignored',
                 'status = dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, candidate);\n    if (!status) return status;',
                 'status = dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, candidate);'),
                ('connected null accepted', 'return connected ? MS::kFailure : MS::kSuccess;',
                 '(void)connected; return MS::kSuccess;'),
                ('topology ignored', 'if (!candidate || !candidate->hasValidPointTopology()) return MS::kInvalidParameter;',
                 'if (!candidate) return MS::kInvalidParameter;')):
            assert helper.count(old) == 1, label
            cases.append(('optional reader: ' + label, path,
                          source.replace(helper, helper.replace(old, new, 1), 1)))
    for label, path, modified in cases:
        try:
            run(root, bodies(root, {path: modified}), suite=suite, arguments=[], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == suite + '_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr, label
            print('Detected optional-input mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected optional-input mutation: ' + label)
