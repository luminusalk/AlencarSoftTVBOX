/* Read-only GXAV ABI probe for the NationalChip GX6622 receiver.
 * ABI constants and layouts are documented in:
 * https://github.com/McMCCRU/minisatip-gx/tree/master/src/gxapi/include
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define GXAV_IOCTL_CHIP_DETECT 0x1701
#define GXAV_IOCTL_FBINFO_GET 0x1800

struct gxav_chip_detect {
    uint32_t chip_id;
};

struct gxav_fb_info {
    uint32_t start;
    uint32_t size;
    uint32_t main_surface_size;
};

int main(void) {
    struct gxav_chip_detect chip = {0};
    struct gxav_fb_info fb = {0};
    int fd = open("/dev/gxav0", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "gxav_open errno=%d (%s)\n", errno, strerror(errno));
        return 1;
    }

    if (ioctl(fd, GXAV_IOCTL_CHIP_DETECT, &chip) < 0)
        fprintf(stderr, "chip_detect errno=%d (%s)\n", errno, strerror(errno));
    else
        printf("chip_id=0x%08x\n", chip.chip_id);

    if (ioctl(fd, GXAV_IOCTL_FBINFO_GET, &fb) < 0)
        fprintf(stderr, "fbinfo_get errno=%d (%s)\n", errno, strerror(errno));
    else
        printf("fb_start=0x%08x fb_size=%u main_surface_size=%u\n",
               fb.start, fb.size, fb.main_surface_size);

    close(fd);
    return 0;
}
