#!/usr/bin/env python3
"""Complete syntax builders/adapters/parsers with frozen legacy flag contracts."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract


def generate(root, contract, sdk=False):
    output, cases = [], []
    for index, node in enumerate(contract):
        name = node['name']
        namespace = 'syntax_' + str(index)
        source = (root / node['path']).read_text()
        build = extract(source, 'MStatus ' + name + '::buildSyntax(')
        adapter = extract(source, 'MSyntax ' + name + '::newSyntax(')
        if not re.search(r'newSyntax\(\)\s+try\s*\{', adapter) or \
                'reportCommandSyntaxException<' + name + '>' not in adapter:
            raise AssertionError('Value-return callback boundary guard missing: ' + name)
        constants = []
        for match in re.finditer(r'^#define\s+(\w+)\s+"[^"\n]*"', source, re.M):
            if re.search(r'\b' + match.group(1) + r'\b', build): constants.append(match.group())
        for match in re.finditer(r'const\s+char[^;\n]*?\b(\w+)(?:\[\])?\s*=\s*"[^"\n]*";', source):
            if re.search(r'\b' + match.group(1) + r'\b', build): constants.append(match.group())
        fields = ''
        parse = ''
        if not sdk and name == 'getCurveLengthPosCmd':
            fields = 'double lengthPos = 0; MString curve; MStatus parseArgs(const MArgList&);'
            parse = extract(source, 'MStatus ' + name + '::parseArgs(')
        if not sdk and name == 'DPointDataExportCmd':
            fields = 'MString filePath; std::vector<MString> savedNodeNames; MStatus parseArgs(const MArgList&);'
            parse = extract(source, 'MStatus ' + name + '::parseArgs(')
        if parse and 'buildCommandSyntax<' + name + '>' not in parse:
            raise AssertionError('Parser does not validate syntax: ' + name)
        output.append('namespace ' + namespace + ' {\n' + '\n'.join(constants) +
            '\nstruct ' + name + ' { static void* creator() { return nullptr; }\n'
            'static MStatus buildSyntax(MSyntax&); static MSyntax newSyntax(); ' + fields + ' };\n' +
            build + '\n' + adapter + '\n' + parse + '\n}')
        qualified = namespace + '::' + name
        expected = ','.join(json.dumps(value) for value in node['operations'])
        if not sdk:
            cases.append('{"' + name + '", {' + expected + '},\n' +
                '[](MSyntax& s){ return dommy::maya::buildCommandSyntax<' + qualified + '>(s,"' + name + '"); },\n' +
                qualified + '::newSyntax,\n' +
                '[](){ return dommy::maya::validateCommandSyntax<' + qualified + '>("' + name + '"); },\n' +
                '[](){ return dommy::maya::commandSyntaxStatus<' + qualified + '>(); },\n' +
                '[](dommy::PluginRegistration& r){ return r.registerCommandWithSyntax<' + qualified + '>("' + name + '"); }},')
    if sdk:
        output.append('void checkedSyntaxRegistration(MFnPlugin& p, dommy::PluginLifetime& l) { dommy::PluginRegistration r(p,l);')
        for index, node in enumerate(contract):
            output.append('r.registerCommandWithSyntax<syntax_' + str(index) + '::' + node['name'] + '>("' + node['name'] + '");')
        output.append('}')
    else:
        output.append('const SyntaxCase syntaxCases[] = {\n' + '\n'.join(cases) + '\n};')
        names = {node['name']: 'syntax_' + str(index) + '::' + node['name'] for index, node in enumerate(contract)}
        output.append('using CurveCommand = ' + names['getCurveLengthPosCmd'] + ';\nusing ExportCommand = ' + names['DPointDataExportCmd'] + ';')
    return '\n\n'.join(output)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mutations', action='store_true')
    parser.add_argument('--maya-sdk', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    contract = json.loads((root / 'tests/fixtures/command_syntax_contract.json').read_text())
    registered = '\n'.join(path.read_text() for path in (root / 'src/plugin/registration/modules').glob('*.inl'))
    for node in contract:
        name = node['name']
        if not re.search(r'registerCommandWithSyntax<' + name + r'>\(', registered):
            raise AssertionError('Syntax command bypasses preflight: ' + name)
        source = (root / node['path']).read_text()
        parser_body = extract(source, 'MStatus ' + name + ('::doIt(' if name == 'DExportAbcCmd' else '::parseArgs('))
        if 'buildCommandSyntax<' + name + '>' not in parser_body:
            raise AssertionError('Command execution bypasses checked syntax: ' + name)
    registration = extract((root / 'src/plugin/registration/PluginRegistration.h').read_text(), 'namespace dommy {')
    generated = generate(root, contract)
    with tempfile.TemporaryDirectory(prefix='dommy-command-syntax-') as directory:
        temp = Path(directory)
        (temp / 'maya').mkdir()
        (temp / 'maya/MSyntax.h').write_text((root / 'tests/cpp/command_syntax_host.h').read_text())
        include = temp / 'command_syntax_methods.inc'
        include.write_text(registration + '\n' + generated)
        binary = temp / 'test'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fno-elide-constructors',
            '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
            '-I' + str(temp), '-I' + str(root / 'tests/stubs'), '-I' + str(root / 'src/core'),
            str(root / 'tests/cpp/command_syntax_fixture.cpp'), '-o', str(binary)]
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True, env=env)
        print('Six frozen syntax contracts, preflight/rollback/latch, return-copy/fallback ABI limits and two complete parsers passed.')
        if args.mutations:
            adapter = extract(generated, 'MSyntax getCurveLengthPosCmd::newSyntax(')
            unguarded_adapter = '''MSyntax getCurveLengthPosCmd::newSyntax() {
    return dommy::maya::createCommandSyntax<getCurveLengthPosCmd>("getCurveLengthPosCmd");
}'''
            mutations = (
                ('unguarded callback return copy', generated.replace(adapter, unguarded_adapter, 1)),
                ('ignored syntax failure', generated.replace('DOMMY_RETURN_IF_FAILURE(syntax.addFlag(klengthPosFlag, klengthPosLong, MSyntax::kDouble));', 'syntax.addFlag(klengthPosFlag, klengthPosLong, MSyntax::kDouble);', 1)),
                ('changed flag spelling', generated.replace('"-lengthPos"', '"-lengthPosition"', 1)),
                ('ignored database binding failure', generated.replace('MArgDatabase argData(commandSyntax, args, &status);', 'MArgDatabase argData(commandSyntax, args);', 1)),
                ('omitted requested export node', generated.replace('parsedNodeNames.push_back(nodeName);', 'if (use != 1U) parsedNodeNames.push_back(nodeName);', 1)),
                ('lost syntax registration cleanup', registration.replace('recordFailure(syntaxStatus);', 'if (!syntaxStatus) lifetime_.cleanupActions_.pop_back();\n        recordFailure(syntaxStatus);', 1) + '\n' + generated),
            )
            for label, changed in mutations:
                if changed in (generated, registration + '\n' + generated):
                    raise AssertionError('Mutation not applied: ' + label)
                include.write_text(changed if label == 'lost syntax registration cleanup' else registration + '\n' + changed)
                subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                result = subprocess.run([str(binary)], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                if result.returncode == 0: raise AssertionError('Undetected mutation: ' + label)
                print('Rejected mutation:', label)
        if args.maya_sdk:
            sdk_source = temp / 'sdk_syntax.cpp'
            sdk_source.write_text('#include "plugin/registration/PluginRegistration.h"\n#include "common/utility/DStatusOperation.h"\n' + generate(root, contract, sdk=True))
            subprocess.run(shlex.split(os.environ.get('CXX', 'clang++')) + [
                '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                '-Wno-deprecated-declarations', '-isystem', str(args.maya_sdk / 'include'),
                '-I' + str(root / 'src'), '-I' + str(root / 'src/core'),
                '-fsyntax-only', str(sdk_source)], check=True)
            print('Six builders/adapters and checked registration compile against supplied Maya SDK.')


if __name__ == '__main__':
    main()
