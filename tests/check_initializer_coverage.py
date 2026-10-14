#!/usr/bin/env python3
"""Inspect every registered initializer for common unchecked attribute calls.

This is a source guard, not a C++ parser or native registration/rollback test.
Failure injection remains in the global and node-specific execution fixtures.
"""
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
from check_active_cpp_safety import strip_comments_and_literals
from check_geometry_state_transactions import extract
from check_maya_type_ids import registration_files


def call_end(code, opening):
    depth, end = 1, opening + 1
    while end < len(code) and depth:
        depth += (code[end] == '(') - (code[end] == ')')
        end += 1
    if depth:
        raise AssertionError('Unbalanced registration/attribute call')
    return end


def registered_initializers(sources):
    result = set()
    for source in sources:
        code = strip_comments_and_literals(source)
        for match in re.finditer(r'\b(?:registerNode|RegisterNode)\s*\(', code):
            end = call_end(code, match.end() - 1)
            names = re.findall(r'\b(\w+)::initialize\b', code[match.end():end - 1])
            if len(names) != 1:
                raise AssertionError('A node registration has no unique explicit initializer')
            result.add(names[0])
    if not result:
        raise AssertionError('No node initializers found on registration surfaces')
    return result


def initializer_errors(source):
    code = strip_comments_and_literals(source)
    attributes = set(re.findall(r'\bMFn\w*Attribute\s+(\w+)\b', code))
    attribute_calls = []
    if attributes:
        owners = '(?:' + '|'.join(re.escape(name) for name in sorted(attributes)) + ')'
        attribute_calls.append(owners + r'\.(?:set\w+|addField|addChild|addDataAccept|addNumericDataAccept)')
    calls = '(?:' + '|'.join(attribute_calls + ['addAttribute', 'attributeAffects']) + ')'
    for match in re.finditer(r'(?:^|[;{}])\s*(' + calls + r')\s*\(', code):
        yield 'Unchecked initializer API call: ' + match[1]
    for owner in attributes:
        for match in re.finditer(r'\b' + re.escape(owner) + r'\.(?:create|createColor|createPoint)\s*\(', code):
            end = call_end(code, match.end() - 1)
            if not re.search(r'&\s*\w+\b', code[match.end():end - 1]):
                yield 'Attribute creation lacks a return-status output: ' + owner
    if re.search(r'\bCHECK_MSTATUS\s*\(', code):
        yield 'Diagnostic-only initializer status check'


def validate(root):
    # Generic forwarding wrappers are not registration manifests. Include all
    # .inl module surfaces (even optional features), plus Arnold's RegisterNode.
    surfaces = [Path(path) for path in registration_files()
                if Path(path).suffix == '.inl' or Path(path).name == 'DArnoldExtentionPlugins.cpp']
    registered = registered_initializers(path.read_text() for path in surfaces)
    definitions = {}
    for path in sorted((root / 'src').rglob('*.cpp')):
        source = path.read_text()
        code = strip_comments_and_literals(source)
        for match in re.finditer(r'\bMStatus\s+(\w+)::initialize\s*\(', code):
            if match[1] in definitions:
                raise AssertionError('Duplicate initializer definition: ' + match[1])
            body = extract(source[match.start():], match.group())
            definitions[match[1]] = (path, body)
    missing = registered - definitions.keys()
    if missing:
        raise AssertionError('Registered initializers missing inspection: ' + ', '.join(sorted(missing)))
    errors = []
    for name in sorted(registered):
        path, body = definitions[name]
        errors.extend(str(path.relative_to(root)) + ': ' + error for error in initializer_errors(body))
    if errors:
        raise AssertionError('\n'.join(errors))
    print('Initializer source coverage: %d registered callbacks inspected (including optional features); no native runtime claim' % len(registered))


def self_test():
    checked = '''MStatus Node::initialize() try { MStatus status; MFnNumericAttribute attr;
value = attr.create("value", "v", MFnNumericData::kInt, 0, &status);
DOMMY_RETURN_IF_FAILURE(status);
DOMMY_RETURN_IF_FAILURE(
 attr.setKeyable(true));
DOMMY_RETURN_IF_FAILURE(addAttribute(value));
DOMMY_RETURN_IF_FAILURE(attributeAffects(value, output));
return MS::kSuccess; } catch (...) { return MS::kFailure; }'''
    assert not list(initializer_errors(checked))
    for old, new in (
        ('DOMMY_RETURN_IF_FAILURE(\n attr.setKeyable(true));', 'attr.setKeyable(\n true);'),
        ('DOMMY_RETURN_IF_FAILURE(addAttribute(value));', 'addAttribute(value);'),
        ('DOMMY_RETURN_IF_FAILURE(attributeAffects(value, output));', 'attributeAffects(value, output);'),
        ('0, &status', '0'),
        ('DOMMY_RETURN_IF_FAILURE(status);', 'CHECK_MSTATUS(status);'),
    ):
        assert list(initializer_errors(checked.replace(old, new))), old
    assert not list(initializer_errors(checked + '\n// attr.setMin(0);\n/* addAttribute(x); */'))
    surface = 'registration.registerNode("name", Node::typeId, Node::creator, Node::initialize);'
    assert registered_initializers([surface]) == {'Node'}
    assert registered_initializers(['// ' + surface + '\n' + surface]) == {'Node'}
    assert registered_initializers(['plugin.RegisterNode(\n "name", Node::id, Node::creator, Node::initialize);']) == {'Node'}
    try:
        registered_initializers([surface.replace('Node::initialize', 'nullptr')])
    except AssertionError:
        pass
    else:
        raise AssertionError('Uninspected registration was accepted')


if __name__ == '__main__':
    self_test()
    validate(Path(__file__).resolve().parent.parent)
