"""Sign the editable catalog for the Azamerica add-on.

The private key stays on the development PC. Only catalog.json and demo.mp4
are copied to the GitHub release directory.
"""

import base64
import json
import shutil
from pathlib import Path
from xml.etree import ElementTree

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding


root = Path(__file__).resolve().parent
source = json.loads((root / 'catalog_source.json').read_text(encoding='utf-8'))
with (root / 'catalog_signing_private.pem').open('rb') as handle:
    key = serialization.load_pem_private_key(handle.read(), password=None)

canonical = json.dumps(source, sort_keys=True, separators=(',', ':'),
                       ensure_ascii=True).encode('ascii')
signature = key.sign(canonical, padding.PKCS1v15(), hashes.SHA256())
release = root / 'github-release' / 'az-iptv'
release.mkdir(parents=True, exist_ok=True)
(release / 'catalog.json').write_text(json.dumps({
    'payload': source,
    'signature': base64.b64encode(signature).decode('ascii'),
}, ensure_ascii=True, indent=2) + '\n', encoding='utf-8')
shutil.copyfile(root / 'plugin.video.az_iptv' / 'demo.mp4', release / 'demo.mp4')
shutil.copyfile(root / 'catalog_source.json', release / 'catalog_source.json')
app_source = root / 'plugin.video.az_iptv'
version = ElementTree.parse(app_source / 'addon.xml').getroot().attrib['version']
shutil.copyfile(root / 'serve' / ('plugin.video.az_iptv-%s.zip' % version),
                release / ('plugin.video.az_iptv-%s.zip' % version))
source_release = release / 'app'
source_release.mkdir(exist_ok=True)
for name in ('addon.xml', 'default.py', 'plugin_catalog.py',
             'app_update.py', 'playlist.py'):
    shutil.copyfile(app_source / name, source_release / name)
print(release)
