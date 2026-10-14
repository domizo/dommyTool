#!/usr/bin/env python3
"""Check extracted mesh transactions, initializer contracts and host boundaries.

Scalar doubles exercise failure ordering without allocating oversized geometry.
They do not emulate Maya's DG, matrix implementation or viewport evaluation.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run
from check_point_utility_safety import declarations

PATHS = {
    'DMCopyTransform': 'src/modules/mesh/DMesh/Utility/DMCopyTransform.cpp',
    'DMWireGen': 'src/modules/mesh/DMesh/Generate/DMWireGen.cpp',
    'DPolyWire': 'src/modules/mesh/mesh/DPolyWire.cpp',
}
DATA = 'src/modules/mesh/DMesh/Data/DMeshData.cpp'


def validation_bodies(root):
    source = (root / DATA).read_text()
    return '#include "common/utility/DMeshInputValidation.h"\n'+ '\n\n'.join(extract(source, signature) for signature in (
        'bool DMeshInternalData::hasValidPolygonTopology(',
        'bool DMeshInternalData::hasValidUVTopology(',
        'bool DMeshInternalData::hasValidMeshData('))


def append_bodies(root, source=None):
    source = source or (root / DATA).read_text()
    signatures = ('bool DMeshData::getCheckedTotalVertex(', 'bool DMeshData::getCheckedTotalFace(',
                  'bool DMeshData::getCheckedTotalConnect(', 'bool DMeshData::hasValidMeshTopology(',
                  'MStatus DMeshData::copyFrom(', 'bool DMeshData::appendMeshes(',
                  'bool DMeshData::insert(const DMeshInternalData&',
                  'bool DMeshData::insert(const DMItem&', 'bool DMeshData::insert(const ConstDMItem&',
                  'bool DMeshData::insert(const DMeshList&',
                  'bool DMeshData::expand(const DMItem&',
                  'bool DMeshData::expand(const DMeshList&')
    result = [extract(source, signature) for signature in signatures]
    # Execute real count-only preflights. Complete merge transactions are tested
    # separately; native geometry/matrix evaluation remains a host boundary.
    start = source.index('dommy::mesh::MeshMergePlan mergedPlan;', source.index('MStatus DMeshData::copyMergedFrom('))
    end = source.index('std::size_t mergedMeshCount', start)
    assert end < source.index('candidate.meshItemArray.push_back', start)
    block = source[start:end].replace('return MS::kInvalidParameter;', 'return false;')
    result.append('bool DMeshData::mergeStorageAdmission(const DMeshData& data) const {' +
                  'const DMeshData& base = *this;' + block + 'return true;}')
    for name, path, marker, parameters in (
            ('copyTransformAdmission', PATHS['DMCopyTransform'],
             'const std::size_t meshCount =', ', std::size_t count'),
            ('mergeNodeAdmission', 'src/modules/mesh/DMesh/Utility/DMMerge.cpp',
             'dommy::mesh::MeshMergePlan outputPlan;', ', const DMeshData* targetDMData')):
        node = (root/path).read_text()
        start = node.index(marker, node.index('::compute('))
        end = node.index('MFnPluginData pluginData;' if name == 'mergeNodeAdmission'
                         else '// Admit repeated metadata', start)
        copy_call = 'outDMeshes->copyMergedFrom(' if name == 'mergeNodeAdmission' else 'outDMeshes->copyRepeatedFrom('
        assert end < node.index(copy_call, end)
        result.append('MStatus '+name+'(const DMeshData* inputDMData'+parameters+') {' +
                      node[start:end] + 'return MS::kSuccess;}')
    return '\n\n'.join(result)


def node_bodies(root, overrides=None):
    overrides = overrides or {}
    result, readers = [], []
    for name, path in PATHS.items():
        source = overrides.get(path, (root / path).read_text())
        if name in ('DMWireGen','DPolyWire'):
            assert not re.search(r'\.isConnected\(\)',source), name+' has an unchecked connection query'
        attrs = re.findall(r'^MObject '+name+r'::(\w+);', source, re.M)
        result.append('struct '+name+':AttributeNode {static MStatus initialize();'
                      'MObject thisMObject()const{return {"node"};}' +
                      ''.join('static MObject '+a+';' for a in attrs)+'};')
        result.extend('MObject '+name+'::'+a+';' for a in attrs)
        result.append(extract(source, 'MStatus '+name+'::initialize('))
        pattern = (r'const MDataHandle (\w+) = data\.inputValue\((\w+), &(stat|status)\);\s*'
                   r'if \(!\3\) return \3;\s*(?:const )?(?:int|short|double|bool|MVector|double3|double2|std::string)\s*&?\s*(\w+) = '
                   r'\1\.as\w+\(\)(?:\.asChar\(\))?;')
        for match in re.finditer(pattern, source):
            body = match[0].replace('inputValue('+match[2]+',', 'inputValue('+name+'::'+match[2]+',')
            function = 'readControl'+str(len(readers))
            result.append('MStatus '+function+'(MDataBlock& data) {MStatus '+match[3]+';'+body+
                          '(void)'+match[4]+';return MS::kSuccess;}')
            readers.append(function)
        if name == 'DMCopyTransform':
            start = source.index('DOMMY_RETURN_IF_FAILURE(newMat.setRotatePivot')
            block = source[start:source.index('const MMatrix finMat', start)]
            result.append('MStatus transformCalls() {TransformBoundary newMat;'
                          'MVector pivot(2,3,4),movVec(5,6,7),prescale(8,9,10);'
                          'double3 finRotVal={.1,.2,.3}; int rotOrder=2;'+block+'return MS::kSuccess;}')
            if source.count('outDMeshes->copyRepeatedFrom(') != 1 or 'outDMeshes->expand(' in source:
                raise RuntimeError('Copy transform must retain one repeated-copy transaction')
        else:
            start = source.index('// Read only scalar profile counts')
            block = source[start:source.index('#pragma mark', start)]
            for attr in attrs:
                block = re.sub(r'\b'+attr+r'\b', name+'::'+attr, block)
            result.append('MStatus profile'+name+'(std::size_t samplingVal,std::size_t checkedCircleSampling,'
                          'int profileModeVal,double circleRadiusVal,std::size_t meshCount) {'
                          'MStatus stat; MDataBlock data; MObject node{"node"};'
                          'ProfileCurves curves(meshCount); const auto* inputDCData=&curves;'
                          'double2 uvTransVal={0,0},uvScaleVal={1,1};'+
                          block.replace('thisMObject()', 'node')+
                          'profileResult=profPointArray; topologyResult=topologyPlan; return MS::kSuccess;}')
    if len(readers) != 34:
        raise RuntimeError('Incomplete control inventory: '+str(len(readers)))
    result.append('using ControlReader=MStatus(*)(MDataBlock&);std::vector<ControlReader> controlReaders={'+
                  ','.join(readers)+'};')
    return '\n\n'.join(result)


def piece_body(root):
    source = (root/'src/modules/mesh/DMesh/Utility/DMMakePiece.cpp').read_text()
    return '\n'.join(declarations('DMMakePiece', source, False) + [
        extract(source, 'MStatus DMMakePiece::compute('),
        'DOMMY_COMPUTE_CATCH("DMMakePiece")'])


def check_sdk(root, sdk):
    includes = ['-I'+str(directory) for directory, _, _ in os.walk(root/'src')]
    command = shlex.split(os.environ.get('CXX','clang++')) + [
        '-std=c++14','-Wall','-Wextra','-Wpedantic','-Werror',
        '-Wno-deprecated-declarations','-Wno-unused-parameter','-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk)/'include')] + includes + [
        '-isystem', str(root/'third_party/libnoise/source'), '-idirafter',str(root/'tests/stubs')]
    paths = [DATA, PATHS['DMCopyTransform'], 'src/modules/mesh/DMesh/Utility/DMMerge.cpp',
             'src/modules/mesh/DMesh/Utility/DMMakePiece.cpp']
    dependency = Path(os.environ.get('DOMMY_LEGACY_DEPENDENCY_ROOT',
        str(Path(os.environ.get('TMPDIR','/tmp'))/'dommytool-legacy-dependencies')))
    seexpr = dependency/'SeExpr-a5f02bb03199630759b0b94a64f37ce56c08675a/src/SeExpr'
    if (seexpr/'SeExpression.h').is_file():
        command += ['-isystem', str(seexpr)]
        paths += [PATHS['DMWireGen'],PATHS['DPolyWire']]
    else:
        print('SKIP complete DMWireGen/DPolyWire SDK units: legacy SeExpr headers unavailable')
    for path in paths:
        subprocess.run(command+['-fsyntax-only',str(root/path)],check=True)
        subprocess.run(command+['--analyze','-Xanalyzer','-analyzer-output=text',
            '-Xanalyzer','-analyzer-werror','-o','/dev/null',str(root/path)],check=True)
    print('Mesh generation: '+str(len(paths))+' complete SDK translation units passed syntax/analysis')


def profile_mutations(root):
    for name in ('DMWireGen','DPolyWire'):
        path=PATHS[name]
        source=(root/path).read_text()
        old=('profileConnected = profCrvPlug.isConnected(&stat);\n'
             '                    DOMMY_RETURN_IF_FAILURE(stat);' if name=='DMWireGen' else
             'const bool profileConnected = profCrvPlug.isConnected(&stat);\n'
             '            DOMMY_RETURN_IF_FAILURE(stat);')
        assert old in source,name
        changed=source.replace(old,old[:old.index('\n')],1)
        try:
            run(root,node_bodies(root,{path:changed}),suite='mesh_generation',
                arguments=[str(root/'tests/fixtures/mesh_generation_initialization_contract.txt')],capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name=='mesh_generation_tests','Compile failure: '+name
            print('Detected mutation: '+name+' profile connection failure ignored',flush=True)
        else:
            raise AssertionError('Undetected profile connection mutation: '+name)


def mutations(root):
    append = append_bodies(root)
    validation = validation_bodies(root)
    piece = piece_body(root)
    run(root, append, suite='mesh_append', arguments=[])
    run(root, validation, suite='mesh_validation', arguments=[])
    cases = (
        ('mesh append byte admission omitted', 'mesh_append', append,
         'if (!dommy::mesh::validMeshOperationStorage(plan)) return false;', ''),
        ('mesh copy byte admission omitted', 'mesh_append', append,
         '!dommy::mesh::validMeshOperationStorage(plan)) return MS::kInvalidParameter;',
         'false) return MS::kInvalidParameter;'),
        ('reference insert clones before admission', 'mesh_append', append,
         'const MeshSourceList meshes(1U, &data);',
         'const auto premature = data.copyNewMesh(); const MeshSourceList meshes(1U, premature.get());'),
        ('reference insert clones twice', 'mesh_append', append,
         'const MeshSourceList meshes(1U, &data);',
         'if (data.numberOfPoints() == 3U) {const auto premature = data.copyNewMesh(); '
         'const MeshSourceList meshes(1U, premature.get()); return appendMeshes(meshes);} '
         'const MeshSourceList meshes(1U, &data);'),
        ('copy transform aggregate byte admission omitted', 'mesh_append', append,
         '!dommy::mesh::validMeshOperationStorage(outputPlan))\n        return MS::kInvalidParameter;',
         'false)\n        return MS::kInvalidParameter;'),
        ('merge node aggregate byte admission omitted', 'mesh_append', append,
         '!dommy::mesh::validMeshOperationStorage(outputPlan)) return MS::kInvalidParameter;',
         'false) return MS::kInvalidParameter;'),
        ('merge transaction aggregate byte admission omitted', 'mesh_append', append,
         '!dommy::mesh::validMeshOperationStorage(mergedPlan)) {', 'false) {'),
        ('piece aggregate byte admission omitted', 'mesh_piece', piece,
         'if (!dommy::mesh::validMeshOperationStorage(outputBudget))\n            return MS::kInvalidParameter;', ''),
        ('unnecessary edge enumeration', 'mesh_validation', validation,
         'if (mEdgeGroupList.empty()) return true;', ''),
        ('invalid topology accepted', 'mesh_validation', validation,
         '!hasValidPolygonTopology() ||', ''),
        ('repeated primitive validation', 'mesh_append', append,
         '// Validate each primitive once',
         'for (ConstDMItem mesh : meshItemArray) if (!mesh || !mesh->hasValidMeshData()) return false;\n'
         '    // Validate each primitive once'),
    )
    for label, suite, source, old, new in cases:
        assert old in source, label
        try:
            run(root, source.replace(old, new, 1), suite=suite, arguments=[], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == suite + '_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv)==3 and sys.argv[1]=='--sdk':
        check_sdk(root,sys.argv[2])
    elif '--mutations' in sys.argv:
        profile_mutations(root)
        mutations(root)
    elif '--profile-mutations' in sys.argv:
        profile_mutations(root)
    else:
        run(root,append_bodies(root),suite='mesh_append',arguments=[])
        run(root,validation_bodies(root),suite='mesh_validation',arguments=[])
        run(root,piece_body(root),suite='mesh_piece',arguments=[])
        run(root,node_bodies(root),suite='mesh_generation',arguments=sys.argv[1:] or
            [str(root/'tests/fixtures/mesh_generation_initialization_contract.txt')])
