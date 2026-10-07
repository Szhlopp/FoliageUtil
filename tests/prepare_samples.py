#!/usr/bin/env python3
"""Create tiny diagnostic PNG fixtures for geometry/export tests, never production art."""
import argparse
import json
import struct
import sys
import zlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from prepare_samples import SAMPLES, prepare_sources, load_manifest, MARKER


def png(path, width, height, kind):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
    path.parent.mkdir(parents=True, exist_ok=True)
    color = (128, 128, 255, 255) if 'normal' in kind else (255, 180, 0, 255) if 'roughness' in kind else (80, 155, 50, 255)
    row = bytearray()
    for x in range(width):
        row.extend(color if 'normal' in kind or 'roughness' in kind or x % 16 < 12 else (*color[:3], 0))
    raw = (b'\0' + row) * height
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    prepare_sources(args.out, fixture=True)
    for job in load_manifest()['materials']:
        path = args.out / 'assets' / job['recipe']
        doc = json.loads(path.read_text())
        for name, spec in doc['outputs'].items():
            output = path.parent / name
            if isinstance(spec, dict) and spec.get('type') == 'spritesheet':
                columns, rows, count, padding = spec['columns'], spec['rows'], spec['count'], spec['padding']
                cw, ch = 64, 64
                w, h = cw * columns, ch * rows
                source = json.loads((path.parent / spec['source']).read_text())
                files = []
                for image_name in source['outputs']:
                    if Path(image_name).suffix != '.png':
                        continue
                    file = Path(output.stem + '.assets') / image_name
                    png(output.parent / file, w, h, image_name)
                    files.append({'file': file.as_posix()})
                cells = []
                for i in range(count):
                    col, row = i % columns, i // columns
                    pixels = [col * cw + padding, row * ch + padding, (col + 1) * cw - padding, (row + 1) * ch - padding]
                    cells.append({'index': i, 'column': col, 'row': row, 'pixels': pixels, 'uv_rect': [n / (w if k % 2 == 0 else h) for k, n in enumerate(pixels)]})
                atlas = {'version': 1, 'format': 'texutil-spritesheet', 'uv_origin': 'top-left', 'columns': columns, 'rows': rows, 'count': count, 'padding': padding, 'cell_size': [cw, ch], 'content_size': [cw-2*padding, ch-2*padding], 'image_size': [w, h], 'cells': cells, 'images': files}
                output.write_text(json.dumps(atlas, indent=2) + '\n')
            elif output.suffix == '.png':
                png(output, 32, 32, name)
    (args.out / 'FIXTURES_ONLY.txt').write_text('Diagnostic geometry/export test fixtures. Not TexUtil renders or visual sample assets.\n')
    print(f'Prepared diagnostic sample fixtures in {args.out}')


if __name__ == '__main__':
    main()
