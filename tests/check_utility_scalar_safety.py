#!/usr/bin/env python3
"""Production legacy scalar templates: type, integer and conversion boundaries."""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_path_reservation_exception_safety import disable_core_dumps

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / 'src/core/Utility/DUtility.h'
INTEGER = ROOT / 'src/core/common/utility/DIntegerBlend.h'
CHECKED_SIZE = ROOT / 'src/core/common/utility/DCheckedSize.h'
SOURCE = ROOT / 'tests/cpp/utility_vector_tests.cpp'


def run(replacement=None, mutation=False, analyze=False, sdk=None, section='all', integer=None,
        contraction_off=False, checked_size=None):
    with tempfile.TemporaryDirectory(prefix='dommy-utility-scalar-') as directory:
        temporary = Path(directory)
        if replacement is not None:
            shadow = temporary / 'Utility/DUtility.h'
            shadow.parent.mkdir(parents=True)
            shadow.write_text(replacement)
        if integer is not None:
            shadow = temporary / 'common/utility/DIntegerBlend.h'
            shadow.parent.mkdir(parents=True)
            shadow.write_text(integer)
        if checked_size is not None:
            shadow = temporary / 'common/utility/DCheckedSize.h'
            shadow.parent.mkdir(parents=True, exist_ok=True)
            shadow.write_text(checked_size)
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-DDOMMY_UTILITY_VECTOR_TEST_MAIN', '-I' + str(temporary),
            '-I' + str(ROOT / 'src/core'),
            '-I' + str(ROOT / 'src/core/Utility/3d/Math')]
        if contraction_off:
            command += ['-ffp-contract=off']
        if sdk:
            command += ['-isystem', str(sdk / 'include'),
                '-I' + str(ROOT / 'src/modules/point/DPoint'),
                '-idirafter', str(ROOT / 'tests/stubs')]
            subprocess.run(command + ['-fsyntax-only', str(SOURCE)], check=True)
        else:
            command += ['-I' + str(ROOT / 'tests/stubs')]
        if analyze or sdk:
            subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(SOURCE)], check=True)
            print('Utility scalar/vector templates: Clang analysis' +
                  (' and Maya SDK syntax' if sdk else '') + ': OK')
            return
        binary = temporary / 'utility_scalar_tests'
        command += ['-DDOMMY_UTILITY_ALLOCATION_TESTS=1']
        try:
            subprocess.run(command + ['-fno-omit-frame-pointer',
                '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
                str(SOURCE), '-o', str(binary)], check=True, capture_output=True)
        except subprocess.CalledProcessError as error:
            sys.stderr.write(error.stderr.decode(errors='replace'))
            raise
        options = {'preexec_fn': disable_core_dumps} if os.name == 'posix' else {}
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                           ('0' if sys.platform == 'darwin' else '1'))
        result = subprocess.run([str(binary), section], text=True, capture_output=True,
            cwd=str(temporary), env=environment, timeout=20, **options)
        if mutation:
            if result.returncode == 0 or not any(token in result.stderr for token in
                                               ('Assertion', 'runtime error:')):
                raise AssertionError('Mutation must fail at runtime:\n' + result.stderr)
        elif result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        else:
            print('Utility scalar/vector templates: boundary and compatibility tests: OK', flush=True)


def check_type_contracts(checked_size=None):
    cases = [
        ('D_TO_DEGREES(true)', 'Angle conversion requires non-bool arithmetic scalars'),
        ('D_ABS(std::string("1"))', 'Absolute value requires arithmetic scalars'),
        ('D_FEQ(std::string("1"), std::string("1"))',
         'Equality comparison requires arithmetic scalars'),
    ]
    cases = [('Utility/DUtility.h', expression, message) for expression, message in cases]
    message = 'An index must be a non-bool integer representable in uintmax_t'
    for expression in ('true', '0.0f', '0.0', '0.0L', 'std::string("0")',
                       'SignedIndex::zero', 'UnsignedIndex::zero', 'static_cast<int*>(nullptr)'):
        cases.append(('common/utility/DCheckedSize.h',
                      'dommy::memory::indexInRange(' + expression + ', 1U)', message))
    for qualifiers in ('const', 'volatile', 'const volatile'):
        cases.append(('common/utility/DCheckedSize.h',
                      'dommy::memory::indexInRange<' + qualifiers + ' bool>(true, 1U)', message))
    with tempfile.TemporaryDirectory(prefix='dommy-scalar-types-') as directory:
        temporary = Path(directory)
        if checked_size is not None:
            shadow = temporary / 'common/utility/DCheckedSize.h'
            shadow.parent.mkdir(parents=True)
            shadow.write_text(checked_size)
        source = temporary / 'type_contract.cpp'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-fsyntax-only', '-I' + str(temporary), '-I' + str(ROOT / 'src/core'),
            '-I' + str(ROOT / 'src/core/Utility/3d/Math'), '-I' + str(ROOT / 'tests/stubs')]
        # A self-contained checked-size header must admit all standard integer
        # families without relying on Maya/Utility umbrella headers.
        source.write_text('#include "common/utility/DCheckedSize.h"\nint main() {\n' + ''.join(
            '(void)dommy::memory::indexInRange<' + name + '>(0, 1U);\n' for name in
            ('char', 'signed char', 'unsigned char', 'short', 'unsigned short', 'int',
             'unsigned int', 'long', 'unsigned long', 'long long', 'unsigned long long',
             'wchar_t', 'char16_t', 'char32_t', 'std::size_t', 'std::ptrdiff_t',
             'std::intmax_t', 'std::uintmax_t', 'const int', 'volatile unsigned long',
             'const volatile std::size_t')) + '}\n')
        subprocess.run(command + ['-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-Wtype-limits', str(source)], check=True)
        for header, expression, message in cases:
            source.write_text('#include "' + header + '"\n#include <string>\n'
                'enum class SignedIndex : int { zero = 0 };\n'
                'enum class UnsignedIndex : unsigned { zero = 0 };\n'
                'int main() { (void)' + expression + '; }\n')
            result = subprocess.run(command + [str(source)], text=True, capture_output=True)
            if result.returncode == 0 or message not in result.stderr:
                raise AssertionError('Expected type-contract rejection: ' + expression + '\n' + result.stderr)
    print('Utility scalar templates: unsupported type contracts rejected: OK', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-only', action='store_true')
    parser.add_argument('--analyze', action='store_true')
    parser.add_argument('--sdk', type=Path)
    parser.add_argument('--section', choices=('all', 'index-range', 'lerp', 'gradation', 'format', 'legacy-math', 'normal'), default='all')
    parser.add_argument('--no-fma', action='store_true')
    args = parser.parse_args()
    run(analyze=args.analyze, sdk=args.sdk, section=args.section, contraction_off=args.no_fma)
    if args.baseline_only or args.analyze or args.sdk:
        return
    run(section='normal', contraction_off=True)
    check_type_contracts()
    source = CHECKED_SIZE.read_text()
    for label, before, after in (
        ('negative index admission restored',
         'if (std::is_signed<Integer>::value && index < Integer(0)) return false;', ''),
        ('one-past-end index admitted',
         'return static_cast<std::uintmax_t>(index) <',
         'return static_cast<std::uintmax_t>(index) <='),
        ('integer endpoints rounded through float',
         'return static_cast<std::uintmax_t>(index) <\n'
         '           static_cast<std::uintmax_t>(count);',
         'return static_cast<float>(index) < static_cast<float>(count);'),
        ('empty count subtraction underflows',
         'return static_cast<std::uintmax_t>(index) <\n'
         '           static_cast<std::uintmax_t>(count);',
         'return static_cast<std::uintmax_t>(index) <=\n'
         '           static_cast<std::uintmax_t>(count - 1U);'),
    ):
        assert source.count(before) == 1, label
        run(checked_size=source.replace(before, after, 1), mutation=True, section='index-range')
        print('Detected index mutation: ' + label, flush=True)
    contract = ('static_assert(std::is_integral<Integer>::value && !std::is_same<Integer, bool>::value &&\n'
                '                  std::numeric_limits<Integer>::digits <= std::numeric_limits<std::uintmax_t>::digits,\n'
                '                  "An index must be a non-bool integer representable in uintmax_t");')
    assert source.count(contract) == 1
    try:
        check_type_contracts(source.replace(contract, '', 1))
    except AssertionError as error:
        assert 'dommy::memory::indexInRange(true, 1U)' in str(error), str(error)
        print('Detected index mutation: integer type contract removed', flush=True)
    else:
        raise AssertionError('Index type-contract mutation was not detected')
    source = HEADER.read_text()
    cases = (
        ('signed minimum negated unchecked',
         'if (std::is_integral<T>::value && std::is_signed<T>::value &&\n'
         '        value == std::numeric_limits<T>::min())\n'
         '        throw std::overflow_error("Scalar negation is not representable");', ''),
        ('integer epsilon comparison rounds uint64',
         'return difference < threshold;', 'return static_cast<double>(difference) < epsilon;'),
        ('scalar equality restores overflowing subtraction',
         'return dommy::utility::detail::approximatelyEqual(a, b, D_FEQ_EPS);',
         'return std::fabs(a - b) < D_FEQ_EPS;'),
        ('vector equality restores overflowing subtraction',
         'return dommy::utility::detail::approximatelyEqual(a[0], b[0], eps) &&\n'
         '           dommy::utility::detail::approximatelyEqual(a[1], b[1], eps) &&\n'
         '           dommy::utility::detail::approximatelyEqual(a[2], b[2], eps);',
         'return std::fabs(a[0] - b[0]) < eps && std::fabs(a[1] - b[1]) < eps && '
         'std::fabs(a[2] - b[2]) < eps;'),
        ('angle output narrows unchecked',
         'if (!numeric::checkedNumericCast(value * factor, result))\n'
         '        throw std::overflow_error("Angle output is not representable");',
         'result = static_cast<T>(value * factor);'),
        ('nonfinite angle loses domain-error contract',
         'if (!std::isfinite(static_cast<long double>(value)))\n'
         '        throw std::domain_error("Angle input must be finite");', ''),
        ('checked interpolation dispatch bypassed',
         'return dommy::utility::detail::legacyInterpolation(a, b, t, Checked{});',
         '(void)sizeof(Checked); return a + t*(b-a);'),
        ('mixed interpolation loses integer endpoint bits',
         'const auto first = interpolationParts(source), last = interpolationParts(target);',
         'const std::array<double, 2> first{{static_cast<double>(source), 0.0}}, '
         'last{{static_cast<double>(target), 0.0}};'),
        ('mixed interpolation loses integer weight bits',
         'const auto weights = interpolationParts(weight);',
         'const std::array<double, 2> weights{{static_cast<double>(weight), 0.0}};'),
        ('gradation restores overflowing range division',
         'else if (!dommy::math::normalizeRange(x, xmin, xmax, dx))\n'
         '            throw std::overflow_error("Gradation position is not representable");',
         'else dx = (x - xmin) / (xmax - xmin);'),
        ('gradation publishes partial colors',
         'candidate[axis] = D_LERP(col1[axis], col2[axis], dx);',
         'col[axis] = candidate[axis] = D_LERP(col1[axis], col2[axis], dx);'),
        ('failed string insertion accepted',
         'return dommy::text::checkedStreamText(ss, "String conversion failed");',
         'return ss.str();'),
        ('random range restores overflowing subtraction',
         'return D_LERP(_min, _max, Frand());', 'return (_max-_min)*Frand()+_min;'),
        ('random nonfinite bounds admitted',
         'if (!std::isfinite(_max) || !std::isfinite(_min))\n'
         '            throw std::domain_error("Random bounds must be finite");', ''),
        ('vector random restores float-first subtraction',
         'return DTools::Vec2(static_cast<DReal>(Rand(double(_max[0]), double(_min[0]))),\n'
         '                           static_cast<DReal>(Rand(double(_max[1]), double(_min[1]))));',
         'return DTools::Vec2((_max[0]-_min[0])*Frand()+_min[0], (_max[1]-_min[1])*Frand()+_min[1]);'),
        ('vector random validates only after partial draws',
         'inline DTools::Vec3 Rand(const DTools::Vec3 &_max, const DTools::Vec3 &_min)\n\t{\n'
         '        if (!dommy::utility::detail::finiteInterpolationInput(_max) ||\n'
         '            !dommy::utility::detail::finiteInterpolationInput(_min))\n'
         '            throw std::domain_error("Random bounds must be finite");',
         'inline DTools::Vec3 Rand(const DTools::Vec3 &_max, const DTools::Vec3 &_min)\n\t{'),
        ('average restores float-first addition',
         'throw std::overflow_error("Average output is not representable");\n        return result;',
         'throw std::overflow_error("Average output is not representable");\n        return (A[0]+A[1]+A[2])/3.0;'),
        ('average loses nonfinite domain-error contract',
         'if (!dommy::utility::detail::finiteInterpolationInput(A))\n'
         '            throw std::domain_error("Average input must be finite");', ''),
        ('nonnegative predicate admits nonfinite values',
         'if (!dommy::utility::detail::finiteInterpolationInput(value)) return false;', ''),
        ('rectangle predicate admits nonfinite values',
         'if (!dommy::utility::detail::finiteInterpolationInput(point) ||\n'
         '        !dommy::utility::detail::finiteInterpolationInput(lower) ||\n'
         '        !dommy::utility::detail::finiteInterpolationInput(upper)) return false;', ''),
    )
    for label, before, after in cases:
        if source.count(before) != 1:
            raise AssertionError('Ambiguous mutation boundary: ' + label)
        run(source.replace(before, after), mutation=True)
        print('Detected mutation: ' + label, flush=True)
    source = INTEGER.read_text()
    before = 'return detail::finishIntegerBlend(source, term, descending != weightNegative, false, output);'
    if source.count(before) != 1:
        raise AssertionError('Ambiguous integral interpolation mutation boundary')
    replacement = 'output = static_cast<Output>(source + weight * (target - source)); return true;'
    # Keep variables used so this mutation is rejected at runtime, not by -Werror.
    replacement = '(void)term; (void)weightNegative; ' + replacement
    run(integer=source.replace(before, replacement), mutation=True)
    print('Detected mutation: integral interpolation restores unchecked arithmetic', flush=True)
    source = HEADER.read_text()
    for label, before, after in (
        ('normal random nonfinite parameters admitted',
         'if (!std::isfinite(m) || !std::isfinite(s))\n'
         '            throw std::domain_error("Normal random parameters must be finite");', ''),
        ('normal random restores overflowing intermediate product',
         'if (!dommy::numeric::checkedProductSum(factors, values, result))',
         'result = factors[0] * values[0] + values[1];\n        if (!std::isfinite(result))'),
        ('normal random overflow status discarded',
         'if (!dommy::numeric::checkedProductSum(factors, values, result))\n'
         '            throw std::overflow_error("Normal random output is not representable");',
         '(void)dommy::numeric::checkedProductSum(factors, values, result);'),
    ):
        assert source.count(before) == 1, label
        run(source.replace(before, after), mutation=True, section='normal', contraction_off=True)
        print('Detected mutation: ' + label, flush=True)


if __name__ == '__main__':
    main()
