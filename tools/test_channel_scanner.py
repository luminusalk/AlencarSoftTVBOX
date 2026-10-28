import sys
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
from scan_live_channels import Entry, classify, entries, is_ts
from finalize_live_channels import identity, quality, read
from recheck_live_sequence import check


class ChannelScannerTests(unittest.TestCase):
    def test_final_catalog_excludes_unverified_uhd(self):
        final = Path(__file__).resolve().parents[1] / "AlencarSoftTVBOX-Clang/iptv.m3u8"
        self.assertFalse(any(token in name.upper() for _, name, _ in read(final)
                             for token in (" 4K", " UHD")))

    def test_linear_movie_kids_and_documentary_channels(self):
        self.assertEqual(classify("HBO HD", "BR | FILMES E SERIES"), "Filmes e Séries")
        self.assertEqual(classify("Telecine Premium HD", "Filmes"), "Filmes e Séries")
        self.assertEqual(classify("Disney Channel HD", "BR | INFANTIL"), "Infantil")
        self.assertEqual(classify("Discovery Science HD", "Documentarios"), "Documentários")
        self.assertIsNone(classify("HBO S01E03", "Series"))
        self.assertIsNone(classify("Soldado Universal", "Filmes"))

    def test_sequence_recheck_rejects_regression_and_reused_window(self):
        item = Entry("Teste", "http://example.invalid/live.m3u8", "", "test", 0, "Variedades")
        with patch("recheck_live_sequence.probe", return_value=(True, "TS")), patch("recheck_live_sequence.time.sleep"), patch("recheck_live_sequence.snapshot",
                side_effect=[(100, ["a.ts"]), (90, ["b.ts"])]):
            self.assertEqual(check(item), (False, "sequence_regression"))
        with patch("recheck_live_sequence.probe", return_value=(True, "TS")), patch("recheck_live_sequence.time.sleep"), patch("recheck_live_sequence.snapshot",
                side_effect=[(100, ["a.ts"]), (101, ["a.ts"])]):
            self.assertEqual(check(item), (False, "sequence_advanced_same_segments"))

    def test_direct_ts_recheck_rejects_static_replay(self):
        item = Entry("Teste", "http://example.invalid/live.ts", "", "test", 0, "Esportes")
        first = bytearray(65536)
        first[0] = first[188] = first[376] = 0x47
        with patch("recheck_live_sequence.time.sleep"), patch(
                "recheck_live_sequence.fetch", side_effect=[
                    (200, "video/mp2t", bytes(first), item.url),
                    (200, "video/mp2t", bytes(first), item.url)]):
            self.assertEqual(check(item), (False, "direct_TS_same_prefix_7s"))

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
        self.assertEqual(identity("CNN BRASIL HD ¹"), identity("CNN BRASIL HD ²"))
        self.assertEqual(identity("CNN BRASIL HD ¹"), identity("CNN BRASIL FHD H.265"))
        self.assertEqual(quality("Band SP FHD"), "FHD")


if __name__ == "__main__":
    unittest.main()
