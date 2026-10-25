import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'plugin.video.az_iptv'))
from hls_bridge import HlsSequence, parse_media_playlist


class HlsBridgeTests(unittest.TestCase):
    def test_parses_media_sequence_and_resolves_segment_urls(self):
        body = ('#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-MEDIA-SEQUENCE:42\n'
                '#EXTINF:2.0,\na.ts?token=1\n#EXTINF:2.0,\nb.ts\n')
        result = parse_media_playlist(body,
                                      'https://example.org/live/channel.m3u8?auth=abc')
        self.assertEqual(result, [
            (42, 'https://example.org/live/a.ts?token=1'),
            (43, 'https://example.org/live/b.ts'),
        ])

    def test_sequence_does_not_repeat_segments_after_refresh(self):
        base = 'https://example.org/live/index.m3u8'
        initial = ('#EXTM3U\n#EXT-X-MEDIA-SEQUENCE:10\n'
                   '#EXTINF:2,\n10.ts\n#EXTINF:2,\n11.ts\n')
        updated = ('#EXTM3U\n#EXT-X-MEDIA-SEQUENCE:11\n'
                   '#EXTINF:2,\n11.ts\n#EXTINF:2,\n12.ts\n')
        sequence = HlsSequence(base)
        self.assertEqual([n for n, _ in sequence.accept(initial)], [10, 11])
        self.assertEqual([n for n, _ in sequence.accept(updated)], [12])

    def test_rejects_encrypted_or_non_ts_stream(self):
        base = 'https://example.org/live/index.m3u8'
        with self.assertRaises(ValueError):
            parse_media_playlist('#EXTM3U\n#EXT-X-KEY:METHOD=AES-128,URI="key"\n'
                                 '#EXTINF:2,\npart.ts\n', base)
        with self.assertRaises(ValueError):
            parse_media_playlist('#EXTM3U\n#EXT-X-MAP:URI="init.mp4"\n'
                                 '#EXTINF:2,\npart.m4s\n', base)


if __name__ == '__main__':
    unittest.main()
