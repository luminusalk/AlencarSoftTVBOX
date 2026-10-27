import sys
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from scan_live_channels import classify, entries, is_ts
from finalize_live_channels import identity, quality


class ChannelScannerTests(unittest.TestCase):
    def test_live_categories_and_vod(self):
        self.assertEqual(classify("RecordTV Campos Dos Goytacazes HD", "CANAIS = RECORD"),
                         "Canais Abertos")
        self.assertEqual(classify("NBA TV HD", "BR | ESPORTES"), "Esportes")
        self.assertEqual(classify("GloboNews HD", "BR | NOTICIAS"), "Notícias")
        self.assertIsNone(classify("Episodio 4", "Series | Netflix"))

    def test_transport_stream_requires_sync_packets(self):
        ts = bytearray(564)
        ts[0] = ts[188] = ts[376] = 0x47
        self.assertTrue(is_ts(ts))
        self.assertFalse(is_ts(b"<html>OK</html>"))

    def test_cp1252_playlist_and_hd_fhd_identity(self):
        with TemporaryDirectory() as directory:
            path = Path(directory) / "latin.m3u8"
            path.write_bytes(b'#EXTM3U\n#EXTINF:-1 group-title="BR | NOTICIAS",'
                             b'Jovem Pan Not\xedcias HD\nhttp://example.com/live.ts\n')
            found = list(entries(path, "test"))
            self.assertEqual(len(found), 1)
            self.assertEqual(found[0].category, "Notícias")
            self.assertEqual(found[0].name, "Jovem Pan Notícias HD")
        self.assertEqual(identity("Band SP HD"), identity("Band SP FHD"))
        self.assertEqual(quality("Band SP FHD"), "FHD")


if __name__ == "__main__":
    unittest.main()
