import os
import tempfile
import unittest

from playlist import read_page


class PlaylistTests(unittest.TestCase):
    def setUp(self):
        handle, self.path = tempfile.mkstemp(suffix=".m3u")
        os.close(handle)

    def tearDown(self):
        os.unlink(self.path)

    def write(self, text):
        with open(self.path, "wb") as handle:
            handle.write(text.encode("utf-8"))

    def test_reads_titles_and_skips_player_options(self):
        self.write(
            '#EXTM3U\n'
            '#EXTINF:-1 group-title="Publicos",TV Senado\n'
            '#EXTVLCOPT:network-caching=1000\n'
            'https://example.org/senado.m3u8\n'
            '#EXTINF:-1,Canal Educação\n'
            'http://example.org/educacao.m3u8\n'
            '#EXTINF:-1,Arquivo local\n'
            'file:///etc/passwd\n'
        )
        channels, more = read_page(self.path, 0, 200)
        self.assertEqual(channels, [
            ("TV Senado", "https://example.org/senado.m3u8"),
            ("Canal Educação", "http://example.org/educacao.m3u8"),
        ])
        self.assertFalse(more)

    def test_pages_without_loading_the_entire_list(self):
        self.write("#EXTM3U\n" + "".join(
            "#EXTINF:-1,Canal %d\nhttp://example.org/%d.m3u8\n" % (i, i)
            for i in range(5)
        ))
        first, more = read_page(self.path, 0, 2)
        second, more_second = read_page(self.path, 1, 2)
        third, more_third = read_page(self.path, 2, 2)
        self.assertEqual([name for name, _ in first], ["Canal 0", "Canal 1"])
        self.assertTrue(more)
        self.assertEqual([name for name, _ in second], ["Canal 2", "Canal 3"])
        self.assertTrue(more_second)
        self.assertEqual([name for name, _ in third], ["Canal 4"])
        self.assertFalse(more_third)


if __name__ == "__main__":
    unittest.main()
