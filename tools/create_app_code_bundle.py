"""Build the allowlisted in-place code update for Az IPTV."""

from pathlib import Path
from xml.etree import ElementTree
from zipfile import ZIP_DEFLATED, ZipFile


root = Path(__file__).resolve().parent
source = root / 'plugin.video.az_iptv'
version = ElementTree.parse(source / 'addon.xml').getroot().attrib['version']
release = root / 'github-release' / 'az-iptv'
release.mkdir(parents=True, exist_ok=True)
archive = release / ('app-code-%s.zip' % version)
names = ('addon.xml', 'plugin_catalog.py', 'playlist.py',
         'app_update.py', 'default.py')
with ZipFile(archive, 'w', ZIP_DEFLATED) as bundle:
    for name in names:
        bundle.write(source / name, name)
print(archive)
