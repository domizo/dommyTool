#!/usr/bin/env python3
"""Run complete repeated-copy transaction and node with real typed metadata.

Native Maya transform/geometry evaluation is an explicit transport boundary.
Small test-only budgets exercise output admission without large allocations.
"""
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from check_mesh_merge_safety import bodies as merge_bodies
from check_geometry_state_transactions import extract
from check_curve_path_safety import run


def bodies(root):
    data = (root / 'src/modules/mesh/DMesh/Data/DMeshData.cpp').read_text()
    node = (root / 'src/modules/mesh/DMesh/Utility/DMCopyTransform.cpp').read_text()
    attributes = ['inDM', 'outDM', 'aCount', 'aTranslate', 'aRotate', 'aScale',
                  'aPivot', 'aUniformScale', 'aFreezeTransform', 'aRotateOrder']
    return '\n\n'.join([
        merge_bodies(root), extract(data, 'MStatus DMeshData::copyRepeatedFrom('),
        'struct DMCopyTransform {MStatus compute(const MPlug&, MDataBlock&); ' +
        ''.join('static MObject ' + a + ';' for a in attributes) + '};',
        '\n'.join('MObject DMCopyTransform::' + a + '{"' +
                  ('input' if a == 'inDM' else 'output' if a == 'outDM' else a) + '"};'
                  for a in attributes),
        extract(node, 'MStatus DMCopyTransform::compute('), 'DOMMY_COMPUTE_CATCH("DMCopyTransform")'])


def execute(root, source, capture=False, budget_header=None, extra_headers=None):
    limits = (root / 'src/core/common/utility/DGenerationLimits.h').read_text()
    limits = limits.replace('kMaxGeneratedStringByteCount = 100000000U',
                            'kMaxGeneratedStringByteCount = 128U')
    limits = limits.replace('kMaxWorkItemCount = 100000000U', 'kMaxWorkItemCount = 512U')
    overrides = dict(extra_headers or {})
    overrides['common/utility/DGenerationLimits.h'] = limits
    if budget_header is not None:
        overrides['common/utility/DMeshCopyBudget.h'] = budget_header
    return run(root, source, suite='mesh_copy', arguments=[], capture=capture,
               header_overrides=overrides,
               extra_flags=[str(root / 'src/core/DDataUtility/Data/DPGroupData.cpp'),
                            '-I' + str(root / 'src/core/common/utility')])


def mutations(root):
    source = bodies(root)
    execute(root, source)
    cases = (
        ('node metadata admission omitted',
         'if (!dommy::mesh::validateRepeatedMeshMetadata(*inputDMData, copies, outputPlan))',
         'if (false && !dommy::mesh::validateRepeatedMeshMetadata(*inputDMData, copies, outputPlan))'),
        ('builder metadata admission omitted',
         '!dommy::mesh::validateRepeatedMeshMetadata(source, copies, outputPlan))',
         'false)'),
        ('copy payload cloned twice',
         'DMItem copied = mesh->copyNewMesh();',
         'DMItem redundant = mesh->copyNewMesh(); DMItem copied = mesh->copyNewMesh();'),
        ('copy output aliases input',
         'DMItem copied = mesh->copyNewMesh();',
         'DMItem copied = std::const_pointer_cast<DMeshInternalData>(mesh);'),
        ('copy bounds updated twice',
         'candidate.updateBBox();\n    meshItemArray.swap(candidate.meshItemArray);',
         'candidate.updateBBox(); candidate.updateBBox();\n    meshItemArray.swap(candidate.meshItemArray);'),
        ('primitive attribute repetition omitted',
         'if (!appendOtherDPAttribute(field.second, source.mAttributeList.at(field.first)))',
         'if (false && !appendOtherDPAttribute(field.second, source.mAttributeList.at(field.first)))'),
        ('primitive group repetition omitted',
         'group.second->appendOtherGrp(source.mGroupList.at(group.first));', ';'),
        ('transform callback skipped',
         'if (copyIndex != 0U && operation)', 'if (false && copyIndex != 0U && operation)'),
        ('original batch transformed',
         'if (copyIndex != 0U && operation)', 'if (operation)'),
        ('bake failure ignored',
         'DOMMY_RETURN_IF_FAILURE(copied.bakeTransform(finMat));', '(void)copied.bakeTransform(finMat);'),
        ('callback output budget omitted',
         '!dommy::mesh::validateRepeatedMeshMetadata(candidate, 1U, finalPlan))', 'false)'),
    )
    # Target the repeated-copy method, not similar merge/copyFrom code included
    # for shared facade definitions.
    split = source.index('MStatus DMeshData::copyRepeatedFrom(')
    for label, old, new in cases:
        assert old in source[split:], label
        try:
            execute(root, source[:split] + source[split:].replace(old, new, 1), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'mesh_copy_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    header = (root / 'src/core/common/utility/DMeshCopyBudget.h').read_text()
    for label, old, new in (
            ('component metadata budget omitted', 'copies, kMaxGeneratedStringByteCount, budget.stringBytes)',
             '1U, kMaxGeneratedStringByteCount, budget.stringBytes)'),):
        assert old in header, label
        try:
            execute(root, source, capture=True, budget_header=header.replace(old, new, 1))
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'mesh_copy_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    path = 'common/utility/DPrimitiveCopyBudget.h'
    header = (root / 'src/core' / path).read_text()
    old = 'rows.stringBytes, copies - 1U'
    assert old in header
    try:
        execute(root, source, capture=True, extra_headers={path: header.replace(old, 'rows.stringBytes, 0U', 1)})
    except subprocess.CalledProcessError as error:
        assert Path(error.cmd[0]).name == 'mesh_copy_tests', 'Compile failure: primitive repeated strings omitted'
        print('Detected mutation: primitive repeated strings omitted', flush=True)
    else:
        raise AssertionError('Undetected mutation: primitive repeated strings omitted')


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--mutations' in sys.argv:
        mutations(root)
    else:
        execute(root, bodies(root))
