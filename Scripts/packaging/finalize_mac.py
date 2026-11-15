"""Give the verified standalone bundle TNES branding and a local ad-hoc signature."""
import plistlib
import shutil
import subprocess
import sys
from pathlib import Path

source, destination, branding, licenses = map(Path, sys.argv[1:5])
assert source.suffix == '.app' and (source/'Contents/Info.plist').is_file(), source
assert destination.suffix == '.app' and not destination.exists(), destination
stage = destination.with_name('.' + destination.stem + '-preparing.app')
assert not stage.exists(), stage
subprocess.run(['cp', '-cR', str(source), str(stage)], check=True)
info_path = stage/'Contents/Info.plist'
with info_path.open('rb') as stream:
    info = plistlib.load(stream)
info.update(CFBundleName='TNES', CFBundleDisplayName='TNES',
            CFBundleIdentifier='it.rowtech.tnes', CFBundleIconFile='TNES.icns',
            CFBundleShortVersionString='0.1.0', CFBundleVersion='0.1.0')
with info_path.open('wb') as stream:
    plistlib.dump(info, stream)
resources = stage/'Contents/Resources'
resources.mkdir(exist_ok=True)
shutil.copy2(branding/'TNES.icns', resources/'TNES.icns')
shutil.copy2(licenses, resources/'TNES-LICENSES.md')
# Keep only current validated audio; do not retain stale files from an older staging pass.
audio_source = licenses.parent/'Content/Audio'
audio_destination = stage/'Contents/UE/ClaraDayOne/Content/Audio'
assert audio_source.is_dir() and audio_destination.parent.is_dir()
if audio_destination.exists():
    shutil.rmtree(audio_destination)
subprocess.run(['cp', '-cR', str(audio_source), str(audio_destination)], check=True)
subprocess.run(['xattr', '-cr', str(stage)], check=True)
subprocess.run(['codesign', '--force', '--deep', '--sign', '-', '--timestamp=none', str(stage)], check=True)
subprocess.run(['codesign', '--verify', '--deep', '--strict', '--verbose=2', str(stage)], check=True)
stage.rename(destination)
print(destination)
