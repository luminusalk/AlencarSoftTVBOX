from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


root = Path(__file__).resolve().parent
source = root / "plugin.video.rea_probe"
archive = root / "serve" / "plugin.video.rea_probe-0.1.1.zip"

with ZipFile(archive, "w", compression=ZIP_DEFLATED) as bundle:
    for filename in ("addon.xml", "default.py"):
        bundle.write(source / filename, arcname=f"plugin.video.rea_probe/{filename}")

print(archive)
