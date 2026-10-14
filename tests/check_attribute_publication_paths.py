#!/usr/bin/env python3
"""Keep registered legacy nodes on the runtime-tested attribute adapters."""

from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
from check_active_cpp_safety import strip_comments_and_literals


def function_bodies(source, pattern):
    code = strip_comments_and_literals(source)
    for match in re.finditer(pattern, code):
        start = code.find('{', match.end())
        if start < 0:
            raise ValueError('Missing function body')
        depth, end = 1, start + 1
        while end < len(code) and depth:
            depth += (code[end] == '{') - (code[end] == '}')
            end += 1
        if depth:
            raise ValueError('Unbalanced function body')
        yield match.group(1), code[start + 1:end - 1]


def main():
    root = Path(__file__).resolve().parent.parent
    errors = []
    pattern = r'\bMStatus\s+((?:DP|DC|DCP|DM|DMP)AddAttr(?:Scalar|Vec2|Vec3))\s*\('
    for relative, expected in (
        ('src/modules/point/DPoint/Common/DPointCommon.cpp', 3),
        ('src/modules/curve/DCurve/DCurveUtil.cpp', 6),
        ('src/modules/mesh/DMesh/common/DMeshUtil.cpp', 6),
    ):
        bodies = list(function_bodies((root / relative).read_text(), pattern))
        if len(bodies) != expected:
            errors.append(relative + ': missing checked publication entry points')
        for name, body in bodies:
            if not re.fullmatch(r'\s*return DPublishAttributeValues\([^;]+\);\s*', body):
                errors.append(name + ': must delegate directly to the tested adapter')
    for node, folder, count in (('DPFitRange', 'other', 15), ('DPRemap', 'other', 15),
                                ('DPNormalize', 'vector', 10)):
        path = root / ('src/modules/point/DPoint/math/' + folder + '/' + node + '.cpp')
        bodies = list(function_bodies(path.read_text(), r'\bMStatus\s+(' + node + r'::compute)\s*\('))
        if len(bodies) != 1:
            errors.append(node + ': missing compute body')
            continue
        code = bodies[0][1]
        for entry in ('DReadUnaryMathAttribute', 'DPublishAttributeValues'):
            calls = re.findall(r'\b' + entry + r'\s*\(', code)
            checked = re.findall(r'DOMMY_RETURN_IF_FAILURE\(\s*' + entry + r'\s*\(', code)
            if len(calls) != count or len(checked) != count:
                errors.append(node + ': every read/publication must propagate failure')
        if len(re.findall(r'if\s*\(!operation\(result\)\)\s*return MS::kInvalidParameter', code)) != count:
            errors.append(node + ': every operation must propagate failure')
        if 'return dommy::maya::mapMayaUnaryArray' not in code:
            errors.append(node + ': array output must use the checked shared mapper')
        if re.search(r'\b(DScalarDataList|DFloatArray|DVec2DataList|DVec3DataList|append|setLength|normal)\b', code):
            errors.append(node + ': legacy narrowing or unchecked array path returned')
    normalize = strip_comments_and_literals((root / 'src/modules/point/DPoint/math/vector/DPNormalize.cpp').read_text())
    if len(re.findall(r'DOMMY_RETURN_IF_FAILURE\(\s*DPublishAttributeValues\(', normalize)) != 10:
        errors.append('DPNormalize: all ten branches must use checked publication')
    if re.search(r'\b(removeAttr|addAttrWithValues|addDPAttributeValues|checkDPAttribute|checkDCurveAttribute|checkDMeshAttribute)\s*\(', normalize):
        errors.append('DPNormalize: unverified add/delete path returned')
    for node, operation in (
            ('DPVecLength', 'Length'), ('DPVecMinComponent', 'Minimum'),
            ('DPVecMaxComponent', 'Maximum'), ('DPVecAverageComponent', 'Average'),
            ('DPDotProduct', 'Dot'), ('DPVecAngle', 'Angle'), ('DPCrossProduct', 'Cross')):
        path = root / ('src/modules/point/DPoint/math/vector/' + node + '.cpp')
        bodies = list(function_bodies(path.read_text(), r'\bMStatus\s+(' + node + r'::compute)\s*\('))
        expected = (r'\s*return dommy::maya::computeVectorNode<' + node +
                    r',\s*dommy::math::VectorNodeKind::' + operation + r'>\(plug, data\);\s*')
        if len(bodies) != 1 or not re.fullmatch(expected, bodies[0][1]):
            errors.append(node + ': both outputs must delegate to the tested vector-node pipeline')
        code = strip_comments_and_literals(path.read_text())
        output = code[code.find('outDataArray = gAttr.create'):]
        expected_type = 'kVectorArray' if operation == 'Cross' else 'kDoubleArray'
        if 'gAttr.addDataAccept(MFnData::' + expected_type + ')' not in output:
            errors.append(node + ': declaration must accept its actual array output type')
    vector = strip_comments_and_literals((root / 'src/modules/point/DPoint/math/DMayaVectorNode.h').read_text())
    for entry in ('setPluginDataOutput<Data>', 'validatePluginData(*source)',
                  'readUnaryMathVectors<MFnVectorArrayData>', 'readUnaryMathVectors<MFnPointArrayData>',
                  'math::writeScalarMathArray<double>', 'math::writeVectorMathArray<MVector>',
                  'math::processVectorNodePartitions', 'math::processVectorNodeAttribute'):
        if entry not in vector:
            errors.append('Vector nodes: missing checked boundary ' + entry)
    preflight = vector.find('vectorNodeDataCount(*source, settings, count)')
    if preflight < 0 or preflight > vector.find('setPluginDataOutput<Data>'):
        errors.append('Vector nodes: math budget must be preflighted before copying procedural data')
    pipeline = strip_comments_and_literals((root / 'src/modules/point/DPoint/math/DVectorNodeMath.h').read_text())
    if 'attribute::writeNumericAttributeValues(' not in pipeline or re.search(
            r'\b(removeAttr|checkDCurvePointAttribute|checkDMeshPointAttribute|addAttrWithValues)\s*\(', pipeline):
        errors.append('Vector nodes: per-owner checked publication is required')
    array = strip_comments_and_literals((root / 'src/core/DDataUtility/Attribute/Convert/DPAttributeToDataArray.cpp').read_text())
    if 'convertAttributeToArray' not in array or re.search(r'\b(DScalarDataList|getDScalarArrayFromAttribute|append)\b', array):
        errors.append('DPAttributeToDataArray: legacy float/intermediate or unchecked append path returned')
    cast = strip_comments_and_literals((root / 'src/core/DDataUtility/Attribute/Edit/DPAttributeCast.cpp').read_text())
    for entry in ('validCastTarget(target)', 'validAttributeName(attributeName)',
                  'validatePluginData(*input)', 'validArrayConversionBudget(',
                  'setPluginDataOutput<DPData>', 'if (!status) return status;',
                  'if (!dommy::attribute::castPointAttribute(output, attributeName, target))'):
        if entry not in cast:
            errors.append('DPAttributeCast: missing checked boundary ' + entry)
    if cast.find('validArrayConversionBudget(') > cast.find('setPluginDataOutput<DPData>'):
        errors.append('DPAttributeCast: budget must be checked before copying point data')
    if re.search(r'\b(DScalarDataList|DVec2DataList|DVec3DataList|checkDPAttribute|removeAttr|addAttr|addAttrWithValues|DScalarVectorListToVectorList)\b', cast):
        errors.append('DPAttributeCast: legacy narrowing or delete-before-convert path returned')
    cast_helper = strip_comments_and_literals((root / 'src/core/common/utility/DAttributeCast.h').read_text())
    for entry in ('numeric::checkedValueCast(', 'convertTypedArray<Source>(', 'it->second.swap(candidate)'):
        if entry not in cast_helper:
            errors.append('Attribute cast: missing tested conversion/commit ' + entry)
    ray = strip_comments_and_literals((root / 'src/modules/point/DPoint/Utility/DPRay.cpp').read_text())
    if len(re.findall(r'if\s*\(!dommy::group::replacePointGroupPartitions\(', ray)) != 2:
        errors.append('DPRay: curve and mesh group writes must use the checked partition adapter')
    if 'hitIdsList.size() != 1U ||' not in ray or '!outputDPData->replaceGrp(rayGroupName, hitIdsList[0])' not in ray:
        errors.append('DPRay: point group write must validate the mask and propagate replacement failure')
    if 'data.inputValue(aRayGroupName, &stat)' not in ray or 'validAttributeName(rayGroupName)' not in ray:
        errors.append('DPRay: group name must be read and validated before processing')
    if re.search(r'\b(checkDPAttribute|checkDCurvePointAttribute|checkDMeshPointAttribute|removeAttr|addGrp)\s*\(', ray):
        errors.append('DPRay: group writes must not remove ordinary attributes or use insert-only addGrp')
    for owner, relative in (
        ('DPData', 'src/core/DDataUtility/Data/DPData.cpp'),
        ('DPrimBaseData', 'src/core/Data/DPrimBaseData.cpp'),
    ):
        bodies = list(function_bodies((root / relative).read_text(),
            r'\bbool\s+(' + owner + r'::replaceGrp)\s*\('))
        expected = r'\s*return dommy::group::replaceGroupValues\(mGrooupList, name, values, numberOfPoints\(\)\);\s*'
        if len(bodies) != 1 or not re.fullmatch(expected, bodies[0][1]):
            errors.append(owner + ': point-group replacement must delegate to the tested storage helper')
    for node, data_type, relative in (
        ('DPGroup', 'DPData', 'src/core/DDataUtility/Group/DPGroup.cpp'),
        ('DCGroup', 'DCurveData', 'src/modules/curve/DCurve/Group/DCGroup.cpp'),
        ('DMGroup', 'DMeshData', 'src/modules/mesh/DMesh/Data/DMGroup.cpp'),
    ):
        source = (root / relative).read_text()
        compute = list(function_bodies(source, r'\bMStatus\s+(' + node + r'::compute)\s*\('))[0][1]
        compact = re.sub(r'\s+', ' ', compute)
        compact = re.sub(r'<\s+', '<', compact)
        for entry in ('DOMMY_RETURN_IF_FAILURE(evalParms(data, params))',
                      'setPluginDataOutput<' + data_type + '>', 'publishCombinedGroup('):
            if entry not in compact:
                errors.append(node + ': missing checked group boundary ' + entry)
        if re.search(r'\b(removeGrp|removeCompGrp|addGrp|addCompGrp)\s*\(', compute):
            errors.append(node + ': insert-only or delete-before-combine group update returned')
        for match in re.finditer(r'\bgenerate(?:Numeric|Bound|Normal|AttributeNormal)Group\(', compute):
            if not re.search(r'DOMMY_RETURN_IF_FAILURE\(\s*$', compute[:match.start()]):
                errors.append(node + ': selection failure is not propagated')
        params = list(function_bodies(source, r'\bMStatus\s+(' + node + r'::evalParms)\s*\('))[0][1]
        if 'validateGroupParams(parms,' not in params:
            errors.append(node + ': missing parameter preflight')
        if re.search(r'data\.inputValue\([^,()]+\)', params):
            errors.append(node + ': unchecked input read returned')
        if node == 'DPGroup':
            for entry in ('expression.isValid()', 'dommy::expression::evaluateChecked(',
                          'dommy::group::evaluateSelection('):
                if entry not in compute:
                    errors.append(node + ': missing expression failure boundary ' + entry)
        if node == 'DCGroup':
            if 'curveDirectionAtClosestPoint(' not in compute or 'getParamAtPoint(' in compute:
                errors.append(node + ': curve direction must use the tested closest-point adapter')
        if node == 'DMGroup':
            for kind in ('FACE', 'EDGE'):
                if 'combineCompGrp(D_COMP_' + kind not in compact:
                    errors.append(node + ': missing checked component combination ' + kind)
            if 'if (params.isEdgeBoundry)' not in compute:
                errors.append(node + ': edge boundary condition must use its own enable flag')
            if re.search(r'getConnectedFaces\([^,()]+\)', compute):
                errors.append(node + ': connected-face query must report MStatus')
    common = strip_comments_and_literals((root / 'src/core/Data/DGroupCommon.cpp').read_text())
    for entry in ('dommy::group::selectPattern(', 'dommy::group::evaluateSelection(',
                  'dommy::group::normalSelected(', 'freeCachedIntersectionAccelerator('):
        if entry not in common:
            errors.append('Group selection: missing tested/checked boundary ' + entry)
    blend = strip_comments_and_literals((root / 'src/core/DDataUtility/Attribute/DPAttributeBlend.cpp').read_text())
    if 'dommy::attribute::publishBlendedAttribute(' not in blend or re.search(r'\bremoveAttr\s*\(', blend):
        errors.append('DPAttributeBlend: publication and deletion must use the checked shared adapter')
    if re.search(r'if\s*\(blended\)\s*addBlendedAttribute\(', blend):
        errors.append('DPAttributeBlend: publication result must update blended before success')
    helpers = strip_comments_and_literals((root / 'src/modules/point/DPoint/Common/DPointSeExpr.h').read_text())
    if re.search(r'\b(m_attributeName|m_groupName)\b', helpers):
        errors.append('DPointSeExpr: argument names must not be stored on shared function helpers')
    if helpers.count('dommy::expression::prepareNamedCall(') != 3 or helpers.count('dommy::expression::readCallName(') != 3:
        errors.append('DPointSeExpr: all three custom functions must use call-local argument adapters')
    if helpers.count('dommy::expression::createPointAttribute(') != 2:
        errors.append('DPointSeExpr: new scalar/vector fields must use indexed zero-filled staging')
    getter = helpers.split('class getDPAttrFuncHelper', 1)[1].split('class setDPAttrFuncHelper', 1)[0]
    if not re.search(r'checkedScalarCast\(\s*m_dpoint->attrListValueWithIndex<int64_t>\('
                     r'attributeName,\s*m_pid\),\s*value,\s*true\)', getter):
        errors.append('DPointSeExpr: int64 reads must require an exact double conversion')
    seexpr = strip_comments_and_literals((root / 'src/core/DDataUtility/Attribute/DPAttributeSeExpr.cpp').read_text())
    if 'dommy::expression::setUserVectors(expression, parameters.userVectors)' not in seexpr:
        errors.append('DPAttributeSeExpr: UserVector inputs must use the double binding adapter')
    if ('isFiniteVector(parameters.userVectors[index])' not in seexpr or
            re.search(r'isFloatRepresentableVector\(parameters\.userVectors', seexpr)):
        errors.append('DPAttributeSeExpr: double UserVector inputs must not have float range limits')
    for index in range(1, 5):
        signature = 'void setUserVector{}(const SeVec3d& p_input)'.format(index)
        assignment = 'userVec{}.val = p_input;'.format(index)
        if signature not in helpers or assignment not in helpers:
            errors.append('DPointSeExpr: UserVector{} must preserve double components'.format(index))
    for entry in ('dommy::expression::evaluateResults(', 'dommy::expression::publishResult(',
                  'validArrayConversionBudget(', 'setPluginDataOutput<DPData>',
                  'setPluginDataOutput<DCurveData>', 'setPluginDataOutput<DMeshData>'):
        if entry not in re.sub(r'\s+', '', seexpr):
            errors.append('DPAttributeSeExpr: missing checked expression boundary ' + entry)
    if re.search(r'\b(DVec3DataList|checkDPAttribute|checkDCurvePointAttribute|checkDMeshPointAttribute|convertDVec3DToScalar|removeAttr)\b', seexpr):
        errors.append('DPAttributeSeExpr: float result workspace or delete-before-evaluate path returned')
    sort = strip_comments_and_literals((root / 'src/modules/point/DPoint/Utility/DPSort.cpp').read_text())
    if 'dommy::attribute::sortAttributeIndices(' not in sort or 'dommy::attribute::sortValueIndices(' not in sort:
        errors.append('DPSort: use the typed sorting adapter for attributes and geometric keys')
    if re.search(r'\b(getSingleValue|getTwoValueComponent|getTripleValueComponent|sortValueWithPair)\s*\(', sort):
        errors.append('DPSort: lossy double-only attribute sorting returned')
    for relative in (
        'src/modules/point/DPoint/Utility/DPColor.cpp',
        'src/modules/mesh/DMesh/Utility/DMColor.cpp',
        'src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp',
        'src/core/DDataUtility/Particle/nobject/DPnParticle.cpp',
        'src/core/DDataUtility/Generate/DPointVolumeEmit.cpp',
        'src/core/DDataUtility/Attribute/DPAttributeSeExpr.cpp',
        'src/core/DDataUtility/Group/DPGroup.cpp',
    ):
        code = strip_comments_and_literals((root / relative).read_text())
        if re.search(r'\b(?:expr|expression)\.evaluate\(', code):
            errors.append(relative + ': raw expression evaluation bypasses the checked adapter')
        if 'dommy::expression::evaluateChecked(' not in code and 'dommy::expression::evaluateResults(' not in code:
            errors.append(relative + ': missing checked evaluation adapter')
        if 'dommy::maya::setExpressionTime(' not in code or re.search(r'\.(setFrame|setTime)\(', code):
            errors.append(relative + ': use explicit unit conversion through the shared time adapter')
        if relative.endswith(('DPColor.cpp', 'DMColor.cpp')):
            if re.search(r'\b(checkDPAttribute|checkDMeshPointAttribute|checkDMeshAttribute|removeAttr|addAttr|addAttrWithValues|addDPAttributeValues)\(', code):
                errors.append(relative + ': color fields must not be deleted early or written unchecked')
            for entry in ('dommy::color::publishValues(', 'dommy::expression::publishResult('):
                if entry not in code:
                    errors.append(relative + ': missing checked color publication ' + entry)
        if relative.endswith(('DMColor.cpp', 'DPMeshDuplicator.cpp', 'DPnParticle.cpp', 'DPointVolumeEmit.cpp')):
            if 'usesPointDataFunctions()' not in code:
                errors.append(relative + ': reject point-data functions when no point-data binding exists')
        if relative.endswith('DPMeshDuplicator.cpp') and 'dommy::expression::readScale(' not in code:
            errors.append(relative + ': scale locals must honor scalar/vector type')
        if relative.endswith('DPointVolumeEmit.cpp') and 'expression.setId(placedPoints.size())' not in code:
            errors.append(relative + ': packing candidates must receive a deterministic point ID')
    for relative in ('src/modules/point/DPoint/Utility/DPColor.cpp',
                     'src/modules/mesh/DMesh/Utility/DMColor.cpp',
                     'src/modules/curve/DCurve/Utility/DCColor.cpp'):
        code = strip_comments_and_literals((root / relative).read_text())
        if code.count('dommy::color::readPalette(') != 1 or 'plColArray.append(' in code:
            errors.append(relative + ': use the shared checked palette reader exactly once')
        if re.search(r'\b(checkDCurvePointAttribute|checkDCurveAttribute|removeAttr|addAttr|addAttrWithValues)\(', code):
            errors.append(relative + ': unchecked color publication or early deletion returned')
        if 'dommy::color::publishValues(' not in code or 'validAttributeName(colAttrName)' not in code:
            errors.append(relative + ': missing color publication/name validation')
        bounds = code.split('colorAddTypeVal==dommy::color::kBoundingBox', 1)[1].split('} else if', 1)[0]
        if not any(entry in bounds for entry in ('dommy::color::normalizeBoundingColor(',
                                                  'dommy::color::transformedBoundingColors(')):
            errors.append(relative + ': bounding colors must use the double checked adapter')
        if relative.endswith(('DMColor.cpp', 'DCColor.cpp')):
            for entry in ('dommy::color::transformedPointBounds(',
                          'dommy::color::transformedBoundingColors(', 'getTransformMatrix()'):
                if entry not in bounds:
                    errors.append(relative + ': missing same-space bounding color boundary ' + entry)
            if re.search(r'\b(getBoundingBox|boundingColors|getPointsInWorld)\(', bounds):
                errors.append(relative + ': mixed-space or float-world-point bounding colors returned')
        if re.search(r'\bfloat\s+\w+Size\b', bounds):
            errors.append(relative + ': float bounding extents returned')
        if relative.endswith('DCColor.cpp') and re.search(r'data\.inputValue\([^,()]+\)', code):
            errors.append(relative + ': unchecked input read returned')
    blend = strip_comments_and_literals((root / 'src/core/common/utility/DAttributeBlendMath.h').read_text())
    if 'return checkedIntegerBlend(source, target, weight, result)' not in blend:
        errors.append('Integer blends: must retain exact endpoints through the tested integer adapter')
    if 'static_cast<Value>' in blend:
        errors.append('Blends: unchecked final floating-to-integer cast returned')
    primitive = strip_comments_and_literals((root / 'src/core/Data/DPrimBaseData.cpp').read_text())
    for entry in ('!dommy::attribute::isFiniteMatrix(matrix)',
                  '!dommy::attribute::isFiniteMatrix(m_mat)',
                  '!dommy::numeric::appendTransformedBounds(points, matrix, bounds)',
                  '!dommy::numeric::checkedTransformPoints(pointArray, m_mat, result)',
                  '!dommy::numeric::checkedPointMean(pointArray, local)',
                  '!dommy::numeric::checkedTransformPoint(local, m_mat, transformed)',
                  '!dommy::numeric::checkedFloatCast(center[axis], values[axis])'):
        if entry not in primitive:
            errors.append('Primitive world coordinates: missing checked boundary ' + entry)
    bodies = list(function_bodies(primitive, r'\bMBoundingBox\s+(DPrimBaseData::getBBox)\s*\('))
    if len(bodies) != 1 or 'worldBounds(pointArray, m_mat)' not in bodies[0][1]:
        errors.append('Primitive bounds: must use the double transformed bounds adapter')
    elif re.search(r'\b(getPointsInWorld|CalBBoxFromPointArray|MPToDP)\s*\(', bodies[0][1]):
        errors.append('Primitive bounds: premature float narrowing returned')
    for relative in ('src/core/Data/DCurveData.cpp', 'src/modules/mesh/DMesh/Data/DMeshData.cpp'):
        code = strip_comments_and_literals((root / relative).read_text())
        bodies = list(function_bodies(code,
            r'\bvoid\s+(\w+::getPrimitiveCenterList)\s*\(\s*MVectorArray\s*&\s*centerList\s*\)'))
        if len(bodies) != 1 or 'getCenterDouble()' not in bodies[0][1] or 'getCenter()' in bodies[0][1]:
            errors.append(relative + ': Maya vector centers must retain double precision')
        elif 'dommy::maya::buildCheckedArray(' not in bodies[0][1] or 'setLength(' in bodies[0][1]:
            errors.append(relative + ': center arrays must use checked staged allocation/copy')
    for node, data, floating, relative in (
        ('DMExplodeView', 'DMeshData', 'true', 'src/modules/mesh/DMesh/Utility/DMExplodeView.cpp'),
        ('DCExplodeView', 'DCurveData', 'false', 'src/modules/curve/DCurve/Utility/DCExplodeView.cpp'),
    ):
        bodies = list(function_bodies((root / relative).read_text(), r'\bMStatus\s+(' + node + r'::compute)\s*\('))
        expected = r'\s*return dommy::maya::computeExplodeNode<' + node + ', ' + data + ', ' + floating + r'>\([^;]+\);\s*'
        if len(bodies) != 1 or not re.fullmatch(expected, bodies[0][1]):
            errors.append(node + ': delegate to the shared checked world-space pipeline')
    explode = strip_comments_and_literals((root / 'src/core/common/utility/DMayaExplodeNode.h').read_text())
    for entry in ('getPluginDataFromHandle(', 'validatePluginData(*input)', 'geometry::buildExplodeTransforms(',
                  'setPluginDataOutput<Data>(', 'output.copyFrom(*input)', 'setTransformMatrix(transforms[index])',
                  'output.updateBBox()', 'return block.setClean(plug)'):
        if entry not in explode:
            errors.append('ExplodeView: missing checked boundary ' + entry)
    if explode.find('geometry::buildExplodeTransforms(') > explode.find('setPluginDataOutput<Data>('):
        errors.append('ExplodeView: plan before copying/publishing custom data')
    if re.search(r'block\.inputValue\([^,()]+\)', explode):
        errors.append('ExplodeView: all Maya input reads must check status')
    explode_math = strip_comments_and_literals((root / 'src/core/common/utility/DPrimitiveExplode.h').read_text())
    for entry in ('getCenterDouble()', 'attribute::convertVector3Attribute(',
                  'numeric::checkedPointMean(selected, mean)', 'replaceGroupCenter(geometricCenters,',
                  'translatedWorldMatrix(', 'numeric::appendTransformedBounds(', 'output.swap(candidate)'):
        if entry not in explode_math:
            errors.append('ExplodeView: missing tested precision/transaction boundary ' + entry)
    if re.search(r'\b(getCenter|getPointsInWorld|replacePositions|setPosition)\(', explode_math):
        errors.append('ExplodeView: float center or world-delta-to-local-point path returned')
    for prefix, relative in (('DM', 'src/modules/mesh/DMesh/common/DMeshUtil.h'),
                              ('DC', 'src/modules/curve/DCurve/DCurveUtil.h')):
        code = strip_comments_and_literals((root / relative).read_text())
        bodies = list(function_bodies(code, r'\bvoid\s+(get' + prefix + r'Vec3ToVectorArray)\s*\('))
        if len(bodies) != 2 or any('dommy::attribute::convertTypedArray<T>(' not in body for _, body in bodies):
            errors.append(relative + ': Vec3 conversions must check type/count/range before mutation')
        bodies = list(function_bodies(code, r'\bvoid\s+(get' + prefix + r'AttributeMVectorArrayFromVec3)\s*\('))
        if len(bodies) != 2 or any('default: throw std::invalid_argument(' not in body for _, body in bodies):
            errors.append(relative + ': a present wrong-type Vec3 attribute must fail explicitly')
    transform_path = 'src/modules/mesh/DMesh/Utility/DMTransform.cpp'
    bodies = list(function_bodies((root / transform_path).read_text(), r'\bMStatus\s+(DMTransform::compute)\s*\('))
    if len(bodies) != 1:
        errors.append('DMTransform: missing checked compute body')
    else:
        body = bodies[0][1]
        for entry in ('getValidInputDM(', 'convertScalarAttribute(', 'convertVector3Attribute(',
                      'getCenterDouble()', 'composeWorldPivotTransform(', 'appendTransformedBounds(',
                      'setPluginDataOutput<DMeshData>(', 'output.copyFrom(*input)',
                      'setTransformMatrix(transforms[index])'):
            if entry not in body:
                errors.append('DMTransform: missing checked world-pivot boundary ' + entry)
        if re.search(r'\b(getCenter|StagedPositionEdits|replacePositions|setPosition)\b', body):
            errors.append('DMTransform: float center or local-vertex edit returned')
        if re.search(r'data\.inputValue\([^,()]+\)', body):
            errors.append('DMTransform: unchecked Maya input read returned')
        if body.find('composeWorldPivotTransform(') > body.find('setPluginDataOutput<DMeshData>('):
            errors.append('DMTransform: plan every matrix before copying/publishing output')
    info = strip_comments_and_literals((root / 'src/modules/mesh/DMesh/Utility/DMInfo.cpp').read_text())
    for entry in ('getValidInputDM(inDM,', 'getValidInputDM(targetDM,',
                  'DMeshPointSpace::Transformed', 'mesh::buildDeformationInfo(',
                  'mesh::vertexAreaChange(', 'mesh::publishDeformationInfo(',
                  'setPluginDataOutput<DMeshData>('):
        if entry not in info:
            errors.append('DMInfo: missing checked world-space boundary ' + entry)
    deformation = strip_comments_and_literals((root / 'src/core/common/utility/DMeshDeformationInfo.h').read_text())
    for entry in ('maximumDistance != 0.0', 'numeric::checkedTransformPoint(',
                  'numeric::checkedScalarMean(', 'attribute::commitStagedAttribute('):
        if entry not in deformation:
            errors.append('DMInfo: missing tested math/publication boundary ' + entry)
    centers = strip_comments_and_literals((root / 'src/modules/mesh/DMesh/common/DMeshUtil.cpp').read_text())
    bodies = list(function_bodies(centers, r'\bbool\s+(buildPrimitiveCenters)\s*\('))
    if len(bodies) != 1 or 'getCenterDouble()' not in bodies[0][1] or 'checkedPointMean(' in bodies[0][1]:
        errors.append('DMeshUtil: primitive centers must use the world-space center accessor')
    character = strip_comments_and_literals((root / 'src/modules/mesh/DMesh/Utility/DMCharactor.cpp').read_text())
    if 'DMGeneratePrimitiveCenterList(*outputMeshes, centers)' not in character or 'commitStagedAttribute(' not in character:
        errors.append('DMCharactor: generate world centers and publish the staged center field')
    for name in ('DMConnetedPieces', 'DMConnectTargets'):
        code = strip_comments_and_literals((root / ('src/modules/mesh/DMesh/Utility/' + name + '.cpp')).read_text())
        for entry in ('WorldPointList', 'WorldPointSearchKDTreeRadiusWithDistanceOp',
                      'checkedTransformPoint(', 'getCenterDouble()', 'getValidInputDM(',
                      'validArrayConversionBudget('):
            if entry not in code:
                errors.append(name + ': missing bounded world-space search boundary ' + entry)
        if re.search(r'\bgetCenter\(|\bgetInput(?:DP|DC|DM)fromPlug\(', code):
            errors.append(name + ': float center or out-of-context target value read returned')
        if name == 'DMConnetedPieces':
            for entry in ('DMeshPointSpace::Transformed', 'direction::difference(', 'checkedLinearBlend(',
                          'MSpace::kObject, nullptr, &status'):
                if entry not in code:
                    errors.append(name + ': missing checked world-space surface offset ' + entry)
        else:
            for entry in ('Imath::V3d first', 'Imath::V3d second', 'data.inputValue(aTargetDP, &status)',
                          'getValidInputDC(aTargetDC, data, target)', 'getValidInputDM(aTargetDM, data, target)',
                          'curveFunction.setCVs(worldCVs, MSpace::kObject)', 'curveFunction.updateCurve()'):
                if entry not in code:
                    errors.append(name + ': missing current-context/precision boundary ' + entry)
            if code.find('curveFunction.updateCurve()') > code.find('curveFunction.length('):
                errors.append(name + ': evaluate half arc length only after transforming CVs')
    search = strip_comments_and_literals((root / 'src/core/common/object/DPointOp.h').read_text())
    for entry in ('const Points& points_', 'PointSearchKDTreeRadiusWithDistance<DPointList>',
                  'PointSearchKDTreeRadiusWithDistance<WorldPointList>'):
        if entry not in search:
            errors.append('Radius query worker: preserve typed local/world point paths ' + entry)
    curve_builder = strip_comments_and_literals((root / 'src/modules/curve/DCurve/Common/DConnectionCurveBuilder.h').read_text())
    if curve_builder.count('maya::isFloatRepresentablePoint(') != 2:
        errors.append('Connection curves: validate both endpoints before float-backed storage')
    for prefix, relative in (('DMesh', 'src/modules/mesh/DMesh/common/DMeshUtil.cpp'),
                              ('DCurve', 'src/modules/curve/DCurve/DCurveUtil.cpp')):
        bodies = list(function_bodies((root / relative).read_text(),
            r'\bvoid\s+(get' + prefix + r'MatchedGroups)\s*\('))
        if len(bodies) != 1 or 'dommy::group::appendMatchedGroupNames(' not in bodies[0][1]:
            errors.append(relative + ': must share the staged group-pattern boundary')
    if 'group::forEachMatchingGroup(' not in explode_math:
        errors.append('ExplodeView: must share the checked group-pattern boundary')
    for path in (root / 'src').rglob('*'):
        if path.suffix not in ('.h', '.cpp', '.hpp', '.inl') or path.name == 'DGroupPattern.h':
            continue
        if re.search(r'\bstd::regex(?:_match|_search)?\b', strip_comments_and_literals(path.read_text())):
            errors.append(str(path.relative_to(root)) + ': bypasses the common group-pattern boundary')
    for owner, relative in (
        ('DMeshDisplay', 'src/modules/mesh/DMesh/Display/DMeshDisplay.cpp'),
        ('DMMarker', 'src/modules/mesh/DMesh/Display/DMMarker.cpp'),
        ('DCurveDisplay', 'src/modules/curve/DCurve/Display/DCurveDisplay.cpp'),
        ('DCMarker', 'src/modules/curve/DCurve/Display/DCMarker.cpp'),
        ('DCurveShape', 'src/modules/curve/DCurve/Shape/DCurveShape.cpp'),
    ):
        source = (root / relative).read_text()
        code = strip_comments_and_literals(source)
        if not re.search(owner + r'::boundingBox\(\)\s+const\s+try\b', code) or (
                'DOMMY_VIEWPORT_BOUNDING_BOX_CATCH("' + owner + '")') not in source:
            errors.append(owner + ': bounding-box callbacks must contain transformed-data exceptions')
    variables = strip_comments_and_literals((root / 'src/core/common/utility/DSeExprVariables.h').read_text())
    for entry in ('double val = 0.0', 'SeVec3d val = SeVec3d(0.0, 0.0, 0.0)'):
        if entry not in variables:
            errors.append('SeExpr variables: safe default initialization is missing')
    for relative in ('src/modules/point/DPoint/Common/DPointSeExpr.h',
                     'src/modules/curve/DCurve/DCurveSeExpr.h'):
        if 'using SimpleVar = dommy::expression::ScalarVariable' not in (root / relative).read_text():
            errors.append(relative + ': use the initialized shared scalar variable')
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    print('Attribute conversion/publication call-path check: OK')
    return 0


if __name__ == '__main__':
    sys.exit(main())
