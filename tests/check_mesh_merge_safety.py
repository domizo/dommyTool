#!/usr/bin/env python3
"""Execute complete mesh-merge/copy bodies with real typed metadata.

Native geometry, transforms, bounds and Maya publication are explicit transport
boundaries. Small overridden budgets test rejection without large allocations.
"""
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

DATA = 'src/modules/mesh/DMesh/Data/DMeshData.cpp'


def bodies(root):
    source = (root / DATA).read_text()
    signatures = [
        'bool DMeshData::hasValidMeshTopology(', 'MStatus DMeshData::copyFrom(',
        'bool DMeshData::mergeDMeshData(', 'bool DMeshData::appendMeshes(', 'bool DMeshData::hasUVs(',
        'DStringList DMeshData::getPointAttributeList(',
        'DP_ATTR_DATA_TYPE DMeshData::getPointAttributeType(',
        'DStringList DMeshData::getAllCompGroupName(']
    if 'MStatus DMeshData::copyMergedFrom(' in source:
        signatures.append('MStatus DMeshData::copyMergedFrom(')
    result = [extract(source, signature) for signature in signatures]
    for path, functions in (
            ('src/modules/point/DPoint/Common/DPAttributeCommon.cpp',
             ('void CopyAttributeToAttribute(', 'bool appendOtherDPAttribute(')),
            ('src/core/Data/DGroupCommon.cpp',
             ('void CopyGroupToGroup(', 'bool IsExistInGroupList(', 'bool MergeGroupToGroup(',
              'void ExpandGroupListSize('))):
        text = (root / path).read_text()
        result.extend(extract(text, function) for function in functions)
    node = (root / 'src/modules/mesh/DMesh/Utility/DMMerge.cpp').read_text()
    result.extend((
        'struct DMMerge {MStatus compute(const MPlug&, MDataBlock&); '
        'static MObject inDM, targetDM, outDM;};',
        'MObject DMMerge::inDM{"input"}, DMMerge::targetDM{"target"}, DMMerge::outDM{"output"};',
        extract(node, 'MStatus DMMerge::compute('), 'DOMMY_COMPUTE_CATCH("DMMerge")'))
    return '\n\n'.join(result)


def execute(root, source, capture=False):
    limits = (root / 'src/core/common/utility/DGenerationLimits.h').read_text()
    limits = limits.replace('kMaxGeneratedStringByteCount = 100000000U',
                            'kMaxGeneratedStringByteCount = 128U')
    limits = limits.replace('kMaxWorkItemCount = 100000000U', 'kMaxWorkItemCount = 512U')
    return run(root, source, suite='mesh_merge', arguments=[], capture=capture,
               header_overrides={'common/utility/DGenerationLimits.h': limits},
               extra_flags=[str(root / 'src/core/DDataUtility/Data/DPGroupData.cpp')])


def schemas(root):
    source = bodies(root)
    # CountingLess preserves the exact std::less<string> ordering. Actual
    # containers and merge bodies are unchanged apart from instrumentation.
    source = source.replace('std::set<std::string>', 'std::set<std::string, CountingLess>')
    source = source.replace('std::map<std::string, DP_ATTR_DATA_TYPE>',
                            'std::map<std::string, DP_ATTR_DATA_TYPE, CountingLess>')
    return run(root, source, suite='mesh_schema', arguments=[],
               extra_flags=[str(root / 'src/core/DDataUtility/Data/DPGroupData.cpp')])


def mutations(root):
    source = bodies(root)
    execute(root, source)
    schemas(root)
    cases = (
        ('copy component admission omitted',
         '!dommy::mesh::validateRepeatedMeshMetadata(rhs, 1U, plan)', 'false'),
        ('append component admission omitted',
         '!dommy::mesh::addMeshMetadataCost(meshes.begin(), meshes.end(), budget)', 'false'),
        ('merge component admission omitted',
         '!dommy::mesh::addMeshMetadataCost(data.beginMeshes(), data.endMeshes(), budget)', 'false'),
        ('merge metadata admission omitted',
         '!dommy::attribute::validateAttributeMerge(\n', 'false && !dommy::attribute::validateAttributeMerge(\n'),
        ('merge primitive-group admission omitted',
         'if (data.mGroupList.find(group.first) != data.mGroupList.end() &&',
         'if (false && data.mGroupList.find(group.first) != data.mGroupList.end() &&'),
        ('right empty input loses identity',
         'if (data.meshItemArray.empty()) return copyFrom(base);', ''),
        ('left empty input loses identity',
         'if (base.meshItemArray.empty()) return copyFrom(data);', ''),
        ('base primitive copied twice',
         'DMItem copied = mesh->copyNewMesh();',
         'DMItem redundant = mesh->copyNewMesh(); DMItem copied = mesh->copyNewMesh();'),
        ('bounds recomputed twice',
         'candidate.updateBBox();\n    meshItemArray.swap(candidate.meshItemArray);',
         'candidate.updateBBox(); candidate.updateBBox();\n    meshItemArray.swap(candidate.meshItemArray);'),
        ('component schemas not filtered',
         'filterComponents(*copied);', ''),
        ('UV mismatch not cleared',
         'if (removeUV) mesh.clearUVs();', ''),
        ('output aliases a target primitive',
         'DMItem copied = mesh->copyNewMesh();', 'DMItem copied = std::const_pointer_cast<DMeshInternalData>(mesh);'),
        ('node duplicates the base copy',
         'outDMeshes->copyMergedFrom(*inputDMData, *targetDMData)',
         '(outDMeshes->copyFrom(*inputDMData), outDMeshes->copyMergedFrom(*inputDMData, *targetDMData))'),
        ('node recomputes bounds',
         'MStatus outputStatus;', 'outDMeshes->updateBBox();\n    MStatus outputStatus;'),
    )
    for label, old, new in cases:
        assert old in source, label
        try:
            execute(root, source.replace(old, new, 1), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'mesh_merge_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--mutations' in sys.argv:
        mutations(root)
    else:
        execute(root, bodies(root))
        schemas(root)
