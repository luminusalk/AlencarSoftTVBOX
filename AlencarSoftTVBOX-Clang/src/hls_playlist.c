#include "hls_playlist.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int copy_url(char *out, size_t size, const char *src, size_t length) {
    if (!out || !src || length == 0 || length >= size) return -1;
    memcpy(out, src, length);
    out[length] = '\0';
    return 0;
}

int hls_resolve_url(const char *base, const char *reference,
                       char *out, size_t size) {
    const char *scheme_end = strstr(base, "://");
    const char *authority_end;
    const char *last_slash;
    size_t prefix;
    size_t ref_len = strlen(reference);

    if (!scheme_end || (strncmp(base, "http://", 7) != 0 &&
                        strncmp(base, "https://", 8) != 0)) return -1;
    if (strncmp(reference, "http://", 7) == 0 ||
        strncmp(reference, "https://", 8) == 0)
        return copy_url(out, size, reference, ref_len);

    authority_end = strchr(scheme_end + 3, '/');
    if (!authority_end) authority_end = base + strlen(base);
    if (reference[0] == '/' && reference[1] == '/') {
        prefix = (size_t)(scheme_end - base + 1);
    } else if (reference[0] == '/') {
        prefix = (size_t)(authority_end - base);
    } else {
        const char *path_end = strpbrk(authority_end, "?#");
        if (!path_end) path_end = base + strlen(base);
        last_slash = path_end;
        while (last_slash > authority_end && last_slash[-1] != '/') last_slash--;
        prefix = (size_t)(last_slash - base);
        if (prefix == (size_t)(authority_end - base)) {
            if (prefix + 1 + ref_len >= size) return -1;
            memcpy(out, base, prefix);
            out[prefix++] = '/';
            memcpy(out + prefix, reference, ref_len + 1);
            return 0;
        }
    }
    if (prefix + ref_len >= size) return -1;
    memcpy(out, base, prefix);
    memcpy(out + prefix, reference, ref_len + 1);
    return 0;
}

static int is_ts_url(const char *url) {
    const char *end = strpbrk(url, "?#");
    size_t length = end ? (size_t)(end - url) : strlen(url);
    return length >= 3 && url[length - 3] == '.' &&
           (url[length - 2] == 't' || url[length - 2] == 'T') &&
           (url[length - 1] == 's' || url[length - 1] == 'S');
}

int hls_parse_media_playlist(const char *body, const char *playlist_url,
                             uint64_t *next_sequence, hls_segment_t *segments,
                             size_t capacity, size_t *count) {
    const char *cursor;
    uint64_t first_sequence = 0;
    uint64_t next_after = 0;
    size_t seen = 0, fresh = 0;
    int first_line = 1, has_sequence = 0;
    unsigned int pending_duration_ms = 0;

    if (!body || !playlist_url || !next_sequence || !segments || !count)
        return -1;
    *count = 0;
    next_after = *next_sequence;
    cursor = body;
    while (*cursor) {
        const char *end = strpbrk(cursor, "\r\n");
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        char line[HLS_SEGMENT_URL_SIZE];
        char *tail;
        unsigned long long parsed;

        while (length && isspace((unsigned char)*cursor)) {
            cursor++;
            length--;
        }
        while (length && isspace((unsigned char)cursor[length - 1])) length--;
        if (length >= sizeof(line)) return -1;
        memcpy(line, cursor, length);
        line[length] = '\0';
        if (end) {
            cursor = end + 1;
            while (*cursor == '\r' || *cursor == '\n') cursor++;
        } else cursor += strlen(cursor);

        if (!length) continue;
        if (first_line) {
            first_line = 0;
            if (strcmp(line, "#EXTM3U") != 0) return -1;
            continue;
        }
        if (strncmp(line, "#EXT-X-KEY:", 11) == 0 ||
            strncmp(line, "#EXT-X-MAP:", 11) == 0 ||
            strncmp(line, "#EXT-X-STREAM-INF:", 18) == 0 ||
            strncmp(line, "#EXT-X-I-FRAME-STREAM-INF:", 26) == 0)
            return -1;
        if (strncmp(line, "#EXT-X-MEDIA-SEQUENCE:", 22) == 0) {
            if (has_sequence || !isdigit((unsigned char)line[22])) return -1;
            errno = 0;
            parsed = strtoull(line + 22, &tail, 10);
            if (errno || *tail) return -1;
            first_sequence = (uint64_t)parsed;
            has_sequence = 1;
            continue;
        }
        if (strncmp(line, "#EXTINF:", 8) == 0) {
            double seconds;
            errno = 0;
            seconds = strtod(line + 8, &tail);
            if (errno || tail == line + 8 || (*tail && *tail != ',') ||
                seconds < 0.0 || seconds != seconds) return -1;
            if (seconds > 120.0) seconds = 120.0;
            pending_duration_ms = (unsigned int)(seconds * 1000.0 + 0.5);
            continue;
        }
        if (line[0] == '#') continue;
        if (!has_sequence || first_sequence > UINT64_MAX - seen) return -1;
        if (first_sequence + seen >= *next_sequence) {
            if (fresh >= capacity) return -1;
            if (hls_resolve_url(playlist_url, line, segments[fresh].url,
                            sizeof(segments[fresh].url)) < 0 ||
                !is_ts_url(segments[fresh].url)) return -1;
            segments[fresh].sequence = first_sequence + seen;
            segments[fresh].duration_ms = pending_duration_ms;
            fresh++;
        }
        pending_duration_ms = 0;
        seen++;
    }
    if (first_line || !has_sequence || seen == 0 ||
        first_sequence > UINT64_MAX - seen) return -1;
    if (fresh) next_after = first_sequence + seen;
    *next_sequence = next_after;
    *count = fresh;
    return 0;
}
