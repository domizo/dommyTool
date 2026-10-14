#!/usr/bin/env python3
"""Nucleus particle entry-point contracts with injectable host boundaries.

The production compute/initializers run; native solver behavior and ownership
remain Maya integration tests, not behavior implemented by these doubles.
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

FOLDER = 'src/core/DDataUtility/Particle/nobject/'


def initializers(root):
    result = []
    for name in ('DPnParticle', 'DPnPopSolver'):
        source = (root / (FOLDER + name + '.cpp')).read_text()
        result += declarations(name, source, True)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n'.join(result)


def compute(root, include_setup=False):
    source = (root / (FOLDER + 'DPnParticle.cpp')).read_text()
    header = (root / (FOLDER + 'DPnParticle.h')).read_text()
    attrs = re.findall(r'^MObject DPnParticle::(\w+);', source, re.M)
    validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    result = ['namespace dommy { namespace maya {',
              extract(validation, 'inline bool isFinitePoint('),
              extract((root / 'src/core/common/utility/DMayaPluginData.h').read_text(),
                      'template <typename DataType>\nMStatus getPluginDataFromHandle('), '}}']
    result.append(extract((root / 'src/modules/point/DPoint/Common/DPAttributeCommon.cpp').read_text(),
                          'bool isVector3DAttrType(const DP_ATTR_DATA_TYPE &type)'))
    result.append(extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                          'MStatus getValidInputDP('))
    if include_setup:
        controls = (root / 'src/core/common/object/DNucleusParticleControls.h').read_text()
        result.append(extract(controls, 'namespace dommy {'))
    result += [
              'struct DPnParticle:ParticleNodeBoundary {',
              'MStatus compute(const MPlug&,MDataBlock&);']
    result += [re.search(r'enum class PendingParticleSetup[^;]+;', header).group(0),
               re.search(r'PendingParticleSetup m_pendingParticleSetup[^;]+;', header).group(0)]
    if include_setup:
        result += ['MStatus initializeDPDataParticle(DPData&,MDataBlock&);',
                   'MStatus updateDPDataParticle(DPData&,MDataBlock&);',
                   'MStatus configureDPDataParticle(DPData&,MDataBlock&,bool);',
                   'MStatus initializeDPSolver(MDataBlock&);']
    result += ['static MObject ' + a + ';' for a in attrs]
    result += ['};']
    result += ['MObject DPnParticle::' + a + '{"' + a + '"};' for a in attrs]
    result.append(extract(source, 'MStatus DPnParticle::compute('))
    if include_setup:
        result += ['static const bool DP_NPARTICLE_DEBUG=false, DP_NPOP_SOLVER_DEBUG=false;',
                   extract(source, 'namespace {')]
        for method in ('initializeDPDataParticle', 'updateDPDataParticle',
                       'configureDPDataParticle', 'initializeDPSolver'):
            result.append(extract(source, 'MStatus DPnParticle::' + method + '('))
        pop = (root / (FOLDER + 'DPnPopSolver.cpp')).read_text()
        pop_attrs = re.findall(r'^MObject DPnPopSolver::(\w+);', pop, re.M)
        result += ['struct DPnPopSolver:ParticleNodeBoundary {'
                   'MStatus initSolver(MDataBlock&);'
                   'MStatus initDPParticle(const DPData&,MDataBlock&);'
                   'MStatus updateCommonDPParticle(MDataBlock&);' +
                   ''.join('static MObject ' + a + ';' for a in pop_attrs) + '};']
        result += ['MObject DPnPopSolver::' + a + '{"' + a + '"};' for a in pop_attrs]
        result += [extract(pop, 'MStatus DPnPopSolver::' + method + '(')
                   for method in ('initSolver', 'initDPParticle', 'updateCommonDPParticle')]
    return '\n'.join(result)


def sdk_check(root, sdk):
    # Compile the changed entry points against actual Maya declarations. Other
    # project/SeExpr/solver functions are declarations, not behavioral stubs.
    prefix = '''
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MFnMessageAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MPxNode.h>
#include <maya/MDataBlock.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnPluginData.h>
#include <maya/MnParticle.h>
#include <maya/MnSolver.h>
#include <maya/MnRigid.h>
#include <maya/MFnField.h>
#include <maya/MItDag.h>
#include <maya/MPointArray.h>
#include <maya/MVectorArray.h>
#include <maya/MFloatArray.h>
#include <maya/MDoubleArray.h>
#include <maya/MPlug.h>
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/object/DNucleusParticleControls.h"
#include "common/object/DNucleusCollisionUtils.h"
#include "common/object/DNucleusSolverControls.h"
#include "common/utility/DParticleFieldMath.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaAttributeArray.h"
#include <string>
#include <array>
#define DOMMY_MAYA_DEBUG(a,b) ((void)0)
using DScalarDataList=std::vector<float>;
using DVec3Data=Imath::V3f;
using DVec3DataList=std::vector<DVec3Data>;
struct DPData:MPxData {
    static MTypeId id;
    void setDPData(const DPData&);
    std::size_t numberOfPoints() const;
    bool mergeDPData(const DPData&);
    bool hasValidPointTopology() const;
    bool isExistAttr(const std::string&) const;
    DPAttributeData::ConstPtr getAttr(const std::string&) const;
    void setPosition(const MFloatPointArray&);
    void setDPAttributeValues(const std::string&, const MFloatPointArray&);
    void updateBBox();
    const std::vector<std::array<float,3>>* getPositionPtrConst() const;
    void addAttrWithValues(const std::string&,const DVec3DataList&);
    void addAttr(const std::string&,const DPFloatAttr&);
};
struct DMeshData {static MTypeId id;};
using AttributeNode = MPxNode;
const DPData* getInputDP(const MObject&, MDataBlock&);
bool isVector3DAttrType(const DP_ATTR_DATA_TYPE&);
void checkValidString(std::string&, const char*);
void EchoWarning(const char*, const std::string&);
struct SeVec3d {double operator[](int) const;};
struct DPointSeExpr {
    explicit DPointSeExpr(const std::string&);
    bool usesVar(const char*) const;
    bool usesPointDataFunctions() const;
    bool isValid() const;
    std::string parseError() const;
};
namespace dommy {
namespace maya {
MStatus createPluginData(MFnPluginData&,const MTypeId&,DPData*&);
MStatus publishPluginDataObject(MDataHandle&,const MObject&,const DPData&);
MStatus setExpressionTime(DPointSeExpr&,const MTime&);
MStatus getEvaluationStepSeconds(MDataBlock&,double&);
}
namespace expression {bool evaluateChecked(DPointSeExpr&,SeVec3d&);}
}
struct ParticleNodeBoundary:MPxNode {
    MnParticle fNObject;
    MnSolver solver;
    std::vector<MnRigid*> colObjList;
    MStatus ensureNParticle();
    MStatus initializeDPDataParticle(DPData&, MDataBlock&);
    MStatus initializeDPSolver(MDataBlock&);
    MStatus updateDPDataParticle(DPData&, MDataBlock&);
    MStatus syncCollisions(MDataBlock&);
    MStatus calculateDPSolver(MDataBlock&,float);
};
MStatus checkCollisionBoundary(MDataBlock& data,const MObject& attribute,
                               MnSolver& solver,std::vector<MnRigid*>& registered) {
    return dommy::maya::syncNucleusRigidCollisions(data,attribute,solver,registered);
}
'''
    init = initializers(root)
    # The real static MObject definitions must not use the test-only string IDs.
    init = re.sub(r'MObject (\w+::\w+)\{"\w+"\};', r'MObject \1;', init)
    runtime = re.sub(r'MObject (\w+::\w+)\{"\w+"\};', r'MObject \1;', compute(root, True))
    runtime = runtime[runtime.index('struct DPnParticle:'):]
    # Isolate initializer and compute classes to avoid duplicate DPnParticle.
    simulation_source = re.sub(r'MObject (\w+::\w+)\{"\w+"\};', r'MObject \1;', simulation(root, False))
    pop_source = re.sub(r'MObject (\w+::\w+)\{"\w+"\};', r'MObject \1;', pop_compute(root, False))
    required = extract((root / 'src/modules/point/DPoint/Common/DPointCommon.cpp').read_text(),
                       'MStatus getValidInputDP(')
    source = (prefix + '\n' + required + '\nnamespace Initializers {\n' + init + '\n}\n' + runtime +
              '\nnamespace Simulation {\n' + simulation_source + '\n}\n' +
              '\nnamespace PopCompute {\n' + pop_source + '\n}\n')
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src/core'),
        # Only non-SDK DType/Imath fallbacks; actual Maya headers take priority.
        '-idirafter', str(root / 'tests/stubs'),
        '-x', 'c++', '-']
    subprocess.run(command + ['-fsyntax-only'], input=source, text=True, check=True)
    subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                              '-Xanalyzer', '-analyzer-werror', '-o', os.devnull],
                   input=source, text=True, check=True)
    print('Nucleus particle entry points: Maya SDK syntax/analysis passed (native solver not executed)')


def baseline(root):
    run(root, initializers(root), suite='nucleus_particle_init')
    run(root, compute(root), suite='nucleus_particle_compute', arguments=[])
    run(root, compute(root, True), suite='nucleus_particle_setup', arguments=[])
    run(root, compute(root, True), suite='nucleus_pop_setup', arguments=[])
    run(root, collisions(root), suite='nucleus_collision', arguments=[])
    run(root, simulation(root), suite='nucleus_simulation', arguments=[])
    run(root, pop_compute(root), suite='nucleus_pop_compute', arguments=[])


def required_input_mutations(root):
    source = compute(root)
    cases = [
        ('unknown output initializes particle',
         'if (plug != prevDP && plug != outDP) return MS::kUnknownParameter;', ''),
        ('required input status collapsed',
         'DOMMY_RETURN_IF_FAILURE(getValidInputDP(nextDP, data, nextDPData));',
         'if (!getValidInputDP(nextDP, data, nextDPData)) return MS::kFailure;'),
    ]
    for label, old, new in cases:
        assert old in source, label
        try:
            run(root, source.replace(old, new, 1), suite='nucleus_particle_compute',
                arguments=[], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'nucleus_particle_compute_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected particle input mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected particle input mutation: ' + label)


def collisions(root):
    source = (root / 'src/core/common/object/DNucleusCollisionUtils.h').read_text()
    return extract(source, 'namespace dommy {')


def pop_compute(root, include_helpers=True):
    source = (root / (FOLDER + 'DPnPopSolver.cpp')).read_text()
    attrs = re.findall(r'^MObject DPnPopSolver::(\w+);', source, re.M)
    result = ['static const bool DP_NPOP_SOLVER_DEBUG=false;']
    if include_helpers:
        helper = (root / 'src/core/common/utility/DMayaPluginData.h').read_text()
        result += ['namespace dommy { namespace maya {',
                   extract(helper, 'template <typename DataType>\nMStatus getPluginDataFromHandle('), '}}']
    result += ['struct DPnPopSolver:ParticleNodeBoundary {'
               'MFloatPointArray m_positions;MStatus compute(const MPlug&,MDataBlock&);'
               'MStatus initDPParticle(const DPData&,MDataBlock&);'
               'MStatus updateCommonDPParticle(MDataBlock&);'
               'MStatus initSolver(MDataBlock&);MStatus calcSolver(MDataBlock&,float);'
               'MStatus applyFields(double);' +
               ''.join('static MObject ' + a + ';' for a in attrs) + '};']
    result += ['MObject DPnPopSolver::' + a + '{"' + a + '"};' for a in attrs]
    result += [extract(source, 'MStatus DPnPopSolver::compute('),
               'DOMMY_COMPUTE_CATCH("DPnPopSolver")']
    return '\n'.join(result)


def simulation(root, include_helpers=True):
    result = ['static const bool DP_NPOP_SOLVER_DEBUG=false;']
    if include_helpers:
        validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
        result += ['namespace dommy { namespace maya {',
                   extract(validation, 'inline bool isFinitePoint('), '}}',
                   extract((root / 'src/core/common/object/DNucleusSolverControls.h').read_text(),
                           'namespace dommy {')]
    for name, method in (('DPnParticle', 'calculateDPSolver'), ('DPnPopSolver', 'calcSolver')):
        source = (root / (FOLDER + name + '.cpp')).read_text()
        attrs = re.findall(r'^MObject ' + name + r'::(\w+);', source, re.M)
        result += ['struct ' + name + ':ParticleNodeBoundary {'
                   'MFloatPointArray m_positions;MStatus applyFields(double);'
                   'MStatus ' + method + '(MDataBlock&,float);' +
                   ''.join('static MObject ' + a + ';' for a in attrs) + '};']
        result += ['MObject ' + name + '::' + a + '{"' + a + '"};' for a in attrs]
        result.append(extract(source, 'MStatus ' + name + '::' + method + '('))
        if name == 'DPnPopSolver':
            result.append(extract(source, 'MStatus DPnPopSolver::applyFields('))
    return '\n'.join(result)


def mutations(root):
    baseline(root)
    init, runtime, setup = initializers(root), compute(root), compute(root, True)
    cases = [
        ('emitter array status', 'compute',
         'DOMMY_RETURN_IF_FAILURE(stat);\n\n                const unsigned int count',
         'CHECK_MSTATUS(stat);\n\n                const unsigned int count'),
        ('emitter element status', 'compute',
         'DOMMY_RETURN_IF_FAILURE(stat);\n                    MDataHandle emitterInput',
         'CHECK_MSTATUS(stat);\n                    MDataHandle emitterInput'),
        ('emitter cursor failure', 'compute',
         'DOMMY_RETURN_IF_FAILURE(arrayHandle.next());', 'CHECK_MSTATUS(arrayHandle.next());'),
        ('position cardinality', 'compute', 'currentP.length() != currentTopoNum', 'false'),
        ('velocity cardinality', 'compute', 'currentV.length() != currentTopoNum', 'false'),
        ('output cardinality', 'compute', 'currentTopoNum != outputDPData->numberOfPoints() ||', 'false ||'),
        ('position finiteness', 'compute', '!dommy::maya::isFinitePoint(currentP[index])', 'false'),
        ('velocity finiteness', 'compute', '!dommy::maya::isFinitePoint(currentV[index])', 'false'),
        ('output attribute handle status', 'compute',
         'DOMMY_RETURN_IF_FAILURE(stat);\n            std::string velStr = velocityHandle',
         'CHECK_MSTATUS(stat);\n            std::string velStr = velocityHandle'),
        ('mutation before validation', 'compute',
         'if (currentTopoNum != outputDPData->numberOfPoints() ||',
         'outputDPData->setPosition(currentP);\n            if (currentTopoNum != outputDPData->numberOfPoints() ||'),
        ('attribute creation failure', 'init', 'DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);'),
        ('property failure', 'init', 'DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));',
         '(void)tAttr.setReadable(false);'),
        ('attribute addition failure', 'init', 'DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));',
         '(void)addAttribute(inDP);'),
        ('default string failure', 'init',
         'fnStringData.create("vel", &stat);\n    DOMMY_RETURN_IF_FAILURE(stat);',
         'fnStringData.create("vel", &stat);'),
        ('compound child failure', 'init', 'DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aActiveEmit));',
         '(void)cmpAttr.addChild(aActiveEmit);'),
        ('dependency failure', 'init', 'DOMMY_RETURN_IF_FAILURE(attributeAffects(input, prevDP));',
         '(void)attributeAffects(input, prevDP);'),
        ('initializer exception escape', 'init', 'return dommy::maya::reportEntryPointException(',
         'throw; return dommy::maya::reportEntryPointException('),
        ('compute ignores pending configuration', 'setup',
         'bool changedTopology = m_pendingParticleSetup != PendingParticleSetup::None;',
         'bool changedTopology = false;'),
        ('count-only retry fast path', 'setup',
         'm_pendingParticleSetup == PendingParticleSetup::None &&', ''),
        ('failed initialization loses policy', 'setup',
         'm_pendingParticleSetup == PendingParticleSetup::Initialization', 'false'),
        ('update is not marked pending', 'setup',
         ': PendingParticleSetup::Update;', ': PendingParticleSetup::None;'),
        ('successful configuration stays pending', 'setup',
         'm_pendingParticleSetup = PendingParticleSetup::None;\n    return MS::kSuccess;',
         'return MS::kSuccess;'),
        ('native position setter failure', 'setup',
         'DOMMY_RETURN_IF_FAILURE(fNObject.setPositions(positions, true));',
         '(void)fNObject.setPositions(positions, true);'),
        ('common setter failure', 'setup',
         'DOMMY_RETURN_IF_FAILURE(applyParticleControls(fNObject, controls));',
         '(void)applyParticleControls(fNObject, controls);'),
        ('native mutation before validation', 'setup',
         'MFloatPointArray positions;\n',
         '(void)fNObject.setTopology(static_cast<int>(count));\n    MFloatPointArray positions;\n'),
        ('particle typed scalar validation ignored', 'setup',
         'if (!result.missing && !dommy::attribute::convertScalarAttribute(',
         'if (false && !dommy::attribute::convertScalarAttribute('),
        ('point array error propagation', 'setup',
         'DOMMY_RETURN_IF_FAILURE(dommy::geometry::copyPointsToMayaArray(',
         '(void)(dommy::geometry::copyPointsToMayaArray('),
        ('common scalar validation omitted', 'setup',
         'if (!std::isfinite(static_cast<double>(finiteValues[index])))', 'if (false)'),
        ('time narrowing unchecked', 'setup',
         'if (!dommy::numeric::checkedFloatCast(\n            startTimeHandle.asTime().as(MTime::kSeconds), startTime))',
         'startTime = static_cast<float>(startTimeHandle.asTime().as(MTime::kSeconds));\n    if (false)'),
        ('solver replaced before time validation', 'setup',
         'float startTime = 0.0f;', 'solver = MnSolver();\n    float startTime = 0.0f;'),
    ]
    pop = extract(setup, 'MStatus DPnPopSolver::initSolver(')
    cases.append(('pop solver time narrowing unchecked', 'setup', pop, pop.replace(
        'if (!dommy::numeric::checkedFloatCast(\n            startTimeHandle.asTime().as(MTime::kSeconds), startTime))',
        'startTime = static_cast<float>(startTimeHandle.asTime().as(MTime::kSeconds));\n    if (false)')))
    pop = extract(setup, 'MStatus DPnPopSolver::initDPParticle(')
    pop_cases = [
        ('pop failed velocity handle consumed',
         'DOMMY_RETURN_IF_FAILURE(status);\n    const std::string velocityName',
         'CHECK_MSTATUS(status);\n    const std::string velocityName'),
        ('pop native mutation before preflight',
         'MFloatPointArray positions;',
         '(void)fNObject.setTopology(static_cast<int>(pointCount));\n    MFloatPointArray positions;'),
        ('pop point array status ignored',
         'DOMMY_RETURN_IF_FAILURE(dommy::geometry::copyPointsToMayaArray(',
         '(void)(dommy::geometry::copyPointsToMayaArray('),
        ('pop position cardinality ignored',
         'if (positions.length() != pointCount) return MS::kFailure;', ''),
        ('pop typed velocity conversion status ignored',
         'DOMMY_RETURN_IF_FAILURE(dommy::attribute::copyVector3MayaArray<Imath::V3f>(',
         '(void)(dommy::attribute::copyVector3MayaArray<Imath::V3f>('),
        ('pop typed radius validation ignored',
         'if (!dommy::attribute::convertScalarAttribute(\n                    dpoint.getAttr(radiusName)',
         'if (false && !dommy::attribute::convertScalarAttribute(\n                    dpoint.getAttr(radiusName)'),
        ('pop typed mass validation ignored',
         'if (!dommy::attribute::convertScalarAttribute(\n                    dpoint.getAttr(massName)',
         'if (false && !dommy::attribute::convertScalarAttribute(\n                    dpoint.getAttr(massName)'),
        ('pop invalid scalar accepted',
         'if (!std::isfinite(value) || value < 0.0f) return MS::kInvalidParameter;', ''),
        ('pop native position failure ignored',
         'DOMMY_RETURN_IF_FAILURE(fNObject.setPositions(positions, true));',
         '(void)fNObject.setPositions(positions, true);'),
        ('pop empty-input radius/mass policy changed', 'if (pointCount != 0U)', 'if (true)'),
    ]
    for label, old, new in pop_cases:
        assert old in pop, label
        cases.append((label, 'pop', pop, pop.replace(old, new, 1)))
    cases += [
        ('collision count failure ignored', 'collision',
         'inputs.elementCount(&status);\n    if (status != MS::kSuccess) {\n        return status;\n    }',
         'inputs.elementCount();'),
        ('collision tracking allocation after native add', 'collision',
         'registeredColliders.reserve(activeColliders.size());', ''),
        ('collision reserve after native cleanup', 'collision',
         'registeredColliders.reserve(activeColliders.size());\n\n    status = clearNucleusRigidCollisions(solver, registeredColliders);',
         'status = clearNucleusRigidCollisions(solver, registeredColliders);\n    registeredColliders.reserve(activeColliders.size());'),
        ('collision native add failure ignored', 'collision',
         'if (status != MS::kSuccess) {\n            const MStatus failure = status;',
         'if (false) {\n            const MStatus failure = status;'),
        ('collision final failure ignored', 'collision',
         'if (status == MS::kSuccess) return status;', 'return MS::kSuccess;'),
        ('collision failed removal loses tracking', 'collision',
         'if (status != MS::kSuccess) {\n            return status;\n        }\n        registeredColliders.pop_back();',
         'registeredColliders.pop_back();\n        if (status != MS::kSuccess) return status;'),
    ]
    simulation_source = simulation(root)
    field_body = extract(simulation_source, 'MStatus DPnPopSolver::applyFields(')
    for array, declaration in (('positions', 'MPointArray positions;'),
                               ('velocityVectors', 'MVectorArray velocityVectors;'),
                               ('masses', 'MDoubleArray masses;'),
                               ('totalForce', 'MVectorArray totalForce;'),
                               ('newVelocities', 'MFloatPointArray newVelocities;')):
        start = field_body.index(declaration)
        end = field_body.index('}, ' + array + '));', start) + len('}, ' + array + '));')
        block = field_body[start:end]
        cases.append(('field ' + array + ' build status ignored', 'simulation', block,
                      block.replace('DOMMY_RETURN_IF_FAILURE(', '(void)(', 1)))
    cases += [
        ('field point budget omitted', 'simulation',
         'if (numPoint > dommy::generation::kMaxGeneratedPointCount) return MS::kInvalidParameter;', ''),
        ('field native cardinality ignored', 'simulation',
         'if (!dommy::field::hasMatchingParticleFieldCounts(',
         'if (false && !dommy::field::hasMatchingParticleFieldCounts('),
        ('field nonfinite position accepted', 'simulation',
         'if (!dommy::maya::isFinitePoint(position)) return MS::kFailure;', ''),
        ('field nonfinite velocity accepted', 'simulation',
         'if (!dommy::maya::isFinitePoint(velocity)) return MS::kFailure;', ''),
        ('field force cardinality ignored', 'simulation',
         'if (fieldForce.length() != numPoint)', 'if (false)'),
        ('field accumulation status ignored', 'simulation',
         'status = totalForce.set(sum, index);\n            if (status != MS::kSuccess) return status;',
         'status = totalForce.set(sum, index);'),
        ('field inverse mass mapping changed', 'simulation',
         'const double inverseMass = static_cast<double>(inverseMasses[index]);',
         'const double inverseMass = masses[index];'),
        ('field time step ignored', 'simulation',
         'deltaTime, updated[0])', '1.0, updated[0])'),
        ('field velocity w discarded', 'simulation', 'current.w);', '1.0f);'),
        ('solver direction is borrowed', 'simulation',
         'const float gravityDirection[] = {gravitySource[0], gravitySource[1], gravitySource[2]};',
         'const float3& gravityDirection = gravitySource;'),
        ('solver disabled inversion changed', 'simulation',
         'solver.setDisabled(!enabled)', 'solver.setDisabled(enabled)'),
        ('solver iteration validation omitted', 'simulation',
         'substeps <= 0 || iterations <= 0', 'false'),
        ('solver setter failure ignored', 'simulation',
         'DOMMY_RETURN_IF_FAILURE(solver.setGravity(gravity));',
         '(void)solver.setGravity(gravity);'),
    ]
    for attribute in ('aGravity', 'aGravityDirection', 'aAirDensity', 'aWindSpeed',
                      'aWindDirection', 'aWindNoiseIntensity', 'aSubsteps',
                      'aSolverIterations', 'aEnable'):
        block = 'data.inputValue(Node::' + attribute + ', &status);\n    DOMMY_RETURN_IF_FAILURE(status);'
        cases.append(('solver failed ' + attribute + ' handle consumed', 'simulation', block,
                      block.replace('DOMMY_RETURN_IF_FAILURE(status);', 'CHECK_MSTATUS(status);')))
    pop_runtime = pop_compute(root)
    handle_pattern = (r'(?:const )?MDataHandle (\w+) = data\.inputValue\([^;]+;'
                      r'\s*DOMMY_RETURN_IF_FAILURE\(stat\);')
    for kind, source in (('compute', runtime), ('pop_compute', pop_runtime)):
        for index, match in enumerate(re.finditer(handle_pattern, source)):
            if match.group(1) == 'velocityHandle':
                continue  # Output-velocity handle was already covered above.
            block = match.group(0)
            changed = block.replace('DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);')
            # CurrentTime is read in two distinct output branches. Mutate the
            # selected occurrence, not the first identical source fragment.
            cases.append((kind + ' failed ' + match.group(1) + ' ' + str(index), kind, source,
                          source[:match.start()] + changed + source[match.end():]))
    for kind, source in (('compute', runtime), ('pop_compute', pop_runtime)):
        block = re.search(r'if \(!std::isfinite\(startTime.value\(\)\) \|\|\s*'
                          r'!std::isfinite\(startTime.as\(MTime::kSeconds\)\)\) \{\s*'
                          r'return MS::kInvalidParameter;\s*\}', source).group(0)
        cases.append((kind + ' invalid start time accepted', kind, block, ''))
        time = 'currTime' if kind == 'compute' else 'time'
        block = 'dommy::numeric::checkedFloatCast(' + time + '.as(MTime::kSeconds), solveTime)'
        cases.append((kind + ' solver time narrowing unchecked', kind, block,
                      '(solveTime = static_cast<float>(' + time + '.as(MTime::kSeconds)), true)'))
    block = re.search(r'if \(!std::isfinite\(currTime.value\(\)\) \|\|\s*'
                      r'!std::isfinite\(currTime.as\(MTime::kSeconds\)\)\) \{\s*'
                      r'return MS::kInvalidParameter;\s*\}', runtime).group(0)
    cases.append(('previous output invalid current time accepted', 'compute', block, ''))
    cases += [
        ('emitter typed input status ignored', 'compute',
         'DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(',
         '(void)(dommy::maya::getPluginDataFromHandle('),
        ('emitter invalid topology accepted', 'compute',
         'if (!emitDPData->hasValidPointTopology()) return MS::kInvalidParameter;', ''),
        ('emitter connected null accepted', 'compute',
         'if (connected) return MS::kFailure;', '(void)connected;'),
        ('emitter logical index replaced by physical index', 'compute',
         '.elementByLogicalIndex(logicalIndex, &stat)', '.elementByLogicalIndex(i, &stat)'),
        ('output velocity type admission omitted', 'compute',
         'if (!attribute || !isVector3DAttrType(attribute->getAttrType()))', 'if (false)'),
        ('force float intermediate restored', 'compute',
         'checkedMultiplyFieldValue(forceArray[i][axis], dt, impulse)',
         'checkedMultiplyFieldValue(float(forceArray[i][axis] * float(dt)), 1.0, impulse)'),
        ('force float envelope intermediate restored', 'compute',
         'checkedMultiplyFieldValue(impulse, forceEnv, impulse)',
         'checkedMultiplyFieldValue(float(impulse * forceEnv), 1.0, impulse)'),
        ('force narrowing unchecked', 'compute',
         'dommy::numeric::checkedFloatCast(velocity, velArray[i][axis])',
         '(velArray[i][axis] = static_cast<float>(velocity), true)'),
        ('optional input typed read failure ignored', 'pop_compute',
         'DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(',
         '(void)(dommy::maya::getPluginDataFromHandle('),
        ('optional input topology ignored', 'pop_compute',
         'if (!inputDPData->hasValidPointTopology()) return MS::kInvalidParameter;', ''),
        ('optional connected null accepted', 'pop_compute',
         'if (connected) return MS::kFailure;', '(void)connected;'),
        ('optional connection query failure ignored', 'pop_compute',
         'inputPlug.isConnected(&stat);\n                        DOMMY_RETURN_IF_FAILURE(stat);',
         'inputPlug.isConnected(&stat);'),
        ('optional data treated as absent', 'pop_compute',
         'if (!inputHandle.data().isNull())', 'if (false)'),
    ]
    for operation in ('arrayHandle.elementIndex(&stat)',
                      '.elementByLogicalIndex(logicalIndex, &stat)',
                      'element.child(aEmitDP, &stat)', 'input.isConnected(&stat)'):
        match=re.search(re.escape(operation)+r';\s*DOMMY_RETURN_IF_FAILURE\(stat\);',runtime)
        assert match, operation
        block=match.group(0)
        cases.append(('emitter query status '+operation,'compute',block,
                      block.replace('DOMMY_RETURN_IF_FAILURE(stat);','CHECK_MSTATUS(stat);')))
    # Exercise both force and velocity conversion calls independently. Setup's
    # identically named converter must not accidentally be mutated instead.
    pattern=r'DOMMY_RETURN_IF_FAILURE\(dommy::attribute::copyVector3MayaArray<Imath::V3f>\([\s\S]+?\)\);'
    for index,match in enumerate(re.finditer(pattern,runtime)):
        block=match.group(0)
        cases.append(('force typed array status '+str(index),'compute',runtime,
                      runtime[:match.start()]+block.replace('DOMMY_RETURN_IF_FAILURE(', '(void)(',1)+runtime[match.end():]))
    setup_start=setup.index('MStatus DPnParticle::configureDPDataParticle(')
    match=re.search(pattern,setup[setup_start:])
    block=match.group(0)
    cases.append(('particle setup typed velocity status','setup',block,
                  block.replace('DOMMY_RETURN_IF_FAILURE(', '(void)(',1)))
    originals = {'init': init, 'compute': runtime, 'setup': setup,
                 'pop': setup, 'collision': collisions(root), 'simulation': simulation_source,
                 'pop_compute': pop_runtime}
    for label, kind, old, new in cases:
        original = originals[kind]
        assert old in original, label
        suite = {'pop': 'nucleus_pop_setup', 'collision': 'nucleus_collision',
                 'simulation': 'nucleus_simulation', 'pop_compute': 'nucleus_pop_compute'}.get(
            kind, 'nucleus_particle_' + kind)
        try:
            run(root, original.replace(old, new, 1), suite=suite, capture=True,
                arguments=None if kind == 'init' else [])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == suite + '_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    print('Nucleus mutations: all ' + str(len(cases)) + ' detected at runtime', flush=True)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--emit' in sys.argv:
        run(root, initializers(root), suite='nucleus_particle_init', arguments=['--emit'])
    elif '--sdk' in sys.argv:
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1])
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        baseline(root)
        required_input_mutations(root)
