#!/usr/bin/env python3
"""Source-derived curve storage, batch commits and frozen initialization."""
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run
from check_motion_duplicator_safety import sdk_check

FOLDER = 'src/modules/curve/DCurve/Generate/'
NAMES = ('DCurveFromCurve', 'DCurveLinkGen', 'DCurveFromMesh', 'DCurveFromPfx', 'DCText')


def initializers(root):
    result = []
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        result += declarations(name, source, True)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n'.join(result)


def plans(root):
    result = []
    for path in ('src/core/common/utility/DCurveGenerationStorage.h',
                 FOLDER + 'DCurveFromCurvePlan.h', FOLDER + 'DCurveSourceGenerationPlan.h',
                 FOLDER + 'DTextGenerationPlan.h'):
        result.append(extract((root / path).read_text(), 'namespace dommy {'))
    pfx = (root / (FOLDER + 'DCurveFromPfx.cpp')).read_text()
    for signature in ('struct PfxExportOptions {', 'struct PfxGenerationBudget {'):
        result.append(extract(pfx, signature) + ';')
    result += [extract(pfx, 'MStatus reserveLineBudget('), extract(pfx, 'MStatus preflightLines(')]
    start = pfx.index('PfxGenerationBudget plan;')
    end = pfx.index('MObject outputObject;', start)
    result += ['MStatus pfxAdmission(const MRenderLineArray& mainLines, const MRenderLineArray& leafLines, '
               'const MRenderLineArray& flowerLines, bool exportMainLines, bool exportLeafLines, '
               'bool exportFlowerLines, const PfxExportOptions& options, PfxGenerationBudget& result) {' +
               pfx[start:end] + 'result=plan;return MS::kSuccess;}']
    # Extract, rather than imitate, the changed input boundaries.
    link = (root / (FOLDER + 'DCurveLinkGen.cpp')).read_text()
    start = link.index('MDataHandle control = data.inputValue(aCount, &stat);')
    end = link.index('if (degree != 0', start)
    result += ['MStatus linkControls(MDataBlock& data, int& rawCurveCountOut, int& rawSegmentCountOut, short& degreeOut) {'
               'MStatus stat;const MObject aCount{0},aSegment{1},aDegree{2};' + link[start:end] +
               'rawCurveCountOut=rawCurveCount;rawSegmentCountOut=rawSegmentCount;degreeOut=degree;return MS::kSuccess;}']
    start = link.index('const std::size_t linkCount = arrayHandle.elementCount(')
    end = link.index('if(linkCount<2)', start)
    result += ['MStatus linkCount(MArrayDataHandle& arrayHandle, std::size_t& result) {MStatus stat;' +
               link[start:end] + 'result=linkCount;return MS::kSuccess;}']
    start = link.index('const double totalLength =')
    end = link.index('const double finLength =', start)
    result += ['MStatus linkLength(MFnNurbsCurve& crvFn, double& result) {MStatus stat;' +
               link[start:end] + 'result=totalLength;return MS::kSuccess;}']
    # API/native work is tested separately; assert preflight placement in every caller.
    mesh = (root / (FOLDER + 'DCurveFromMesh.cpp')).read_text()
    curve = (root / (FOLDER + 'DCurveFromCurve.cpp')).read_text()
    text = (root / (FOLDER + 'DCText.cpp')).read_text()
    start = text.index('MStatus status;', text.index('MStatus DCText::compute('))
    end = text.index('MFnPluginData pluginData;', start)
    result += ['MStatus textControls(MDataBlock& data, dommy::curve::TextGenerationPlan& result) {'
               'const MObject aText{0},aFontName{1},aSize{2},aSampling{3},aTracking{4},aGroupCharPiece{5},aGroupName{6};' +
               text[start:end] + '(void)createCharacterGroups;result=plan;return MS::kSuccess;}']
    assert mesh.index('checkedMeshCurveStorage(') < mesh.index('mesh.getPoints(')
    assert link.index('checkedLinkGenerationPlan(') < link.index('std::vector<MPointArray> pointArray')
    assert curve.index('makeCurveFromCurvePlan(') < curve.index('generateMCurveFromDcurve(')
    assert pfx.index('if (exportFlowerLines) DOMMY_RETURN_IF_FAILURE(preflightLines(') < pfx.index('generatedCurves.reserve(')
    assert text.index('admitTextCurvePoints(') < text.index('points.push_back(')
    return '\n'.join(result)


def generation(root):
    text = (root / (FOLDER + 'DCText.cpp')).read_text()
    curve = (root / (FOLDER + 'DCurveFromCurve.cpp')).read_text()
    result = [extract(text, 'namespace {')]
    start = curve.index('if (!output->insert(generatedCurves))')
    end = curve.index('MDataHandle outputHandle =', start)
    result += ['MStatus commitFromCurve(DCurveData* output, DCurveData::DCurveList& generatedCurves, '
               'const dommy::curve::CurveFromCurvePlan& plan) {' + curve[start:end] + 'return MS::kSuccess;}']
    assert curve.count('output->insert(') == 1 and 'generatedCurves.push_back(result);' in curve
    assert text.count('curves.insert(') == 1 and 'generatedCurves.push_back(curve);' in text
    assert text.index('commitTextCurve(') < text.index('DCItem curve(new DCurveInternalData(')
    return '\n'.join(result)


def mutations(root):
    initial, plan, generated = initializers(root), plans(root), generation(root)
    cases = [
        ('initializer status', 'curve_source_init', initial, 'DOMMY_RETURN_IF_FAILURE(stat);', '(void)stat;', None),
        ('initializer setter', 'curve_source_init', initial, 'DOMMY_RETURN_IF_FAILURE(messageAttribute.setReadable(false));',
         '(void)messageAttribute.setReadable(false);', None),
        ('initializer exception', 'curve_source_init', initial, 'return dommy::maya::reportEntryPointException(',
         'throw; return dommy::maya::reportEntryPointException(', None),
    ]
    for label, old, new in (
        ('curve payload', 'totalPoints, 96U', 'totalPoints, 0U'),
        ('curve objects', 'curves, 2048U', 'curves, 0U'),
        ('matrix/construction scratch', 'pointsPerCurve, 512U', 'pointsPerCurve, 0U'),
        ('native source payload', 'sourcePoints, 256U', 'sourcePoints, 0U'),
        ('source objects', 'sourceCvCounts.size(), 2112U', 'sourceCvCounts.size(), 0U'),
        ('Link retained points', 'sampledPoints, 64U', 'sampledPoints, 0U'),
        ('Link random arrays', 'candidate.curveCount, 64U', 'candidate.curveCount, 0U'),
        ('Link object arrays', 'links, 128U', 'links, 0U'),
        ('mesh source points', 'points, 64U', 'points, 0U'),
        ('Pfx double metadata', 'doubles * 32U', 'doubles * 0U'),
        ('Pfx vector metadata', 'vectors * 48U', 'vectors * 0U'),
        ('Pfx attribute objects', 'options.enabledAttributeCount(), 1024U', 'options.enabledAttributeCount(), 0U'),
        ('Pfx selected leaves omitted', 'if (exportLeafLines) DOMMY_RETURN_IF_FAILURE(preflightLines(leafLines, options, plan));',
         '(void)exportLeafLines;(void)leafLines;'),
        ('Pfx selected flowers omitted', 'if (exportFlowerLines) DOMMY_RETURN_IF_FAILURE(preflightLines(flowerLines, options, plan));',
         '(void)exportFlowerLines;(void)flowerLines;'),
        ('text decode storage', 'textBytes, 4U', 'textBytes, 0U'),
        ('text character arrays', 'createGroups ? 96U : 32U', 'createGroups ? 0U : 32U'),
        ('text mask storage', 'masks, 4U', 'masks, 0U'),
        ('text group objects', '512U + 4U *', '0U + 4U *'),
        ('text group names', '4U * (budget.groupNameBytes + 10U)', '0U * (budget.groupNameBytes + 10U)'),
        ('text membership indices', 'budget.createGroups ? curves : 0U, 32U', 'budget.createGroups ? curves : 0U, 0U'),
        ('Link failed input handle', 'if (!stat) return stat;', '(void)stat;'),
        ('Link/Pfx failed API status', 'DOMMY_RETURN_IF_FAILURE(stat);', '(void)stat;'),
        ('Text failed input handle', 'CHECK_MSTATUS_AND_RETURN_IT(status);', '(void)status;'),
        ('Text borrowed tracking', 'const float tracking[2] = {trackingValues[0], trackingValues[1]};',
         'const float* tracking = trackingValues;'),
    ):
        cases.append((label, 'curve_source_plans', plan, old, new, []))
    for label, old, new in (
        ('font path status','DOMMY_RETURN_IF_FAILURE(fileObject.setRawFullName(requestedPath.c_str()));','(void)fileObject.setRawFullName(requestedPath.c_str());'),
        ('font resolved path','const std::string candidate = fileObject.resolvedFullName().asChar();','const std::string candidate = requestedPath;'),
        ('text incremental admission', '!dommy::curve::admitTextCurvePoints(budget, candidatePointCount, bytes)',
         '(static_cast<void>(budget), false)'),
        ('Bezier block admission', '!dommy::curve::admitTextCurvePoints(budget, candidateLocalCount, bytes)',
         '(static_cast<void>(budget), false)'),
        ('text batch failure', 'if (!curves.insert(generatedCurves)) return MS::kFailure;', '(void)curves.insert(generatedCurves);'),
        ('source batch failure', 'if (!output->insert(generatedCurves)) return MS::kFailure;', '(void)output->insert(generatedCurves);'),
        ('text group remapping', 'characterCurveIds.push_back(generatedCurves.size() - 1U);', 'characterCurveIds.push_back(0U);'),
        ('text scale', '(*point)[0] *= 0.001f;', '(*point)[0] *= 1.0f;'),
    ):
        cases.append((label, 'curve_source_generation', generated, old, new, []))
    for label, suite, body, old, new, arguments in cases:
        assert old in body, label
        try:
            run(root, body.replace(old, new), capture=True, suite=suite, arguments=arguments)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == suite + '_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--emit' in sys.argv:
        result = run(root, initializers(root), capture=True, suite='curve_source_init', arguments=['--emit'])
        print(result.stdout, end='')
    elif '--mutations' in sys.argv:
        mutations(root)
    elif '--sdk' in sys.argv:
        for name in NAMES:
            includes = ()
            if name == 'DCText':
                if '--freetype' not in sys.argv:
                    print('DCText: skipped; supply --freetype with the FreeType include directory')
                    continue
                includes = (sys.argv[sys.argv.index('--freetype') + 1],)
            sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1], FOLDER + name + '.cpp', name, includes)
    else:
        run(root, initializers(root), suite='curve_source_init')
        run(root, plans(root), suite='curve_source_plans', arguments=[])
        run(root, generation(root), suite='curve_source_generation', arguments=[])
