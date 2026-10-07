#!/usr/bin/env python3
"""Create a disposable project under out/ and run the real Unity editor integration test."""
import argparse
import json
import re
import shutil
import subprocess
from pathlib import Path
from package_unity_recipe import package

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--editor', required=True, type=Path)
    parser.add_argument('--package', required=True, type=Path)
    parser.add_argument('--recipe', required=True, type=Path)
    parser.add_argument('--project', type=Path, default=ROOT / 'out/unity-integration-test')
    parser.add_argument('--name', default='strand')
    parser.add_argument('--card-bake')
    parser.add_argument('--reuse-mature-bake', action='store_true', help='Test prepared CardBake growth, shared textures and cache invalidation')
    parser.add_argument('--minerals', action='store_true', help='Test static mineral generation, seed changes, buffers and two rendered views')
    parser.add_argument('--urp-version', help='Match the installed editor, e.g. 17.4.0 for Unity 6000.4')
    args = parser.parse_args()
    if args.reuse_mature_bake and not args.card_bake:
        parser.error('--reuse-mature-bake requires --card-bake')
    if args.minerals and (args.card_bake or args.reuse_mature_bake):
        parser.error('--minerals cannot combine with CardBake testing')
    project = args.project.resolve()
    if ROOT / 'out' not in project.parents or not re.fullmatch(r'[A-Za-z0-9_-]+', args.name):
        parser.error('Use a project under out/ and a simple sample name')
    if project.exists() and any(project.iterdir()):
        parser.error('Choose an empty disposable project directory; existing projects are preserved')
    editor_version = subprocess.check_output([str(args.editor.resolve()), '-version'], text=True).strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+[abfp]\d+', editor_version):
        parser.error('Could not read a supported Unity editor version')
    (project / 'Assets/Editor').mkdir(parents=True, exist_ok=True)
    (project / 'Packages').mkdir()
    (project / 'ProjectSettings').mkdir()
    (project / 'ProjectSettings/ProjectVersion.txt').write_text('m_EditorVersion: ' + editor_version + '\n', encoding='utf-8')
    package(args.recipe, project / 'Assets/StreamingAssets/FoliageUtil' / args.name)
    shutil.copy2(ROOT / 'tests/unity/FoliageSmoke.cs', project / 'Assets/Editor/FoliageSmoke.cs')
    shutil.copy2(ROOT / 'tests/unity/FoliageCardGrowthSmoke.cs', project / 'Assets/Editor/FoliageCardGrowthSmoke.cs')
    shutil.copy2(ROOT / 'tests/unity/FoliageMineralSmoke.cs', project / 'Assets/Editor/FoliageMineralSmoke.cs')
    dependencies = {'com.szhlopp.foliageutil': 'file:' + args.package.resolve().as_posix()}
    method = 'FoliageCardGrowthSmoke.Run' if args.reuse_mature_bake else 'FoliageSmoke.Run'
    if args.minerals:
        method = 'FoliageMineralSmoke.Run'
    if args.urp_version:
        dependencies['com.unity.render-pipelines.universal'] = args.urp_version
        shutil.copy2(ROOT / 'tests/unity/FoliageUrpSmoke.cs', project / 'Assets/Editor/FoliageUrpSmoke.cs')
        method = 'FoliageUrpSmoke.Run'
    (project / 'Packages/manifest.json').write_text(json.dumps({'dependencies': dependencies}, indent=2), encoding='utf-8')
    command = [str(args.editor.resolve()), '-batchmode', '-accept-apiupdate', '-projectPath', str(project), '-executeMethod', method, '-foliageSample', args.name, '-logFile', str(project / 'smoke.log')]
    if args.card_bake:
        command += ['-foliageBake', args.card_bake]
    if args.reuse_mature_bake:
        command += ['-foliageReuseBake']
    if args.minerals:
        command += ['-foliageMinerals']
    completed = subprocess.run(command)
    # A first dependency import can migrate Unity APIs and then request a reload.
    # Retry only that explicit case, keeping the original compiler/import log.
    if completed.returncode and '[API Updater] Updated Files:' in (project / 'smoke.log').read_text(encoding='utf-8', errors='replace'):
        (project / 'smoke.log').rename(project / 'initial-import.log')
        completed = subprocess.run(command)
    completed.check_returncode()
    if 'FOLIAGE_UNITY_SMOKE_PASSED' not in (project / 'smoke.log').read_text(encoding='utf-8', errors='replace'):
        raise RuntimeError('Unity exited without completing the integration test; inspect smoke.log')
    print(project / 'smoke.log')


if __name__ == '__main__':
    main()
