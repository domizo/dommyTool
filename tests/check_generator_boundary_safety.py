#!/usr/bin/env python3
"""Active Generator status/numeric boundaries, frozen schemas and storage admission.

Extracts production code; native DG evaluation and geometry are not emulated.
"""
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run
from check_motion_duplicator_safety import sdk_check

SOURCES = (
    'src/modules/generators/Generate/Human/DHEyelashGen.cpp',
    'src/modules/generators/Generate/Human/DHEyeGen.cpp',
    'src/modules/generators/Generate/DCurveDuplicator.cpp',
    'src/modules/generators/Generate/DBubbleCrystal.cpp',
    'src/modules/generators/Generate/LeafGen/LeafGen.cpp',
    'src/modules/generators/icosahedron/icosahedronNode.cpp',
)
CUSTOM_EYELASH = 'src/modules/curve/DCurve/Generate/DCEyelashGen.cpp'


def initializer(root):
    source = (root / SOURCES[0]).read_text()
    return '\n'.join(declarations('DHEyelashGen', source, True) +
                     [extract(source, 'MStatus DHEyelashGen::initialize(')])


def registrations(root):
    result = []
    for path in SOURCES[1:]:
        source = (root / path).read_text()
        name = Path(path).stem
        result += declarations(name, source, True)
        result += [extract(source, 'MStatus ' + name + '::initialize(')]
    return '\n'.join(result)


def custom_initializer(root):
    source = (root / CUSTOM_EYELASH).read_text()
    return '\n'.join(declarations('DCEyelashGen', source, True) +
                     [extract(source, 'MStatus DCEyelashGen::initialize(')])


def inputs(root):
    functions = []
    for path in SOURCES + (CUSTOM_EYELASH,):
        source = (root / path).read_text()
        compute = extract(source, 'MStatus ' + Path(path).stem + '::compute(')
        compute = re.sub(r'//[^\n]*', '', compute)
        assert not re.search(r'inputValue\([^;]+\)\.as', compute), path
        pattern = (r'const MDataHandle (\w+Handle) = (data|block)\.inputValue\((\w+), &(stat|status)\);'
                   r'\s*DOMMY_RETURN_IF_FAILURE\(\4\);\s*const ')
        matches = list(re.finditer(pattern, compute))
        assert len(matches) == {'DHEyelashGen': 16, 'DHEyeGen': 14, 'DCurveDuplicator': 8,
                               'DBubbleCrystal': 11, 'LeafGen': 12, 'icosahedronNode': 2,
                               'DCEyelashGen': 19}[Path(path).stem]
        for match in matches:
            handle, block, attr, status = match.groups()
            end = compute.index(';', match.end()) + 1
            snippet = compute[match.start():end]
            tuple_read = 'Raw = ' in snippet
            if tuple_read:
                end = compute.index(';', compute.index(';', end) + 1) + 1
                snippet = compute[match.start():end]
            name = handle[:-6]
            checks = ('assert(' + name + '[0] == 1 && ' + name + '[1] == 2);'
                      'raw[0] = 99; assert(' + name + '[0] == 1);' if tuple_read else '')
            functions.append('MStatus probe' + str(len(functions)) + '() {MStatus ' + status +
                '; MDataBlock ' + block + '; MObject ' + attr + ';\n' + snippet + '\n' + checks +
                '\n++successes; return MS::kSuccess;}')
    functions.append('MStatus (*const probes[])() = {' +
                     ','.join('probe' + str(i) for i in range(len(functions))) + '};')
    return '\n'.join(functions)


def mesh_bodies(root):
    source = (root / 'src/modules/mesh/legacy/dupMesh.cpp').read_text()
    header = (root / 'src/modules/mesh/legacy/dupMesh.h').read_text()
    curve = (root / SOURCES[2]).read_text()
    compute = extract(curve, 'MStatus DCurveDuplicator::compute(')
    assert 'collectDuplicateMeshes(arrayHandle, countVal, dupShapeArray)' in compute
    assert 'inCrvPlug.isConnected(&stat)' in compute and 'tangentPlug.isConnected(&stat)' in compute
    # Only the unrelated DMeshData constructor declaration needs a test type.
    result = [extract(header, 'class dupMesh {') + ';',
              extract((root / 'src/core/common/utility/DMayaMeshImport.h').read_text(), 'namespace dommy {')]
    result += [extract(source, key) for key in ('MStatus dupMesh::preflight(',
               'bool dupMesh::matches(', 'dupMesh::dupMesh(MObject obj)')]
    result += [extract(curve, 'struct DuplicateMeshInput {') + ';',
               extract(curve, 'MStatus validateDuplicateBudget('),
               extract(curve, 'MStatus collectDuplicateMeshes(')]
    return '\n'.join(result)


def arrays(root, path=SOURCES[0]):
    source = (root / path).read_text()
    compute = extract(source, 'MStatus ' + Path(path).stem + '::compute(')
    compute = re.sub(r'//[^\n]*', '', compute)
    operations = re.findall(r'DOMMY_RETURN_IF_FAILURE\(([^;]+(?:append|setLength|copy|addRotationQuaternion|addTranslation|addRotation)\([^;]+)\);', compute)
    # No unchecked versions of the audited operations may reappear.
    for line in compute.splitlines():
        if re.search(r'(?:\.|->)(?:append|setLength|copy|addRotationQuaternion|addTranslation|addRotation)\(', line):
            if path == CUSTOM_EYELASH and line.strip() == 'if (!generatedCurves.append(curve)) {':
                continue  # bool-returning batch API, executed by eyelash_deformation below
            assert 'DOMMY_RETURN_IF_FAILURE' in line, line
    result = []
    for index, expression in enumerate(operations):
        result.append('MStatus arrayProbe' + str(index) + '() {\n'
                      'DOMMY_RETURN_IF_FAILURE(' + expression + '); ++successes; return MS::kSuccess;}')
    assert len(result) == 26, len(result)
    result.append('MStatus (*const arrayProbes[])() = {' +
                  ','.join('arrayProbe' + str(i) for i in range(len(result))) + '};')
    append_rotation = re.search(r'DOMMY_RETURN_IF_FAILURE\(baseRotMatArray.append\([^;]+;', compute)[0]
    append_transform = re.search(r'DOMMY_RETURN_IF_FAILURE\(transMatArray.append\([^;]+;', compute)[0]
    use = re.search(r'MPoint finPt = newPt\*baseRotMatArray\[i\]\*transMatArray\[i\]\+ basePt;', compute)[0]
    result.append('namespace matrixScenario {MStatus execute() {'
                  'MatrixList baseRotMatArray, transMatArray; Transform rotMat, transMat;'
                  'for (int i=0; i<2; ++i) {' + append_rotation + append_transform + '}'
                  'double newPt=1, basePt=0; for (int i=0; i<2; ++i) {' + use +
                  '(void)finPt;} return MS::kSuccess;}}')
    return '\n'.join(result)


def numerics(root):
    curve = (root / SOURCES[2]).read_text()
    bubble = (root / SOURCES[3]).read_text()
    values = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    math = (root / 'src/core/math/dommyMath.cpp').read_text()
    result = ['namespace dommy {namespace maya {',
              extract(values, 'inline bool isFiniteVector('),
              extract(values, 'inline bool isFloatRepresentableVector('), '}}',
              extract(curve, 'bool isFinitePoint('),
              extract(curve, 'bool transformCurveDuplicatePoint(')]
    compute = extract(curve, 'MStatus DCurveDuplicator::compute(')
    assert compute.index('transformCurveDuplicatePoint(newArray[p]') < compute.index('finVertArray.append(newArray[p])')
    prefix = compute[compute.index('double start, end;'):compute.index('MVector oldBiNormal;')]
    loop = compute[compute.index('MPoint cv;', compute.index('for (std::size_t c')):
                   compute.index('MVector\tnormal;', compute.index('for (std::size_t c'))]
    result += ['MStatus samplingProbe(std::size_t countVal, double curveLengthVal, double curveOffsetVal, '
               'bool useTangent=false, int tangentModeVal=0) {MStatus stat;\n' + prefix +
               '\nfor (std::size_t c=0; c<totalSampling; ++c) {\n' + loop +
               '} ++admissions; return MS::kSuccess;}']
    tangent = compute[compute.index('MPoint targetCV;'):compute.index('normal = targetCV-cv;')]
    result += ['MStatus tangentProbe(double tangentStart, double tangentEnd) {MStatus stat;MPoint cv;\n' +
               tangent + 'return MS::kSuccess;}']
    body = bubble[bubble.index('MFloatPointArray vertexArray;'):bubble.index('// 中心部分の面')]
    result += ['namespace dommy {namespace math {',
               re.search(r'const double kDegreesToRadians = [^;]+;', math)[0],
               extract(math, 'double stableDistance('),
               extract(math, 'double degreesToRadians('),
               extract(math, 'double distance(const MVector&'), '}}']
    controls = ('sideVal', 'divideVal', 'midDiv', 'radiusVal', 'seedVal', 'arcNoiseFrequencyVal',
                'heightNoiseFrequencyVal', 'arcNoiseAmplitudeVal', 'heightNoiseAmplitudeVal',
                'heightNoisePowerVal', 'growthVal')
    result += ['MStatus bubbleProbe(const BubbleSettings& settings) {' +
               ''.join('const auto ' + value + '=settings.' + value + ';' for value in controls) +
               'const std::size_t totalSideCount=sideVal*(midDiv+1);\n' + body +
               'bubbleOutput.swap(vertexArray);return MS::kSuccess;}']
    budget = (root / 'src/core/common/utility/DEyelashGeneration.h').read_text()
    result += [extract(budget, 'namespace dommy {')]
    for path, allocation in ((SOURCES[0], 'MDoubleArray rootBendRatioArray;'),
        (CUSTOM_EYELASH, 'MDoubleArray rootBendRatioArray;')):
        source = (root/path).read_text()
        assert source.index('buildEyelashGenerationPlan(') < source.index(allocation)
    return '\n'.join(result)


def eyelash_controls(root):
    budget = (root / 'src/core/common/utility/DEyelashGeneration.h').read_text()
    result = [extract(budget, 'namespace dommy {')]
    for path, kind in ((SOURCES[0], 'native'), (CUSTOM_EYELASH, 'custom')):
        source = re.sub(r'//[^\n]*', '', (root / path).read_text())
        compute = extract(source, 'MStatus ' + Path(path).stem + '::compute(')
        prefix = compute[compute.index('double start,end;'):compute.index('double rootBendStep')]
        loop_start = compute.index('for (std::size_t i = 0U; i < generationPlan.curveCount; ++i)')
        loop = compute[compute.index('double param;', loop_start):compute.index('const double globaPos', loop_start)]
        query = re.search(r'DOMMY_RETURN_IF_FAILURE\(\s*crvFn.getPointAtParam\([^;]+;', compute)[0]
        if kind == 'custom':
            query += re.search(r'DOMMY_RETURN_IF_FAILURE\(\s*tanCrvFn.getPointAtParam\([^;]+;', compute)[0]
        result.append('MStatus ' + kind + 'Sampling(int lashCount, double offset, double noise) {(void)offset;'
                      'MStatus stat; dommy::generation::EyelashGenerationPlan generationPlan;'
                      'generationPlan.curveCount=lashCount; MDoubleArray rndPosArray(lashCount,noise);' +
                      prefix + 'for(std::size_t i=0;i<generationPlan.curveCount;++i) {' +
                      loop + 'MPoint rootPt,tPt;' + query + '} ++admissions;return MS::kSuccess;}')
        # Every ramp binding is tested with the exact declaration and status check.
        ramps = re.findall(r'MRampAttribute (\w+)\([^;]+;\s*DOMMY_RETURN_IF_FAILURE\(stat\);', compute)
        all_ramps = re.findall(r'MRampAttribute (\w+)\([^;]+;', compute)
        assert ramps == all_ramps and len(ramps) == 10
        declarations_text = []
        for name in ramps:
            snippet = re.search(r'MRampAttribute ' + name + r'\([^;]+;\s*DOMMY_RETURN_IF_FAILURE\(stat\);', compute)[0]
            plugs = set(re.findall(r'\b\w+RampPlug\b', snippet))
            attrs = set(re.findall(r'\ba\w+Ramp\b', snippet))
            declarations_text += ['{MPlug ' + ','.join(sorted(plugs)) + ';' if plugs else '{']
            declarations_text += ['MObject ' + ','.join(sorted(attrs)) + ';'] if attrs else []
            declarations_text += [snippet + '}']
        result.append('MStatus ' + kind + 'Ramps(){MStatus stat;' +
                      ''.join(declarations_text) + '++admissions; return MS::kSuccess;}')
    source = (root / CUSTOM_EYELASH).read_text()
    assert not re.search(r'\bseed\s*\+\s*[1-9]', re.sub(r'//[^\n]*', '', source))
    assert 'if (seed < 0 ||' in source
    body = source[source.index('MDoubleArray lenRandValueArray;'):source.index('//            DVec3DataList tangentList')]
    result.append('MStatus randomProbe(int seed) try {'
                  'dommy::generation::EyelashGenerationPlan generationPlan;'
                  'generationPlan.curveCount=5; const int lashCount=5;'
                  'const double lenRand[]={-1.0,2.0},randTransMin[]={-2,-3,-4},randTransMax[]={2,3,4};'
                  'const double randRotMin[]={-5,-6,-7},randRotMax[]={5,6,7}; const double randPos=0.5;'
                  + body + 'randomOutput={lenRandValueArray,rndPosArray,randTransX,randTransY,randTransZ,'
                  'randRotX,randRotY,randRotZ};++admissions;return MS::kSuccess;'
                  '}catch(const std::invalid_argument&){return MS::kInvalidParameter;}')
    return '\n'.join(result)


def eyelash_deformation(root):
    values = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    math = (root / 'src/core/math/dommyMath.cpp').read_text()
    deform = (root / 'src/core/common/object/DDeformMath.cpp').read_text()
    result = [extract((root / 'src/core/common/utility/DEyelashGeneration.h').read_text(),
                      'namespace dommy {'),
              'namespace dommy {namespace maya {',
              extract(values, 'inline bool isFinitePoint('),
              extract(values, 'inline bool isFiniteMatrix('), '}}',
              'namespace dommy {namespace math {',
              re.search(r'const double kDegreesToRadians = [^;]+;', math)[0],
              extract(math, 'double degreesToRadians('), '}}',
              extract(deform, 'bool deformBendWithCvs(')]
    for path, kind in ((SOURCES[0], 'native'), (CUSTOM_EYELASH, 'custom')):
        compute = extract((root / path).read_text(),
                          'MStatus ' + Path(path).stem + '::compute(')
        mappings = re.findall(r'if \(!dommy::(?:generation::remapEyelashBendWeight|numeric::checkedFloatCast)'
                              r'\([^;]+;\s*', compute)
        assert len(mappings) == 6, (kind, len(mappings))
        begin = compute.index('MTransformationMatrix rotMat;')
        end = compute.index('DOMMY_RETURN_IF_FAILURE(baseRotMatArray.append', begin)
        result.append('MStatus ' + kind + 'Weights(const std::array<float,3>& bends, '
                      'const std::array<float,3>& rotations, const std::array<double,3>& rootRot, '
                      'const std::array<double,3>& random) {'
                      'const unsigned i=0; float globalBendVal=bends[0],rootBendVal=bends[1],tipBendVal=bends[2];'
                      'float rotX=rotations[0],rotY=rotations[1],rotZ=rotations[2];'
                      'double globalBendRatioArray[1],rootBendPerWeightArray[1],tipBendPerWeightArray[1];'
                      'const double randRotX[]={random[0]},randRotY[]={random[1]},randRotZ[]={random[2]};' +
                      ''.join(mappings) + compute[begin:end] +
                      'mappedBends={{globalBendRatioArray[0],rootBendPerWeightArray[0],tipBendPerWeightArray[0]}};'
                      '++admissions;return MS::kSuccess;}')
    source = (root / CUSTOM_EYELASH).read_text()
    assert 'outDCrvs->insert(' not in source
    declaration = 'DCurveOutputBatch generatedCurves;'
    publication = 'if (!generatedCurves.publishTo(*outDCrvs)) return MS::kFailure;'
    assert source.count(declaration) == source.count(publication) == 1
    assert source.index(declaration) < source.index('const DCItem curve(new DCurveInternalData(')
    assert source.index(publication) < source.index('MDataHandle output =')
    assert 'DOMMY_COMPUTE_CATCH("DCEyelashGen")' in source
    begin = source.index('const DCItem curve(new DCurveInternalData(')
    insertion = source[begin:source.index('\n            }', begin)]
    result += [extract((root / 'src/modules/curve/DCurve/DCurveUtil.h').read_text(),
                       'class DCurveOutputBatch final') + ';',
               'MStatus customBatch(const std::vector<MPointArray>& samples, DCurveData* outDCrvs) try {' +
               declaration + ' for(const MPointArray& sample:samples) {'
               'const MPointArray* cvs=&sample; const unsigned deg=3;'
               'MDoubleArray knotValues(sample.length()+deg-1,0.0); const MDoubleArray* knots=&knotValues;' +
               insertion + '}' + publication + '++publications;return MS::kSuccess;'
               '}catch(...){return MS::kFailure;}']
    return '\n'.join(result)


def mutations(root):
    cases = [
        ('bend staging copy unchecked', 'eyelash_deformation', eyelash_deformation(root),
         'if (dommy::maya::copyCheckedArray(cvs, result) != MS::kSuccess)',
         'result = cvs; if (false)', []),
        ('bend final copy unchecked', 'eyelash_deformation', eyelash_deformation(root),
         'return dommy::maya::copyCheckedArray(result, cvs) == MS::kSuccess;',
         'cvs = result; return true;', []),
        ('bend float overflow unchecked', 'eyelash_deformation', eyelash_deformation(root),
         'if (!numeric::checkedFloatCast(static_cast<double>(value) * 2.0, doubled))',
         'doubled=value*2; if (false)', []),
        ('rotation remap narrowing unchecked', 'eyelash_deformation', eyelash_deformation(root),
         'if (!dommy::numeric::checkedFloatCast((rotX*2.0)-1.0, rotX))',
         'rotX=(rotX*2.0)-1.0; if (false)', []),
        ('derived rotation unchecked', 'eyelash_deformation', eyelash_deformation(root),
         'if (!std::isfinite(baseRot[0]) || !std::isfinite(baseRot[1]) ||\n'
         '                !std::isfinite(baseRot[2])) return MS::kInvalidParameter;', '', []),
        ('eyelash batch append ignored', 'eyelash_deformation', eyelash_deformation(root),
         'if (!generatedCurves.append(curve))', '(void)generatedCurves.append(curve); if (false)', []),
        ('eyelash batch publication ignored', 'eyelash_deformation', eyelash_deformation(root),
         'if (!generatedCurves.publishTo(*outDCrvs)) return MS::kFailure;',
         '(void)generatedCurves.publishTo(*outDCrvs);', []),
        ('eyelash work limit ignored', 'generator_numerics', numerics(root),
         'kMaxWorkItemCount, candidate.bendWorkItems)',
         'std::numeric_limits<std::size_t>::max(), candidate.bendWorkItems)', []),
        ('custom input acquisition failure', 'generator_inputs', inputs(root),
         'data.inputValue(aOffset, &stat);\n        DOMMY_RETURN_IF_FAILURE(stat);',
         'data.inputValue(aOffset, &stat);', []),
        ('custom matrix append failure', 'generator_arrays', arrays(root, CUSTOM_EYELASH),
         'DOMMY_RETURN_IF_FAILURE(transMatArray.append(transMat.asMatrix()));',
         '(void)transMatArray.append(transMat.asMatrix());', []),
        ('custom CV copy failure', 'generator_arrays', arrays(root, CUSTOM_EYELASH),
         'DOMMY_RETURN_IF_FAILURE(cvs->copy(basdeCvs));', '(void)cvs->copy(basdeCvs);', []),
        ('custom initializer status failure', 'custom_eyelash_init', custom_initializer(root),
         'DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 20 ));', '(void)nAttr.setDefault( 20 );', None),
        ('custom initializer exception', 'custom_eyelash_init', custom_initializer(root),
         'return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', None),
        ('signed seed overflow', 'eyelash_controls', eyelash_controls(root),
         'dommy::random::requireOffsetNonnegativeSeed(seed, 8U)', 'seed + 8', []),
        ('historical random stream changed', 'eyelash_controls', eyelash_controls(root),
         'dommy::random::requireOffsetNonnegativeSeed(seed, 8U)',
         'dommy::random::requireOffsetNonnegativeSeed(seed, 7U)', []),
        ('reflected parameter unchecked', 'eyelash_controls', eyelash_controls(root),
         'if (!std::isfinite(candidate) || candidate < start || candidate > end) return false;', '', []),
        ('native curve parameter unchecked', 'eyelash_controls', eyelash_controls(root),
         'if (!std::isfinite(curveParam) || curveParam < start || curveParam > end)\n                return MS::kInvalidParameter;', '', []),
        ('tangent domain ignored', 'eyelash_controls', eyelash_controls(root),
         'param < tangentStart || param > tangentEnd', 'false', []),
        ('registration setter failure', 'generator_registration', registrations(root),
         'DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 9 ));', '(void)nAttr.setDefault( 9 );', None),
        ('registration exception boundary', 'generator_registration', registrations(root),
         'return dommy::maya::reportEntryPointException(', 'throw; return dommy::maya::reportEntryPointException(', None),
        ('translation narrowing omitted', 'generator_numerics', numerics(root),
         'if (!dommy::numeric::checkedFloatCast(candidate.x + (position.x + offset[0]), candidate.x) ||',
         'candidate.x += position.x + offset[0]; if (false ||', []),
        ('finite height narrowed unchecked', 'generator_numerics', numerics(root),
         'if (!dommy::numeric::checkedFloatCast(height, finHeight)) {',
         'finHeight = static_cast<float>(height); if (!std::isfinite(height)) {', []),
        ('length overflow reaches host', 'generator_numerics', numerics(root)
         .replace('if (!std::isfinite(stepLength)) return MS::kInvalidParameter;', '')
         .replace('if (!std::isfinite(sampleDistance) || sampleDistance < 0.0) return MS::kInvalidParameter;', ''),
         'if (!std::isfinite(length)) return MS::kInvalidParameter;', '', []),
        ('invalid returned parameter', 'generator_numerics', numerics(root),
         'if (curveParam < start || curveParam > end) return MS::kInvalidParameter;', '', []),
        ('curve overhead not budgeted', 'generator_numerics', numerics(root),
         '{2048U, 96U, 512U}', '{0U, 96U, 512U}', []),
        ('retained payload not budgeted', 'generator_numerics', numerics(root),
         '{2048U, 96U, 512U}', '{2048U, 0U, 512U}', []),
        ('construction workspace not budgeted', 'generator_numerics', numerics(root),
         '{2048U, 96U, 512U}', '{2048U, 96U, 0U}', []),
        ('failed input read', 'generator_inputs', inputs(root),
         'DOMMY_RETURN_IF_FAILURE(stat);', '', []),
        ('matrix append failure', 'generator_arrays', arrays(root),
         'DOMMY_RETURN_IF_FAILURE(baseRotMatArray.append(rotMat.asMatrix()));',
         '(void)baseRotMatArray.append(rotMat.asMatrix());', []),
        ('initializer failure', 'generator_init', initializer(root),
         'DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));',
         '(void)gAttr.setStorable(false);', None),
        ('import preflight omitted', 'generator_mesh', mesh_bodies(root),
         'if (preflight(obj, plan) != MS::kSuccess) return;',
         '(void)preflight(obj, plan);', []),
        ('aggregate budget omitted', 'generator_mesh', mesh_bodies(root),
         'DOMMY_RETURN_IF_FAILURE(validateDuplicateBudget(inputs, copies, count));',
         '(void)validateDuplicateBudget(inputs, copies, count);', []),
        ('UV cardinality ignored', 'generator_mesh', mesh_bodies(root),
         'if (uvIds != storage_->uvIdsArray.length()) return;', '', []),
        ('normal workspace omitted', 'generator_mesh', mesh_bodies(root),
         'checkedProduct(candidate.normals, 16U,', 'checkedProduct(candidate.normals, 0U,', []),
        ('edge workspace omitted', 'generator_mesh', mesh_bodies(root),
         'checkedProduct(candidate.edges, sizeof(int),', 'checkedProduct(candidate.edges, 0U,', []),
    ]
    for label, suite, body, old, new, arguments in cases:
        assert old in body, label
        try:
            run(root, body.replace(old, new), suite=suite, arguments=arguments, capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == suite + '_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        sdk = sys.argv[sys.argv.index('--sdk') + 1]
        for source in SOURCES + ('src/modules/mesh/legacy/dupMesh.cpp', CUSTOM_EYELASH,
                                 'src/core/common/object/DDeformMath.cpp'):
            sdk_check(root, sdk, source, Path(source).stem)
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, initializer(root), suite='generator_init')
        run(root, custom_initializer(root), suite='custom_eyelash_init')
        run(root, registrations(root), suite='generator_registration')
        run(root, numerics(root), suite='generator_numerics', arguments=[])
        run(root, eyelash_controls(root), suite='eyelash_controls', arguments=[])
        run(root, eyelash_deformation(root), suite='eyelash_deformation', arguments=[])
        run(root, inputs(root), suite='generator_inputs', arguments=[])
        run(root, arrays(root), suite='generator_arrays', arguments=[])
        run(root, arrays(root, CUSTOM_EYELASH), suite='generator_arrays', arguments=[])
        run(root, mesh_bodies(root), suite='generator_mesh', arguments=[])
