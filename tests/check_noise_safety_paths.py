#!/usr/bin/env python3
"""Test real libnoise and extracted noise evaluation with failure-injectable hosts.

The host/TBB doubles check control flow, not Maya geometry or TBB scheduling.
The optional external FastNoise algorithm is not substituted for libnoise here.
"""

import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract


def main():
    root = Path(__file__).resolve().parent.parent
    header = (root / 'src/core/common/object/DNoiseOp.h').read_text()
    common = (root / 'src/core/common/object/DNoiseOp.cpp').read_text()
    paths = {
        'mesh': 'src/modules/mesh/DMesh/Utility/DMNoise.cpp',
        'fast': 'src/modules/mesh/DMesh/Utility/DMFastNoise.cpp',
        'curve': 'src/modules/curve/DCurve/Utility/DCNoise.cpp',
        'point': 'src/modules/point/DPoint/Utility/DPNoise.cpp',
        'pyro': 'src/modules/point/DPoint/Utility/DPPyroNoise.cpp',
        'point_fast': 'src/modules/point/DPoint/Utility/DPFastNoise.cpp',
        'df': 'src/modules/point/DPoint/Utility/DPDFNoise.cpp',
    }
    sources = {name: (root / path).read_text() for name, path in paths.items()}
    for name, source in sources.items():
        compute = source[source.index('::compute('):source.index('DOMMY_COMPUTE_CATCH')]
        if re.search(r'\b(checkDMeshPointAttribute|checkDCurvePointAttribute|addAttrWithValues)\s*\(', compute):
            raise SystemExit(name + ': noise attributes must be staged after input reads')
        if 'commitStagedAttribute(' not in compute and name not in ('point_fast', 'df'):
            raise SystemExit(name + ': missing checked attribute publication')
        if name != 'curve':
            evaluation = source[source.index('::evalNoise'):source.index('DOMMY_COMPUTE_CATCH')]
            if re.search(r'inputValue\([^,)]*\)|CHECK_MSTATUS\(', evaluation):
                raise SystemExit(name + ': unchecked Maya failure')
        if name in ('mesh', 'fast'):
            for boundary in ('getValidInputDM(', 'getCheckedTotalVertex(', 'createPluginData(',
                             'convertVector3Attribute(', 'offsetAlongVector(', 'replacePositions(',
                             'generateMMeshFromDmesh(**mesh, object, DMeshPointSpace::Local)'):
                if boundary not in compute:
                    raise SystemExit(name + ': missing ' + boundary)
            if compute.index('commitStagedAttribute(') < compute.index('offsetAlongVector('):
                raise SystemExit(name + ': output aliases must remain readable')
    for path in ('src/modules/point/DPoint/Utility/DPNoise.cpp', paths['curve'], paths['mesh']):
        source = (root / path).read_text()
        for line in source.splitlines():
            if 'createNoiseFromPosition(' in line and 'DOMMY_RETURN_IF_FAILURE(' not in line:
                raise SystemExit(path + ': noise failure is not propagated')

    enums = []
    for alias in ('NoiseBaseType', 'NoiseDeformType', 'FastNoiseType', 'FNInterpType',
                  'FNFractalType', 'FNCellularDistanceType', 'FNCellularReturnType'):
        match = re.search(r'enum\s*\{[^}]*\};typedef short ' + alias + ';', header)
        if not match:
            raise SystemExit('Noise enum inventory changed')
        enums.append(match.group())
    bodies = enums + [extract(header, 'struct NoiseBaseParams') + ';',
                      extract(header, 'struct FastNoiseParams') + ';',
                      extract(header, 'class NoiseFromDP') + ';',
                      'namespace dommy { namespace math { double degreesToRadians(double value) { return value * 0.0174532925199432957692369076848; } } }',
                      extract(common, 'dommy::noise::WoodSettings makeWoodSettings('),
                      extract(common, 'MStatus validateNoiseBaseParams('),
                      extract(common, 'MStatus createNoiseFromPosition(')]
    bodies += [extract(header, 'class PyroNoiseFromPosition') + ';',
               extract(common, 'MStatus validatePyroNoiseParams('),
               extract(common, 'MStatus createPyroNoiseFromPosition('),
               extract(common, 'MStatus validateFastNoiseParams('),
               extract(common, 'bool makeFastNoisePosition('),
               extract(header, 'class FastNoiseFromDP') + ';',
               extract(common, 'MStatus configureFastNoise('),
               extract(common, 'MStatus createFastNoiseFromPosition(')]
    bodies += [extract(header, 'class PyroNoiseFromCurve') + ';',
               extract(header, 'class PyroNoiseFromMesh') + ';',
               extract(common, 'MStatus storeDivergence('),
               extract(common, 'MStatus makeNoisePosition('),
               extract(header, 'inline MVector getGradientVector('),
               extract(common, 'MStatus validateDFNoiseParams('),
               extract(common, 'MStatus createDevergenceFreenoise('),
               extract(common, 'MStatus createDFNoiseFromPosition(')]
    for name, cls in (('mesh', 'DMNoise'), ('curve', 'DCNoise'),
                      ('point', 'DPNoise'), ('pyro', 'DPPyroNoise'), ('df', 'DPDFNoise')):
        body = extract(sources[name], 'MStatus ' + cls + '::evalNoiseBaseParms(')
        bodies.append(body.replace(cls + '::evalNoiseBaseParms', 'read_' + name))
    fast = sources['fast']
    for name, cls in (('fast', 'DMFastNoise'), ('point_fast', 'DPFastNoise')):
        bodies.append(extract(sources[name], 'MStatus ' + cls + '::evalNoiseParms(')
                      .replace(cls + '::evalNoiseParms', 'read_' + name))
    point = sources['point']
    # This fixture exercises the arithmetic kernel. Complete point output and
    # aggregate admission are executed by check_point_coordinate_output.py.
    start = point.index('    DPointList points =', point.index('::compute('))
    end = point.index('    status = dommy::maya::setPluginDataOutput<DPData>', start)
    bodies.append('MStatus evaluate_points(const Mesh* input, NoiseBaseParams params, '
                  'short deformType, const Imath::V3d& baseVector, '
                  'const std::string& vectorAttributeName, DPointList& output) {\n' +
                  'const std::size_t count = input->numberOfPoints();\n' +
                  point[start:end] + '\noutput.swap(points); return MS::kSuccess; }')
    pyro = sources['pyro']
    start = pyro.index('    std::size_t work =')
    end = pyro.index('    for (const DPoint& point : points)', start)
    bodies.append('MStatus pyro_budget(const SizedPoints& points, std::size_t sourceCount, '
                  'const NoiseBaseParams& params) {\n' + pyro[start:end] +
                  '\nreturn MS::kSuccess; }')
    if end > pyro.index('    DScalarDataList density('):
        raise SystemExit('pyro: work budget must precede density allocation')
    point_fast = sources['point_fast']
    bodies.append(extract(point_fast, 'template <typename Geometry, typename NormalReader>'))
    bodies.append(extract(point_fast[point_fast.index('MStatus deformFastNoiseGeometry(') -
                         len('template <typename Geometry, typename NormalReader>\n'):],
                         'template <typename Geometry, typename NormalReader>'))
    bodies.append(extract(point_fast, 'MStatus advanceFastNoiseSeed('))
    mesh_reader = extract(point_fast[point_fast.rindex('[&]'):], '[&]')
    bodies.append('MStatus point_fast_normals(const std::shared_ptr<Mesh>& mesh, '
                  'const DPointList& points, std::vector<Imath::V3d>& vectors) { '
                  'const auto* item = &mesh; MStatus status; return (' + mesh_reader +
                  ')(points,vectors); }')
    if point_fast.count('deformFastNoiseGeometry(') != 3 or point_fast.count('evaluateFastNoiseGeometry(') != 3:
        raise SystemExit('point_fast: all three data types must use the checked shared path')
    df = sources['df']
    bodies += [extract(df, 'template <typename Geometry>'),
               extract(df, 'DPAttributeData::Ptr makeDFNoiseAttribute('),
               extract(df[df.index('MStatus deformDFNoiseGeometry(') - len('template <typename Geometry>\n'):],
                       'template <typename Geometry>'),
               extract(df, 'MStatus validateDFNoiseSeedRange('),
               extract(df, 'template <typename PrimitiveData>')]
    start = df.index('    MDataHandle handle =', df.index('::compute('))
    end = df.index('    MFnPluginData pluginData;', start)
    bodies.append('MStatus read_df_controls(MDataBlock& data) { MStatus status;\n' +
                  df[start:end].replace('evalNoiseBaseParms(', 'read_df(') +
                  '\n(void)useSeedOffset; (void)isDeform; (void)attributeWidth; (void)pointBytes; return MS::kSuccess; }')
    if df.count('deformDFNoiseGeometry(') != 2 or df.count('evaluateDFNoiseGeometry(') != 3 or df.count('deformDFNoisePrimitives(') != 3:
        raise SystemExit('df: all data types must use the same displacement/publication path')
    if df.count('validateDFNoiseSeedRange(') != 3:
        raise SystemExit('df: both primitive branches must preflight cumulative seeds')
    for name in ('PyroNoiseFromCurve', 'PyroNoiseFromMesh'):
        worker = extract(header, 'class ' + name)
        if 'const float noiseValue' in worker or 'm_radius + mNoiseParam.mAmplitude' in worker:
            raise SystemExit(name + ': unsafe object-noise arithmetic returned')
        if 'densityArray->swap(candidate)' not in worker:
            raise SystemExit(name + ': missing transactional density commit')
    for name, source in sources.items():
        for call in ('createFastNoiseFromPosition(', 'createPyroNoiseFromPosition(',
                     'createDFNoiseFromPosition('):
            for line in source.splitlines():
                if call in line and 'DOMMY_RETURN_IF_FAILURE(' not in line:
                    raise SystemExit(name + ': unpropagated noise failure')
    mesh = sources['mesh']
    start = mesh.index('    MDataHandle handle =', mesh.index('::compute('))
    end = mesh.index('    MFnPluginData pluginData;', start)
    controls = mesh[start:end].replace('evalNoiseBaseParms(', 'read_mesh(')
    bodies.append('MStatus read_controls(MDataBlock& data) { MStatus status;\n' + controls +
                  '\n(void)useSeedOffset; (void)vectorAttributeName; return MS::kSuccess; }')
    start = mesh.index('    std::size_t meshOffset =')
    end = mesh.index('    outputMeshes->updateBBox();', start)
    bodies.append('MStatus evaluate_meshes(DMeshData* outputMeshes, NoiseBaseParams params, '
                  'bool useSeedOffset, bool isNoiseAttribute, short deformType, '
                  'const Imath::V3d& baseVector, const std::string& noiseAttributeName, '
                  'const std::string& vectorAttributeName, double amplitude) { MStatus status;\n' +
                  mesh[start:end] + '\nreturn MS::kSuccess; }')
    # Execute the optional node's normal-query branch too, without pretending
    # that the external FastNoise generator is covered by this host double.
    normals = extract(sources['fast'], 'if (deformType == NOISE_DEFROM_MESH_NORMAL && count != 0U)')
    bodies.append('MStatus fast_normals(const std::shared_ptr<Mesh>& item, '
                  'std::vector<Imath::V3d>& vectors) { const auto* mesh = &item; '
                  'const short deformType = NOISE_DEFROM_MESH_NORMAL; '
                  'const std::size_t count = vectors.size(); MStatus status;\n' + normals +
                  '\nreturn MS::kSuccess; }')
    with tempfile.TemporaryDirectory(prefix='dommy-noise-safety-') as directory:
        temporary = Path(directory)
        (temporary / 'noise_methods.inc').write_text('\n\n'.join(bodies))
        binary = temporary / 'tests'
        modules = ('noisegen', 'modulebase', 'perlin', 'billow', 'ridgedmulti', 'voronoi',
                   'turbulence', 'cylinders', 'scalepoint', 'scalebias', 'add',
                   'translatepoint', 'rotatepoint')
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-unused-parameter',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src'), '-I' + str(root / 'src/core'),
            '-isystem', str(root / 'third_party/libnoise/source'),
            str(root / 'tests/cpp/noise_safety_tests.cpp')]
        command.extend(str(root / 'third_party/libnoise/source' / (name + '.cpp')) for name in modules)
        command += [str(root / 'src/core/noise/PerlinNoise.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary)], check=True, env=environment)
    print('Noise arithmetic, coordinate, attribute, and Maya-failure checks: OK')


if __name__ == '__main__':
    main()
