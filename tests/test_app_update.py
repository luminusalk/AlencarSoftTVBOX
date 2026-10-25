import hashlib
import io
import os
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'plugin.video.az_iptv'))
from app_update import AppUpdateError, install_update, installed_version, newer_version


class AppUpdateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = self.temp.name
        for name, content in [('addon.xml', '<?xml version="1.0"?><addon version="0.2.0"/>'),
                              ('default.py', 'old code'),
                              ('plugin_catalog.py', 'old catalog'),
                              ('playlist.py', 'old playlist'),
                              ('app_update.py', 'old updater')]:
            with open(os.path.join(self.root, name), 'wb') as handle:
                handle.write(content.encode('utf-8'))
        os.mkdir(os.path.join(self.root, 'lists'))
        with open(os.path.join(self.root, 'lists', 'mine.m3u'), 'wb') as handle:
            handle.write(b'personal list')

    def tearDown(self):
        self.temp.cleanup()

    def bundle(self, version='0.3.0', unsafe=False):
        result = io.BytesIO()
        with zipfile.ZipFile(result, 'w', zipfile.ZIP_DEFLATED) as bundle:
            bundle.writestr('addon.xml', '<addon version="%s"/>' % version)
            for name in ('default.py', 'plugin_catalog.py', 'playlist.py', 'app_update.py'):
                bundle.writestr(name, 'new ' + name)
            if unsafe:
                bundle.writestr('../escaped.py', 'danger')
        return result.getvalue()

    def spec(self, raw, version='0.3.0'):
        return {'version': version, 'url': 'https://example.org/app.zip',
                'sha256': hashlib.sha256(raw).hexdigest()}

    def test_installs_verified_code_and_preserves_personal_lists(self):
        raw = self.bundle()
        install_update(self.root, self.spec(raw), lambda url: raw)
        with open(os.path.join(self.root, 'default.py'), 'rb') as handle:
            self.assertEqual(handle.read(), b'new default.py')
        with open(os.path.join(self.root, 'lists', 'mine.m3u'), 'rb') as handle:
            self.assertEqual(handle.read(), b'personal list')

    def test_rejects_bad_digest_and_unsafe_zip(self):
        raw = self.bundle()
        with self.assertRaises(AppUpdateError):
            install_update(self.root, self.spec(raw), lambda url: raw + b'changed')
        unsafe = self.bundle(unsafe=True)
        with self.assertRaises(AppUpdateError):
            install_update(self.root, self.spec(unsafe), lambda url: unsafe)
        with open(os.path.join(self.root, 'default.py'), 'rb') as handle:
            self.assertEqual(handle.read(), b'old code')

    def test_version_comparison_and_rollback_protection(self):
        self.assertTrue(newer_version('0.3.0', '0.2.9'))
        self.assertFalse(newer_version('0.2.0', '0.2.0'))
        raw = self.bundle('0.1.0')
        with self.assertRaises(AppUpdateError):
            install_update(self.root, self.spec(raw, '0.1.0'), lambda url: raw)

    def test_release_package_upgrades_from_030_to_031(self):
        base = os.path.dirname(__file__)
        package = os.path.join(base, 'serve', 'plugin.video.az_iptv-0.3.0.zip')
        release = os.path.join(base, 'github-release', 'az-iptv',
                               'app-code-0.3.1.zip')
        with zipfile.ZipFile(package) as bundle:
            bundle.extractall(self.root)
        app_root = os.path.join(self.root, 'plugin.video.az_iptv')
        with open(release, 'rb') as handle:
            raw = handle.read()
        self.assertEqual(installed_version(app_root), '0.3.0')
        install_update(app_root, self.spec(raw, '0.3.1'), lambda url: raw)
        self.assertEqual(installed_version(app_root), '0.3.1')


if __name__ == '__main__':
    unittest.main()
