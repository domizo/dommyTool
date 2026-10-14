#!/usr/bin/env python3
"""Complete DPFoamMesh compute with injected native-array and publication failures."""
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

ROOT = Path(__file__).resolve().parent.parent
NODE = 'src/modules/point/DPoint/mesh/DPFoamMesh.cpp'


def body(source=None):
    source = source if source is not None else (ROOT / NODE).read_text()
    validation = (ROOT / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    result = ['namespace dommy {namespace maya {']
    result.extend(extract(validation, 'inline bool ' + name + '(')
                  for name in ('isFinitePoint', 'isFiniteVector', 'isFloatRepresentablePoint'))
    result.append('}}')
    attrs = declarations('DPFoamMesh', source, True)
    attrs[0] = attrs[0].replace('static MStatus initialize();',
        'MStatus compute(const MPlug&,MDataBlock&);MString name()const{return "DPFoamMesh";}'
        'static MStatus initialize();')
    result.extend(attrs)
    result.append(extract(source, 'MStatus DPFoamMesh::initialize('))
    result.append(extract(source, 'MStatus DPFoamMesh::compute('))
    result.append('DOMMY_COMPUTE_CATCH("DPFoamMesh")')
    return '\n'.join(result)


def check(source=None, capture=False):
    return run(ROOT, body(source), suite='point_foam', arguments=[], capture=capture)


def mutations():
    source=(ROOT/NODE).read_text()
    for label,old,new in (
        ('empty output omitted','if (pointCount == 0U) {','if (pointCount == 0U) return MS::kUnknownParameter;\n    if (false) {'),
        ('input status ignored','CHECK_MSTATUS_AND_RETURN_IT(stat);','(void)stat;'),
        ('point append ignored','DOMMY_RETURN_IF_FAILURE(finVertectPointArray.append(newPoint));',
         '(void)finVertectPointArray.append(newPoint);'),
        ('face append ignored','DOMMY_RETURN_IF_FAILURE(finPolyCountArray.append(basePolyCountArray[j]));',
         '(void)finPolyCountArray.append(basePolyCountArray[j]);'),
        ('deformation copy ignored','DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(\n                baseVertexArray, newDeformedVtxArray));',
         '(void)dommy::maya::copyCheckedArray(baseVertexArray, newDeformedVtxArray);'),
        ('output handle status ignored','MDataHandle output = data.outputValue(outDM, &stat);\n    CHECK_MSTATUS_AND_RETURN_IT(stat);',
         'MDataHandle output = data.outputValue(outDM, &stat);'),
        ('null generated mesh ignored','if (generatedMesh.isNull()) return MS::kFailure;',''),
    ):
        assert old in source,label
        try: check(source.replace(old,new),capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name=='point_foam_tests','Compile failure: '+label
            assert any(marker in error.stderr for marker in ('Assertion failed','Assertion ','runtime error:'))
            print('Detected mutation: '+label,flush=True)
        else: raise AssertionError('Undetected mutation: '+label)


def sdk_boundary(sdk):
    # Real Maya geometry/array/plugin APIs; only custom DP/DM declarations are
    # substituted. This does not compile their implementations or link a plug-in.
    source=(ROOT/NODE).read_text()
    attrs=declarations('DPFoamMesh',source,True)
    attrs[0]=attrs[0].replace(':AttributeNode', ': public MPxNode').replace(
        'static MStatus initialize();','MStatus compute(const MPlug&,MDataBlock&);static MStatus initialize();')
    attrs[1:]=[re.sub(r'\{"[^\"]+"\}', '{}', value) for value in attrs[1:]]
    prefix='''
#include <maya/MPxNode.h>
#include <maya/MPxData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MFloatPointArray.h>
#include <maya/MIntArray.h>
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "modules/generators/icosahedron/DIcosahedronTopology.h"
#include "math/dommyMath.h"
#include <array>
#include <memory>
using DPointList=std::vector<std::array<float,3>>;
using DScalarDataList=std::vector<float>;
class DPData:public MPxData {
public:
    static const MTypeId id;
    std::size_t numberOfPoints()const;
    const DPointList* getPositionPtrConst()const;
    bool isExistAttr(const std::string&)const;
    bool hasValidPointTopology()const;
};
class DMeshInternalData;
using DMItem=std::shared_ptr<DMeshInternalData>;
class DMeshData:public MPxData {
public:
    static const MTypeId id;
    bool hasValidMeshTopology()const;
    bool insert(const DMItem&);
    void updateBBox();
};
class DMeshInternalData {
public:static MStatus createFromMayaObject(const MObject&,DMItem&);
};
void getDScalarArrayFromAttribute(const DPData&,const std::string&,DScalarDataList&);
'''
    code=prefix+'\n'.join(attrs)+extract(source,'MStatus DPFoamMesh::initialize(')+\
        extract(source,'MStatus DPFoamMesh::compute(')+'\nDOMMY_COMPUTE_CATCH("DPFoamMesh")'
    command=shlex.split(os.environ.get('CXX','clang++'))+[
        '-std=c++14','-Wall','-Wextra','-Wpedantic','-Werror',
        '-Wno-deprecated-declarations','-Wno-unused-parameter','-Wno-unknown-pragmas',
        '-isystem',str(Path(sdk)/'include'),'-I'+str(ROOT/'src'),'-I'+str(ROOT/'src/core'),
        '-x','c++','-']
    for flags in (['-fsyntax-only'],['--analyze','-Xanalyzer','-analyzer-output=text',
                                   '-Xanalyzer','-analyzer-werror','-o',os.devnull]):
        subprocess.run(command+flags,input=code,text=True,check=True)
    print('DPFoamMesh actual compute/initializer: real Maya API boundary syntax/analysis passed')


if __name__ == '__main__':
    if '--sdk-boundary' in sys.argv:
        sdk_boundary(sys.argv[sys.argv.index('--sdk-boundary')+1])
    elif '--sdk' in sys.argv:
        from check_motion_duplicator_safety import sdk_check
        sdk_check(ROOT, sys.argv[sys.argv.index('--sdk') + 1], NODE, 'DPFoamMesh')
    elif '--mutations' in sys.argv:
        mutations()
    else:
        check()
