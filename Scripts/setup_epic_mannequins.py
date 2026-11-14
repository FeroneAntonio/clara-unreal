#!/usr/bin/env python3
"""Install required Epic templates from the contributor's licensed UE installation."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys


def digest(path):
    return hashlib.sha256(path.read_bytes()).digest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine-root', required=True, type=Path, help='UE_5.8 installation directory, containing Engine and Templates')
    parser.add_argument('--check', action='store_true', help='Validate sources and destinations without copying')
    parser.add_argument('--force', action='store_true', help='Replace local template assets that differ from this UE installation')
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    manifest = project / 'Content/Characters/mannequin-template-manifest.json'
    assets = [entry['asset'] for entry in json.loads(manifest.read_text(encoding='utf-8'))['assets']]
    assets.append('/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd')
    sources = args.engine_root.expanduser().resolve() / 'Templates/TemplateResources/High/Characters/Content'
    if not (args.engine_root / 'Engine').is_dir():
        raise RuntimeError('Engine directory missing; --engine-root must point to UE_5.8.')
    jobs = []
    for asset in sorted(set(assets)):
        prefix = '/Game/Characters/'
        if not asset.startswith(prefix):
            raise RuntimeError(f'Unexpected manifest package: {asset}')
        relative = Path(asset[len(prefix):] + '.uasset')
        if '..' in relative.parts:
            raise RuntimeError('Unsafe path in asset manifest.')
        source = sources / relative
        target = project / 'Content/Characters' / relative
        if not source.is_file():
            raise RuntimeError(f'Template missing: {relative}. Install the UE 5.8 template/feature-pack content through Epic Games Launcher.')
        if target.exists() and digest(source) != digest(target) and not args.force:
            raise RuntimeError(f'Local asset differs: {relative}. Preserve your edits or rerun with --force intentionally.')
        jobs.append((source, target))
    if args.check:
        print(f'OK: {len(jobs)} required template files available; no files copied.')
        return
    copied = 0
    for source, target in jobs:
        if target.is_file() and digest(source) == digest(target):
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        copied += 1
    print(f'OK: {len(jobs)} required Epic template assets present ({copied} copied).')
    print('These local assets retain Epic terms and remain excluded from Git.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        print(f'Setup failed: {error}', file=sys.stderr)
        sys.exit(1)
