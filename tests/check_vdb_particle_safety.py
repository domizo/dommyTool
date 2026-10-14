#!/usr/bin/env python3
"""Particle raster initialization contracts and bounded conversion admission."""
from pathlib import Path
import sys
import os, re, shlex, subprocess, tempfile
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run
from check_vdb_distribution_safety import compute_bodies as distribution_bodies

NODES = ('DOpenVDBFromParticle', 'DOpenVDBFromDP', 'DVDBFoamGen')

def initializers(root):
    result = []
    for name in NODES:
        source = (root / ('src/modules/volume/VDB/Generate/' + name + '.cpp')).read_text()
        result += declarations(name, source, True)
        result += [extract(source, 'MStatus ' + name + '::initialize(')]
    return '\n'.join(result)

def planning(root):
    # A renamed exact body allows behavioral mutations of the production
    # admission function without replacing the shared header on disk.
    raster = extract((root/'src/modules/volume/VDB/Common/DVdbParticleRasterPlan.h').read_text(),
                     'inline bool checkedParticleRasterWork(').replace(
                         'checkedParticleRasterWork(', 'testedParticleRasterWork(')
    policy = extract(
        (root / 'src/modules/volume/VDB/Edit/DOpenVDBParticleConversionPolicy.h').read_text(),
        'namespace dommy {').replace('checkedParticleRasterWork(', 'testedParticleRasterWork(')
    return (distribution_bodies(root).split('struct DOpenVDBScatter {')[0] +
            '\nnamespace dommy { namespace vdb {\n' + raster + '\n}}\n' + policy)

def sdk_check(root, sdk):
    code=['#include <maya/'+n+'.h>' for n in ('MPxNode','MFnNumericAttribute','MFnTypedAttribute',
        'MFnGenericAttribute','MFnEnumAttribute','MFnCompoundAttribute','MFnStringData',
        'MFnDependencyNode','MDataHandle','MArrayDataHandle','MDataBlock','MPlug','MPointArray',
        'MVectorArray','MDoubleArray')]
    code+=['#include <sstream>','#include <vector>',
        '#include "common/utility/DComputeGuard.h"','#include "common/utility/DStatusOperation.h"',
        '#include "modules/volume/VDB/Common/DVdbParticleRasterPlan.h"',r'''
using DPointList=std::vector<MPoint>;
struct DPData {static const MTypeId id;bool hasValidPointTopology()const;std::size_t numberOfPoints()const;
    const DPointList* getPositionPtrConst()const;bool isExistAttr(const std::string&)const;};
struct DOpenVDBData {static const MTypeId id;};
namespace openvdb {enum VecType {V0,V1,V2,V3,V4};const int NUM_VEC_TYPES=5;
struct GridBase {static std::string vecTypeExamples(VecType);};}
namespace dommy {namespace maya {template<typename T>MStatus getPluginDataFromHandle(const MDataHandle&,const MTypeId&,const T*&);}
namespace vdb {const std::size_t kMaxVdbPayloadBytes=512U*1024U*1024U;struct TreeEditBudget {};
bool planParticleConversion(std::size_t,const ParticleInputExtrema&,const ParticleRasterParameters&,
    std::size_t,std::size_t,TreeEditBudget&,float&);}}
void getMDoubleArrayFromAttribute(const DPData&,const std::string&,MDoubleArray&);
void getMVectorArrayFromAttribute(const DPData&,const std::string&,MVectorArray&);
void EchoWarning(const char*,const char*);
const unsigned int kMaxExportedPointAttributeCount=1024;
''']
    # Copy the actual block-based reader declarations, not obsolete live-plug
    # overloads. The SDK boundary must follow the production caller contract.
    common=(root/'src/modules/point/DPoint/Common/DPointCommon.h').read_text()
    readers=re.findall(
        r'MStatus get(?:Point|Double|Vector)ArrayFromData\(MDataBlock& block, const MObject& attribute, M(?:Point|Double|Vector)Array& output\);',
        common)
    assert len(readers)==3,readers
    code+=readers
    for name in NODES:
        source=(root/('src/modules/volume/VDB/Generate/'+name+'.cpp')).read_text()
        declared=declarations(name,source,True)
        declared[0]=declared[0].replace(':AttributeNode',':MPxNode').replace('static MStatus initialize();',
            'static MStatus initialize();MStatus compute(const MPlug&,MDataBlock&);')
        declared[1:]=[re.sub(r'\{"[^"]*"\}','',s) for s in declared[1:]]
        prefix=extract(source,'MStatus '+name+'::compute(').split(
            '#pragma mark get output vdb' if name=='DVDBFoamGen' else 'ParticleList paList(')[0]
        depth=prefix.count('{')-prefix.count('}')
        if name!='DVDBFoamGen': prefix+='(void)pruneVal; (void)rasterMaximumRadiusVoxels;\n'
        prefix+='return MS::kSuccess;\n'+'}\n'*(depth-1)+'return MS::kUnknownParameter;\n}\nDOMMY_COMPUTE_CATCH("'+name+'")'
        code+=declared+[extract(source,'MStatus '+name+'::initialize('),prefix]
    cmd=shlex.split(os.environ.get('CXX','clang++'))+['-std=c++14','-Wall','-Wextra','-Werror',
        '-Wno-unused-variable','-Wno-unknown-pragmas','-Wno-deprecated-declarations',
        '-isystem',str(Path(sdk)/'include'),'-I'+str(root/'src'),'-I'+str(root/'src/core')]
    with tempfile.TemporaryDirectory(prefix='dommy-particle-sdk-') as directory:
        path=Path(directory)/'particle.cpp';path.write_text('\n'.join(code))
        subprocess.run(cmd+['-fsyntax-only',str(path)],check=True)
        subprocess.run(cmd+['--analyze','-Xanalyzer','-analyzer-output=text','-Xanalyzer',
            '-analyzer-werror','-o',os.devnull,str(path)],check=True)
    print('Three particle initializers and compute intake through admission: Maya SDK syntax/analysis passed')
    print('Native raster kernels, attribute transfer and full translation units are NOT covered')

def mutations(root):
    body=planning(root)
    for label,old,new in [
        ('particle coordinate bound omitted','std::fabs(box.min()[axis]) > 1e6 || std::fabs(box.max()[axis]) > 1e6','false'),
        ('particle input bytes uncharged','!candidate.addBytes(count, 160)','false'),
        ('particle aggregate work uncharged','!candidate.addWork(work, 1)','false'),
        ('trail precision guard omitted','minimumProgress < roundingMargin','false'),
        ('trail iterations uncharged','perParticle, trailSteps, maximumWorkItems, perTrail',
         'perParticle, 1U, maximumWorkItems, perTrail'),
        ('radius cancellation guard omitted',
         'minimumRadius < 64.0 * std::numeric_limits<float>::epsilon() *\n                                std::max(radiusVoxels, double(minimumRadius))', 'false'),
    ]:
        assert old in body,label
        try:run(root,body.replace(old,new),capture=True,suite='vdb_particle_plan',arguments=[])
        except subprocess.CalledProcessError as error:
            assert error.returncode!=1 or 'runtime error:' in (error.stderr or ''), 'Compile failure is not detection: '+label
            print('Detected mutation: '+label)
        else:raise AssertionError('Undetected mutation: '+label)

if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        sdk_check(root,sys.argv[sys.argv.index('--sdk')+1]);sys.exit(0)
    if '--mutations' in sys.argv:mutations(root);sys.exit(0)
    run(root, initializers(root), suite='vdb_particle_init',
        arguments=['--emit'] if '--emit' in sys.argv else None)
    if '--emit' not in sys.argv:
        run(root, planning(root), suite='vdb_particle_plan', arguments=[])
