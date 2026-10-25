/* Bounded native MPEG-TS playback probe for the Azamerica S1009+.
 * Uses the Linux DVB demux/DVR API directly; no factory player is launched.
 * The sample has H.264 PID 0x100 and AAC PID 0x101.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/dvb/dmx.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define TS_PACKET_SIZE 188
#define CHUNK_PACKETS 50
#define CHUNK_BYTES (TS_PACKET_SIZE * CHUNK_PACKETS)

static volatile sig_atomic_t expired;

struct screen_backup {
    int fd;
    void *pixels;
    unsigned char *original;
    size_t length;
};

static int clear_osd(struct screen_backup *screen)
{
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    memset(screen, 0, sizeof(*screen));
    screen->fd = -1;
    screen->fd = open("/dev/fb0", O_RDWR);
    if (screen->fd < 0 ||
        ioctl(screen->fd, FBIOGET_VSCREENINFO, &var) < 0 ||
        ioctl(screen->fd, FBIOGET_FSCREENINFO, &fix) < 0) {
        fprintf(stderr, "fb_open errno=%d\n", errno);
        return -1;
    }
    screen->length = (size_t)fix.line_length * var.yres;
    if (screen->length == 0 || screen->length > 8 * 1024 * 1024) {
        fprintf(stderr, "fb_size invalid=%lu\n", (unsigned long)screen->length);
        return -1;
    }
    screen->pixels = mmap(NULL, screen->length, PROT_READ | PROT_WRITE,
                          MAP_SHARED, screen->fd, 0);
    if (screen->pixels == MAP_FAILED) {
        screen->pixels = NULL;
        fprintf(stderr, "fb_mmap errno=%d\n", errno);
        return -1;
    }
    screen->original = malloc(screen->length);
    if (!screen->original) {
        fprintf(stderr, "fb_backup alloc failed\n");
        return -1;
    }
    memcpy(screen->original, screen->pixels, screen->length);
    memset(screen->pixels, 0, screen->length);
    printf("osd_clear bytes=%lu\n", (unsigned long)screen->length);
    fflush(stdout);
    return 0;
}

static void restore_osd(struct screen_backup *screen)
{
    if (screen->original && screen->pixels) {
        memcpy(screen->pixels, screen->original, screen->length);
        free(screen->original);
    }
    if (screen->pixels) munmap(screen->pixels, screen->length);
    if (screen->fd >= 0) close(screen->fd);
}

static void on_alarm(int signal_number)
{
    (void)signal_number;
    expired = 1;
}

static int start_pes(unsigned short pid, dmx_pes_type_t type)
{
    struct dmx_pes_filter_params filter;
    int fd = open("/dev/dvb/adapter0/demux0", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "demux_open pid=%u errno=%d\n", pid, errno);
        return -1;
    }
    memset(&filter, 0, sizeof(filter));
    filter.pid = pid;
    filter.input = DMX_IN_DVR;
    filter.output = DMX_OUT_DECODER;
    filter.pes_type = type;
    filter.flags = DMX_IMMEDIATE_START;
    if (ioctl(fd, DMX_SET_PES_FILTER, &filter) < 0) {
        fprintf(stderr, "pes_filter pid=%u type=%d errno=%d\n", pid, type, errno);
        close(fd);
        return -1;
    }
    printf("pes_filter pid=%u type=%d ok\n", pid, type);
    fflush(stdout);
    return fd;
}

int main(int argc, char **argv)
{
    int source = -1, video = -1, audio = -1, dvr = -1;
    int use_clear_osd = 0, use_loop = 0;
    struct screen_backup screen;
    unsigned char buffer[CHUNK_BYTES];
    unsigned long total = 0;
    unsigned long loops = 0;
    int status = 1;
    int arg;

    memset(&screen, 0, sizeof(screen));
    screen.fd = -1;
    for (arg = 2; arg < argc; ++arg) {
        if (strcmp(argv[arg], "--clear-osd") == 0) use_clear_osd = 1;
        else if (strcmp(argv[arg], "--loop") == 0) use_loop = 1;
        else break;
    }
    if (argc < 2 || argc > 4 || arg != argc) {
        fprintf(stderr, "usage: dvb_native_probe <mpeg-ts-file> [--clear-osd] [--loop]\n");
        return 2;
    }
    source = open(argv[1], O_RDONLY);
    if (source < 0) {
        fprintf(stderr, "source_open errno=%d\n", errno);
        goto done;
    }
    signal(SIGALRM, on_alarm);
    signal(SIGTERM, on_alarm);
    signal(SIGINT, on_alarm);
    if (!use_loop) alarm(24);

    video = start_pes(0x100, DMX_PES_VIDEO0);
    if (video < 0) goto done;
    audio = start_pes(0x101, DMX_PES_AUDIO0);
    if (audio < 0) goto done;
    dvr = open("/dev/dvb/adapter0/dvr0", O_WRONLY);
    if (dvr < 0) {
        fprintf(stderr, "dvr_open errno=%d\n", errno);
        goto done;
    }
    printf("dvr_open ok\n");
    fflush(stdout);
    if (use_clear_osd && clear_osd(&screen) < 0) goto done;

    while (!expired) {
        ssize_t got = read(source, buffer, sizeof(buffer));
        ssize_t sent = 0;
        if (got == 0) {
            if (!use_loop) {
                status = 0;
                break;
            }
            if (lseek(source, 0, SEEK_SET) < 0) {
                fprintf(stderr, "source_rewind errno=%d\n", errno);
                break;
            }
            ++loops;
            printf("dvr_loop=%lu total=%lu\n", loops, total);
            fflush(stdout);
            continue;
        }
        if (got < 0) {
            fprintf(stderr, "source_read errno=%d\n", errno);
            break;
        }
        while (sent < got && !expired) {
            ssize_t wrote = write(dvr, buffer + sent, (size_t)(got - sent));
            if (wrote <= 0) {
                fprintf(stderr, "dvr_write total=%lu result=%ld errno=%d\n",
                        total, (long)wrote, errno);
                goto done;
            }
            sent += wrote;
            total += (unsigned long)wrote;
        }
        usleep(80000);
    }
    if (expired) status = 0;
    printf("dvr_sent=%lu loops=%lu expired=%d\n", total, loops, expired);
    fflush(stdout);
done:
    if (audio >= 0) {
        ioctl(audio, DMX_STOP);
        close(audio);
    }
    if (video >= 0) {
        ioctl(video, DMX_STOP);
        close(video);
    }
    if (dvr >= 0) close(dvr);
    if (source >= 0) close(source);
    restore_osd(&screen);
    alarm(0);
    return status;
}
