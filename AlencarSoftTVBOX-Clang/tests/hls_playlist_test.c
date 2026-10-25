#include "hls_playlist.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    hls_segment_t segments[4];
    size_t count = 0;
    uint64_t next = 0;
    const char *base = "https://example.org:8443/live/index.m3u8?auth=old";
    const char *first = "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-MEDIA-SEQUENCE:42\n"
                        "#EXTINF:2,\na.ts?token=1\n#EXTINF:2,\n/live/b.ts\n";
    const char *second = "#EXTM3U\n#EXT-X-MEDIA-SEQUENCE:43\n"
                         "#EXTINF:2,\n/live/b.ts\n#EXTINF:2,\n"
                         "https://cdn.example.org/c.ts?token=2\n";

    assert(hls_parse_media_playlist(first, base, &next, segments, 4, &count) == 0);
    assert(count == 2);
    assert(segments[0].sequence == 42);
    assert(strcmp(segments[0].url, "https://example.org:8443/live/a.ts?token=1") == 0);
    assert(strcmp(segments[1].url, "https://example.org:8443/live/b.ts") == 0);
    assert(next == 44);

    assert(hls_parse_media_playlist(second, base, &next, segments, 4, &count) == 0);
    assert(count == 1);
    assert(segments[0].sequence == 44);
    assert(strcmp(segments[0].url, "https://cdn.example.org/c.ts?token=2") == 0);
    assert(next == 45);

    assert(hls_parse_media_playlist("#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1\nlow.m3u8\n",
                                   base, &next, segments, 4, &count) < 0);
    assert(hls_parse_media_playlist("#EXTM3U\n#EXT-X-KEY:METHOD=AES-128\n"
                                   "#EXT-X-MEDIA-SEQUENCE:1\n1.ts\n",
                                   base, &next, segments, 4, &count) < 0);
    assert(hls_parse_media_playlist("#EXTM3U\n#EXT-X-MAP:URI=init.mp4\n"
                                   "#EXT-X-MEDIA-SEQUENCE:1\n1.m4s\n",
                                   base, &next, segments, 4, &count) < 0);
    assert(hls_parse_media_playlist("not a playlist", base, &next, segments, 4, &count) < 0);
    return 0;
}
