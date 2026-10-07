#!/usr/bin/env python3
"""Run a CTest command; remove its generated artifacts only when it passes."""
import argparse
import shutil
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', required=True, type=Path)
parser.add_argument('--work', required=True, type=Path)
parser.add_argument('command', nargs=argparse.REMAINDER)
args = parser.parse_args()
work, build = args.work.resolve(), args.build.resolve()
relative = work.relative_to(build)
if not relative.parts or relative.parts[0] not in {'examples', 'test-output', 'cli-test', 'card-bake-test', 'card-growth-test', 'development-test', 'export-test', 'native-api-test'}:
    parser.error('Cleanup target must be a designated test artifact directory under the build tree')
command = args.command[1:] if args.command[:1] == ['--'] else args.command
if not command:
    parser.error('Test command is required')
result = subprocess.run(command)
if result.returncode == 0 and work.is_dir():
    shutil.rmtree(work)
raise SystemExit(result.returncode)
