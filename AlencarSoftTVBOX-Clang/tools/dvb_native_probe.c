/* Bounded native MPEG-TS playback probe for the Azamerica S1009+.
 * Uses the Linux DVB demux/DVR API directly; no factory player is launched.
 * The sample has H.264 PID 0x100 and AAC PID 0x101.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/dvb/dmx.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define TS_PACKET_SIZE 188
#define CHUNK_PACKETS 50
#define CHUNK_BYTES (TS_PACKET_SIZE * CHUNK_PACKETS)

static volatile sig_atomic_t expired;

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
    unsigned char buffer[CHUNK_BYTES];
    unsigned long total = 0;
    int status = 1;

    if (argc != 2) {
        fprintf(stderr, "usage: dvb_native_probe <mpeg-ts-file>\n");
        return 2;
    }
    source = open(argv[1], O_RDONLY);
    if (source < 0) {
        fprintf(stderr, "source_open errno=%d\n", errno);
        goto done;
    }
    signal(SIGALRM, on_alarm);
    alarm(24);

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

    while (!expired) {
        ssize_t got = read(source, buffer, sizeof(buffer));
        ssize_t sent = 0;
        if (got == 0) {
            status = 0;
            break;
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
    printf("dvr_sent=%lu expired=%d\n", total, expired);
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
    alarm(0);
    return status;
}
