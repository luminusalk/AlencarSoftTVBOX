/**
 * @file display_vfd.c
 * @brief Driver nativo de baixo nivel para visores VFD e LED.
 */

#include "display_vfd.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

static int s_vfd_fd = -1;
static panel_type_t s_panel_type = PANEL_NONE;

int vfd_init(panel_type_t type) {
    s_panel_type = type;

    /* Abre a porta serial conectada ao visor / MCU frontal */
    s_vfd_fd = open("/dev/ttyS1", O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (s_vfd_fd >= 0) {
        struct termios tty;
        if (tcgetattr(s_vfd_fd, &tty) == 0) {
            cfsetospeed(&tty, B9600);
            cfsetispeed(&tty, B9600);
            tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
            tty.c_cflag |= (CLOCAL | CREAD);
            tty.c_cflag &= ~(PARENB | PARODD);
            tty.c_cflag &= ~CSTOPB;
#ifdef CRTSCTS
            tty.c_cflag &= ~CRTSCTS;
#endif
            tcsetattr(s_vfd_fd, TCSANOW, &tty);
        }
    }
    return (s_vfd_fd >= 0) ? 0 : -1;
}

int vfd_set_text(const char *text) {
    (void)s_panel_type;
    if (!text || s_vfd_fd < 0) return -1;

    char buffer[64];
    int len = snprintf(buffer, sizeof(buffer), "%s\r\n", text);
    if (len > 0) {
        write(s_vfd_fd, buffer, (size_t)len);
    }
    return 0;
}

int vfd_set_channel_number(int number) {
    char buf[16];
    snprintf(buf, sizeof(buf), "CH %04d", number);
    return vfd_set_text(buf);
}

int vfd_set_clock(int hours, int minutes) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", hours, minutes);
    return vfd_set_text(buf);
}

void vfd_close(void) {
    if (s_vfd_fd >= 0) {
        close(s_vfd_fd);
        s_vfd_fd = -1;
    }
}
