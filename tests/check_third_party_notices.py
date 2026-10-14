#!/usr/bin/env python3
"""Protect recorded attribution and the owned-replacement boundary."""
import hashlib
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent.parent

# Keep these scopes outside the inventory so deleting a group or a file record
# cannot silently disable its protection. These globs inspect only source files
# in the named directory, never recursively imported dependencies.
EMBEDDED_NOTICE_SCOPES = {
    'fastnoise_simd': ('third_party/fastnoise_simd/source', ('*.h', '*.cpp'), 'line_comment'),
    'android_cpu_features': ('third_party/fastnoise_simd/source/ARM', ('*.h', '*.c'), 'block_comment'),
    'voro++': ('third_party/voro++', ('*.hh', '*.cc'), 'line_comment'),
    'jc_voronoi': ('src/modules/point/DPoint/mesh/voro', ('jc_voronoi.h',), 'jc_license'),
}


def notice_text(text, marked, notice_format=None):
    if notice_format == 'line_comment':
        match = re.match(r'\A(?://[^\n]*(?:\n|$))+', text)
        if not match:
            raise ValueError('Missing leading line-comment notice')
        notice = match.group()
    elif notice_format in ('block_comment', 'jc_license'):
        match = re.match(r'\A\s*/\*[\s\S]*?\*/', text)
        if not match:
            raise ValueError('Missing leading block-comment notice')
        notice = match.group()
        if notice_format == 'jc_license':
            # Protect the MIT terms, not the unrelated usage example that
            # follows them in this historical single-header library.
            if notice.count('LICENSE:') != 1 or notice.count('DISCLAIMER:') != 1:
                raise ValueError('Missing or duplicated jc_voronoi license boundary')
            notice = notice.split('LICENSE:', 1)[1].split('DISCLAIMER:', 1)[0]
    elif notice_format is not None:
        raise ValueError('Unsupported embedded-notice format')
    elif marked:
        begin = '// DOMMY_THIRD_PARTY_NOTICE_BEGIN\n'
        end = '\n// DOMMY_THIRD_PARTY_NOTICE_END'
        if text.count(begin) != 1 or text.count(end) != 1:
            raise ValueError('Missing or duplicated attribution boundary')
        notice = text.split(begin, 1)[1].split(end, 1)[0]
        # A restored notice must never accidentally insert preprocessor code.
        if re.search(r'(?m)^\s*#', notice):
            raise ValueError('Executable/preprocessor text inside a notice')
        if 'Modified for dommyTool; not the original upstream version.' not in text:
            raise ValueError('Missing local-modification statement')
    else:
        match = re.search(r'/\*[\s\S]*?\*/', text)
        notice = match.group() if match else text[:text.index('// Adapted from')].rstrip()
    return notice.replace('\r\n', '\n').strip()


def validate_record(record, text):
    digest = hashlib.sha256(notice_text(text, record['marker']).encode('utf-8')).hexdigest()
    if digest != record['notice_sha256']:
        raise ValueError('Attribution or license terms changed: ' + record['path'])
    if record['marker']:
        # Different components may share a file without sharing their terms.
        # Exact reference coverage also catches an origin removed from the
        # inventory while its source notice is still present.
        sources = [record] + record.get('additional_sources', [])
        expected_references = {source['upstream'] for source in sources}
        expected_members = set()
        for source in sources:
            for member in [source.get('archive_member')] + source.get('additional_archive_members', []):
                if member:
                    expected_members.add(member)
        references = re.findall(r'(?m)^// Source comparison reference: (.+)$', text)
        members = re.findall(r'(?m)^// Archive member: (.+)$', text)
        if set(references) != expected_references or len(references) != len(expected_references):
            raise ValueError('Missing, duplicated or unrecorded source comparison reference: ' + record['path'])
        if set(members) != expected_members or len(members) != len(expected_members):
            raise ValueError('Missing, duplicated or unrecorded archive source reference: ' + record['path'])


def validate_vendored_header(record, text):
    directive = re.search(r'(?m)^\s*#', text)
    if not directive:
        raise ValueError('Missing vendored source boundary: ' + record['path'])
    header = text[:directive.start()].strip()
    if not re.search(r'Legacy dommyTool adaptation:|Local dommyTool maintenance \(', header):
        raise ValueError('Missing vendored modification statement: ' + record['path'])
    if hashlib.sha256(header.encode('utf-8')).hexdigest() != record['header_sha256']:
        raise ValueError('Vendored copyright/license/modification header changed: ' + record['path'])


def validate_embedded_record(record, text, notice_format, required_phrases):
    notice = notice_text(text, False, notice_format)
    if any(phrase not in notice for phrase in required_phrases):
        raise ValueError('Incomplete embedded notice: ' + record['path'])
    if hashlib.sha256(notice.encode('utf-8')).hexdigest() != record['notice_sha256']:
        raise ValueError('Embedded notice changed: ' + record['path'])


def validate_embedded_groups(groups):
    if set(groups) != set(EMBEDDED_NOTICE_SCOPES):
        raise ValueError('Missing or unsupported embedded-notice group')
    paths = set()
    for name, group in groups.items():
        directory, patterns, notice_format = EMBEDDED_NOTICE_SCOPES[name]
        if group['notice_format'] != notice_format or not group['reference'].startswith('https://'):
            raise ValueError('Invalid embedded-notice format/reference: ' + name)
        phrases = group['required_phrases']
        if not phrases or any(not isinstance(phrase, str) or not phrase.strip() for phrase in phrases):
            raise ValueError('Invalid embedded-notice requirements: ' + name)
        source_directory = ROOT / directory
        if not source_directory.is_dir() or source_directory.resolve() != source_directory:
            raise ValueError('Missing or redirected embedded source directory: ' + name)
        discovered = {str(path.relative_to(ROOT))
                      for pattern in patterns for path in source_directory.glob(pattern)
                      if path.is_file()}
        recorded = set()
        for record in group['files']:
            path = Path(record['path'])
            if path.is_absolute() or '..' in path.parts or record['path'] in recorded or record['path'] in paths:
                raise ValueError('Invalid or duplicated embedded-notice path')
            if record['path'] not in discovered or (ROOT / path).resolve() != ROOT / path:
                raise ValueError('Embedded-notice path is outside its source scope')
            recorded.add(record['path'])
            validate_embedded_record(record, (ROOT / path).read_text(), notice_format, phrases)
        if not recorded or recorded != discovered:
            raise ValueError('Unrecorded or missing embedded-notice source: ' + name)
        paths.update(recorded)
        expected_license = 'third_party/licenses/Voro++-modified-BSD.txt' if name == 'voro++' else None
        if group.get('license_file') != expected_license:
            raise ValueError('Invalid embedded component license scope: ' + name)
        if expected_license:
            license_path = ROOT / expected_license
            if license_path.resolve() != license_path or not license_path.is_file():
                raise ValueError('Missing embedded component license: ' + name)
            if hashlib.sha256(license_path.read_bytes()).hexdigest() != group['license_sha256']:
                raise ValueError('Embedded component license changed: ' + name)
            if expected_license not in (ROOT / 'THIRD_PARTY_NOTICES.md').read_text():
                raise ValueError('Embedded component license is absent from human-readable notices')
    return paths


def main():
    data = json.loads((ROOT / 'config/third_party_sources.json').read_text())
    if data.get('schema_version') != 1:
        raise ValueError('Unsupported attribution schema')
    components = data['components']
    paths = set()
    for record in data['files']:
        path = Path(record['path'])
        if path.is_absolute() or '..' in path.parts or record['path'] in paths:
            raise ValueError('Invalid or duplicated attribution path')
        paths.add(record['path'])
        if record.get('additional_sources') and not record['marker']:
            raise ValueError('Additional source requires a marked attribution')
        origins = set()
        for source in [record] + record.get('additional_sources', []):
            if source['component'] not in components or not source['upstream'].startswith('https://'):
                raise ValueError('Unresolved attribution record')
            origin = (source['component'], source['upstream'])
            if origin in origins:
                raise ValueError('Duplicated attribution origin: ' + record['path'])
            origins.add(origin)
        validate_record(record, (ROOT / path).read_text())
    # A restored attribution may not silently fall out of the inventory.
    for path in (ROOT / 'src').rglob('*'):
        if path.is_file() and path.suffix in ('.h', '.hpp', '.cpp', '.cc', '.c', '.inl'):
            if re.search(r'(?m)^// DOMMY_THIRD_PARTY_NOTICE_BEGIN$', path.read_text()) and str(path.relative_to(ROOT)) not in paths:
                raise ValueError('Unrecorded restored notice: ' + str(path.relative_to(ROOT)))
    for component in components.values():
        path = ROOT / component['notice_file']
        if not path.is_file() or path.stat().st_size == 0:
            raise ValueError('Missing component license text')
        if hashlib.sha256(path.read_bytes()).hexdigest() != component['notice_sha256']:
            raise ValueError('Component license text changed: ' + component['notice_file'])
        if component['notice_file'] not in (ROOT / 'THIRD_PARTY_NOTICES.md').read_text():
            raise ValueError('Component is absent from human-readable notices')
        for extra in component.get('additional_notice_files', []):
            path = Path(extra['path'])
            if path.is_absolute() or '..' in path.parts:
                raise ValueError('Invalid additional-notice path')
            if hashlib.sha256((ROOT / path).read_bytes()).hexdigest() != extra['sha256']:
                raise ValueError('Additional component notice changed: ' + extra['path'])
            if extra['path'] not in (ROOT / 'THIRD_PARTY_NOTICES.md').read_text():
                raise ValueError('Additional notice is absent from human-readable notices')
    embedded_groups = data['embedded_notice_groups']
    embedded_paths = validate_embedded_groups(embedded_groups)
    forbidden = re.compile(r'UTvdb|UT_VDB|SESI_OPENVDB|__HDK_|DOMMY_DECLARE_VDB_GRID_DISPATCH|Author:\s*Lonnie Li')
    for name in data['owned_replacements']:
        if name in paths or name in embedded_paths or forbidden.search((ROOT / name).read_text()):
            raise ValueError('Owned replacement contains a removed legacy implementation')
    for name, phrases in data.get('owned_replacement_guards', {}).items():
        if name not in data['owned_replacements'] or not phrases:
            raise ValueError('Invalid owned-replacement guard')
        if any(phrase in (ROOT / name).read_text() for phrase in phrases):
            raise ValueError('Removed reference-code text reintroduced: ' + name)
    vendored = data['vendored_notice_headers']['libnoise']
    license_path = Path(vendored['license_file'])
    if license_path != Path('third_party/licenses/LGPL-2.1.txt') or not vendored['reference'].startswith('https://'):
        raise ValueError('Invalid vendored license/reference')
    if hashlib.sha256((ROOT / license_path).read_bytes()).hexdigest() != vendored['license_sha256']:
        raise ValueError('Vendored LGPL license text changed')
    vendored_paths = set()
    for record in vendored['modified_files']:
        path = Path(record['path'])
        if path.parts[:3] != ('third_party', 'libnoise', 'source') or '..' in path.parts or record['path'] in vendored_paths:
            raise ValueError('Invalid or duplicated vendored notice path')
        vendored_paths.add(record['path'])
        text = (ROOT / path).read_text()
        validate_vendored_header(record, text)
        # Verify that removing either the modification note, its date or the
        # upstream copyright is rejected; no files are mutated by this test.
        for phrase in ('2026-', 'dommyTool', 'Jason Bevins'):
            if phrase not in text:
                raise ValueError('Incomplete vendored notice: ' + record['path'])
            try:
                validate_vendored_header(record, text.replace(phrase, 'removed', 1))
            except ValueError:
                pass
            else:
                raise ValueError('Vendored notice negative control passed')
    for path in (ROOT / 'third_party/libnoise/source').rglob('*'):
        if path.is_file() and path.suffix in ('.h', '.cpp'):
            text = path.read_text()
            if re.search(r'Legacy dommyTool adaptation:|Local dommyTool maintenance \(', text) and str(path.relative_to(ROOT)) not in vendored_paths:
                raise ValueError('Unrecorded vendored modification notice: ' + str(path.relative_to(ROOT)))
    # Negative controls protect the gate itself without changing any files.
    sample = next(r for r in data['files'] if r['marker'])
    text = (ROOT / sample['path']).read_text()
    for bad in (text.replace('DOMMY_THIRD_PARTY_NOTICE_BEGIN', 'missing', 1),
                text.replace('DOMMY_THIRD_PARTY_NOTICE_BEGIN\n', 'DOMMY_THIRD_PARTY_NOTICE_BEGIN\n#define BAD 1\n', 1),
                text.replace('// Source comparison reference:', '// Missing source comparison reference:', 1),
                text.replace('Permission', 'Removed permission', 1)):
        try:
            validate_record(sample, bad)
        except ValueError:
            pass
        else:
            raise ValueError('Attribution negative control passed')
    multi_source_count = 0
    for record in data['files']:
        if not record.get('additional_sources'):
            continue
        multi_source_count += 1
        text = (ROOT / record['path']).read_text()
        for source in record['additional_sources']:
            reference = '// Source comparison reference: ' + source['upstream']
            bad_texts = [text.replace(reference, '// Missing source comparison reference:', 1),
                         text.replace(reference, reference + '\n' + reference, 1)]
            if source.get('archive_member'):
                member = '// Archive member: ' + source['archive_member']
                bad_texts.append(text.replace(member, '// Missing archive member:', 1))
            if source['component'] == 'jet':
                # Protect both the example header and the upstream MIT text.
                for phrase in ('Copyright (c) 2016 Doyub Kim',
                               'Copyright (c) 2017 Doyub Kim',
                               'The above copyright notice and this permission notice'):
                    if phrase not in text:
                        raise ValueError('Incomplete Jet notice')
                    bad_texts.append(text.replace(phrase, 'removed', 1))
            for bad in bad_texts:
                try:
                    validate_record(record, bad)
                except ValueError:
                    pass
                else:
                    raise ValueError('Additional-source negative control passed')
        incomplete = dict(record, additional_sources=[])
        try:
            validate_record(incomplete, text)
        except ValueError:
            pass
        else:
            raise ValueError('Unrecorded additional-source negative control passed')
    for name, group in embedded_groups.items():
        for record in group['files']:
            text = (ROOT / record['path']).read_text()
            # The notice gate deliberately does not freeze executable code.
            validate_embedded_record(record, text + '\n// Source-body positive control\n',
                                     group['notice_format'], group['required_phrases'])
            if group['notice_format'] == 'jc_license':
                validate_embedded_record(record, text.replace('USAGE:', 'Usage example:', 1),
                                         group['notice_format'], group['required_phrases'])
            for phrase in group['required_phrases']:
                try:
                    validate_embedded_record(record, text.replace(phrase, 'removed', 1),
                                             group['notice_format'], group['required_phrases'])
                except ValueError:
                    pass
                else:
                    raise ValueError('Embedded-notice negative control passed')
            try:
                validate_embedded_record(dict(record, notice_sha256='0' * 64), text,
                                         group['notice_format'], group['required_phrases'])
            except ValueError:
                pass
            else:
                raise ValueError('Embedded fingerprint negative control passed')
        # Exercise the complete inventory gate, not just the notice digest.
        missing_group = dict(embedded_groups)
        del missing_group[name]
        bad_groups = [missing_group]
        for records in (group['files'][1:], group['files'] + group['files'][:1]):
            bad = dict(embedded_groups)
            bad[name] = dict(group, files=records)
            bad_groups.append(bad)
        if group.get('license_file'):
            bad = dict(embedded_groups)
            bad[name] = dict(group, license_sha256='0' * 64)
            bad_groups.append(bad)
        for bad in bad_groups:
            try:
                validate_embedded_groups(bad)
            except ValueError:
                pass
            else:
                raise ValueError('Embedded inventory negative control passed')
    print('Third-party notice coverage: {} files, {} components; negative controls passed'.format(
        len(paths), len(components)))
    print('Multi-component source files: {}; negative controls passed'.format(multi_source_count))
    print('Vendored modification/license headers: {} libnoise files; negative controls passed'.format(len(vendored_paths)))
    print('Embedded notice coverage: {} files, {} groups; negative controls passed'.format(
        len(embedded_paths), len(embedded_groups)))


if __name__ == '__main__':
    main()
