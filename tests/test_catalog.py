import base64
import json
import os
import sys
import tempfile
import unittest

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'plugin.video.az_iptv'))
from plugin_catalog import CatalogError, CatalogStore, canonical_payload, validate_manifest


class CatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        key_path = os.path.join(os.path.dirname(__file__), 'catalog_signing_private.pem')
        with open(key_path, 'rb') as handle:
            cls.key = serialization.load_pem_private_key(handle.read(), password=None)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.store = CatalogStore(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def manifest(self, revision=1):
        payload = {
            'schema': 1,
            'revision': revision,
            'lists': [{'id': 'publicos', 'title': 'Canais publicos', 'channels': [
                {'name': 'Exemplo', 'url': 'https://example.org/video.mp4'}]}],
        }
        signature = self.key.sign(canonical_payload(payload), padding.PKCS1v15(),
                                  hashes.SHA256())
        return json.dumps({'payload': payload,
                           'signature': base64.b64encode(signature).decode('ascii')}).encode('utf-8')

    def test_accepts_signed_manifest_and_rejects_tampering(self):
        raw = self.manifest()
        self.assertEqual(validate_manifest(raw)['revision'], 1)
        changed = raw.replace(b'Exemplo', b'Atacado')
        with self.assertRaises(CatalogError):
            validate_manifest(changed)

    def test_update_keeps_last_good_catalog_on_failure(self):
        first = self.manifest(1)
        self.store.update(lambda: first)
        self.assertEqual(self.store.load()['revision'], 1)
        with self.assertRaises(CatalogError):
            self.store.update(lambda: b'broken')
        self.assertEqual(self.store.load()['revision'], 1)

    def test_rejects_revision_rollback_and_unsafe_urls(self):
        self.store.update(lambda: self.manifest(2))
        with self.assertRaises(CatalogError):
            self.store.update(lambda: self.manifest(1))
        payload = {'schema': 1, 'revision': 3, 'lists': [
            {'id': 'bad', 'title': 'Bad', 'channels': [
                {'name': 'Bad', 'url': 'file:///etc/passwd'}]}]}
        signature = self.key.sign(canonical_payload(payload), padding.PKCS1v15(),
                                  hashes.SHA256())
        raw = json.dumps({'payload': payload,
                          'signature': base64.b64encode(signature).decode('ascii')}).encode('utf-8')
        with self.assertRaises(CatalogError):
            validate_manifest(raw)

    def test_limits_manifest_size(self):
        with self.assertRaises(CatalogError):
            validate_manifest(b' ' * (512 * 1024 + 1))


if __name__ == '__main__':
    unittest.main()
