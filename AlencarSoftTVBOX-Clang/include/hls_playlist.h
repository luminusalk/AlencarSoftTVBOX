#ifndef ALENCARSOFT_HLS_PLAYLIST_H
#define ALENCARSOFT_HLS_PLAYLIST_H

#include <stddef.h>
#include <stdint.h>

#define HLS_SEGMENT_URL_SIZE 1024
int hls_resolve_url(const char *base, const char *reference, char *out, size_t size);

typedef struct {
    uint64_t sequence;
    unsigned int duration_ms;
    char url[HLS_SEGMENT_URL_SIZE];
} hls_segment_t;

/* Returns fresh MPEG-TS segments and advances next_sequence only on success.
 * Master, encrypted and fragmented-MP4 playlists are deliberately rejected. */
int hls_parse_media_playlist(const char *body, const char *playlist_url,
                             uint64_t *next_sequence, hls_segment_t *segments,
                             size_t capacity, size_t *count);

#endif
