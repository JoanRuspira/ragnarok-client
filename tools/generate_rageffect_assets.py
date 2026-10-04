"""Resolve captured effect asset references against installed GRFs; never builds.

Reference: https://github.com/lukasrmattos/ragnarok_effects_list
Only numeric IDs and asset filenames are emitted, not descriptions or media.
Pass GRFs in priority order, as listed in DATA.INI (patches first).
"""
from pathlib import Path
import argparse
import collections
import hashlib
import json
import re
import struct
import zlib


class Grf:
    def __init__(self, path):
        self.path = Path(path)
        self.entries = {}
        with self.path.open('rb') as stream:
            header = stream.read(46)
            table, low, high, version = struct.unpack_from('<4I', header, 30)
            if header[:15] != b'Master of Magic' or version != 0x200:
                raise ValueError(f'Unsupported GRF: {self.path}')
            stream.seek(46 + table)
            compressed, plain = struct.unpack('<2I', stream.read(8))
            data = zlib.decompress(stream.read(compressed))
            if len(data) != plain:
                raise ValueError(f'Invalid GRF index: {self.path}')
        cursor = 0
        for _ in range(high - low - 7):
            end = data.index(0, cursor)
            name = data[cursor:end].decode('cp949', errors='surrogateescape')
            self.entries[name.lower()] = struct.unpack_from('<3IBI', data, end + 1)
            cursor = end + 18

    def read(self, name):
        compressed, _, plain, flags, offset = self.entries[name]
        if flags & 6:
            raise ValueError(f'Encrypted asset: {name}')
        with self.path.open('rb') as stream:
            stream.seek(offset + 46)
            data = zlib.decompress(stream.read(compressed))
        if len(data) != plain:
            raise ValueError(f'Invalid GRF asset: {name}')
        return data


def normalize(name):
    name = name.lower().replace('/', '\\')
    for prefix in ('data\\texture\\', 'texture\\'):
        if name.startswith(prefix):
            return name[len(prefix):]
    return name


def read_str(data):
    magic, version, fps, frames, layers = struct.unpack_from('<5I', data)
    if magic != 0x4D525453 or version != 148 or fps <= 0:
        raise ValueError('Invalid STR header')
    cursor = 36
    textures = []
    for _ in range(layers):
        count, = struct.unpack_from('<I', data, cursor)
        cursor += 4
        names = []
        for _ in range(count):
            raw = data[cursor:cursor + 128]
            if len(raw) != 128:
                raise ValueError('Truncated STR texture')
            names.append(raw.split(b'\0')[0].decode('cp949', errors='replace'))
            cursor += 128
        keys, = struct.unpack_from('<I', data, cursor)
        cursor += 4
        if keys > (len(data) - cursor) // 124:
            raise ValueError('Truncated STR keys')
        cursor += keys * 124
        textures.extend(names)
    if cursor != len(data):
        raise ValueError('Invalid STR size')
    return textures


def resolve(reference, archives):
    assets = {}
    for archive in reversed(archives):
        assets.update({name: archive for name in archive.entries})
    prefix = 'data\\texture\\effect\\'
    signatures = collections.defaultdict(list)
    texture_sets = {}
    for name, archive in assets.items():
        if not name.startswith(prefix) or not name.endswith('.str') or '\\mineffect\\' in name:
            continue
        textures = set()
        for raw in read_str(archive.read(name)):
            raw = raw.lower().replace('/', '\\')
            candidates = [name.rsplit('\\', 1)[0] + '\\' + raw,
                          prefix + raw, 'data\\texture\\' + raw, 'data\\' + raw, raw]
            actual = next((path for path in candidates if path in assets), prefix + raw)
            textures.add(normalize(actual))
        relative = name[len(prefix):]
        if textures:
            signatures[frozenset(textures)].append(relative)
            texture_sets[relative] = textures
    result = {}
    for entry in reference:
        textures = frozenset(normalize(name) for name in entry['assets'])
        exact = signatures.get(textures, [])
        files = []
        if len(exact) == 1:
            files = exact
        elif len(exact) > 1:
            named = [name for name in exact if '\\' in name
                     and Path(name.replace('\\', '/')).stem == name.rsplit('\\', 2)[-2]]
            if len(named) == 1:
                files = named
        else:
            directories = {name.rsplit('\\', 1)[0] for name in textures if '\\' in name} - {'effect'}
            for directory in sorted(directories):
                relative = directory.removeprefix('effect\\')
                candidates = [name for name in texture_sets if name.rsplit('\\', 1)[0] == relative]
                named = [name for name in candidates
                         if Path(name.replace('\\', '/')).stem == relative.rsplit('\\', 1)[-1]]
                if len(named) == 1:
                    files.append(named[0])
                elif len(candidates) == 1:
                    files.append(candidates[0])
            if not directories or len(files) != len(directories):
                files = []
                if len(textures) >= 2:
                    supersets = [name for name, names in texture_sets.items() if textures <= names]
                    if len(supersets) == 1:
                        files = supersets
        if files:
            result[entry['id']] = files
    return result, assets


def cpp_string(value):
    # Archive paths use Korean ANSI bytes, independent of the compiler's encoding.
    return '"' + ''.join('\\\\' if byte == 92 else '\\"' if byte == 34 else chr(byte)
                         if 32 <= byte < 127 else f'\\{byte:03o}'
                         for byte in value.encode('cp949')) + '"'


def sprite_catalog(raw, archives):
    # Read factual fields only from roBrowser's public effect table.
    source = raw.decode('utf-8')
    starts = list(re.finditer(r'^\s{2,}(\d+):\s*\[', source, re.M))
    assets = {name for archive in archives for name in archive.entries}
    prefix = 'data\\sprite\\\uc774\ud329\ud2b8\\'
    rows = []
    for index, entry in enumerate(starts):
        block = source[entry.end():starts[index + 1].start() if index + 1 < len(starts) else len(source)]
        block = re.sub(r'//[^\n]*|/\*.*?\*/', '', block, flags=re.S)
        for component in re.findall(r'\{([^{}]+)\}', block):
            if not re.search(r"type\s*:\s*'SPR'", component):
                continue
            filename = re.search(r"file\s*:\s*'([^']+)'", component)
            if not filename:
                continue
            name = filename[1]
            try:
                name = name.encode('latin1').decode('cp949')
            except (UnicodeEncodeError, UnicodeDecodeError):
                pass
            stem = (prefix + name).lower()
            if stem + '.spr' not in assets or stem + '.act' not in assets:
                continue
            head = bool(re.search(r'head\s*:\s*true', component))
            direction = bool(re.search(r'direction\s*:\s*true', component))
            # SPR/ACT have no registered directory prefix; keep the full GRF key.
            rows.append((int(entry[1]), stem, head, direction))
    output = ['// Generated by tools/generate_rageffect_assets.py; do not edit.',
              '// Sprite IDs: https://github.com/vthibault/roBrowser/blob/master/src/DB/Effects/EffectTable.js',
              '// Reference SHA256: ' + hashlib.sha256(raw).hexdigest(),
              'static constexpr RagEffectSpriteEntry kRagEffectSprites[] = {']
    for effect, stem, head, direction in sorted(rows):
        output.append(f'    {{ {effect}, {cpp_string(stem)}, {str(head).lower()}, {str(direction).lower()} }},')
    output += ['};', '']
    return '\n'.join(output), len(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--grf', type=Path, action='append', required=True)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--sprite-reference', type=Path)
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1]
                        / 'src/world/RagEffectAssets.generated.inc')
    args = parser.parse_args()
    raw = args.reference.read_bytes()
    archives = [Grf(path) for path in args.grf]
    mappings, _ = resolve(json.loads(raw), archives)
    rows = ['// Generated by tools/generate_rageffect_assets.py; do not edit.',
            '// Asset references: https://github.com/lukasrmattos/ragnarok_effects_list',
            '// Reference SHA256: ' + hashlib.sha256(raw).hexdigest(),
            '// Matched against installed GRFs; ambiguous texture-only matches are omitted.',
            'static constexpr RagEffectAssetEntry kRagEffectAssets[] = {']
    for effect_id, files in sorted(mappings.items()):
        rows.append(f'    {{ {effect_id}, {cpp_string("|".join(files))} }},')
    rows += ['};', '']
    output = '\n'.join(rows)
    if args.check:
        if args.output.read_text(encoding='utf-8') != output:
            raise SystemExit('Generated effect catalog is out of date')
    else:
        args.output.write_text(output, encoding='utf-8')
    print(f'{len(mappings)} effect IDs resolve to {sum(map(len, mappings.values()))} STR animations')
    if args.sprite_reference:
        sprites, count = sprite_catalog(args.sprite_reference.read_bytes(), archives)
        sprite_output = args.output.with_name('RagEffectSprites.generated.inc')
        if args.check:
            if sprite_output.read_text(encoding='utf-8') != sprites:
                raise SystemExit('Generated sprite catalog is out of date')
        else:
            sprite_output.write_text(sprites, encoding='utf-8')
        print(f'{count} sprite effect IDs resolve to installed SPR/ACT pairs')


if __name__ == '__main__':
    main()
