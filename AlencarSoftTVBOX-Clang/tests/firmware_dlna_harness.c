#include "firmware_dlna.h"

#include <string.h>
#include <unistd.h>

/* This test sends a local TS file, so the HLS transport must not be used. */
int hls_bridge_start(const char *url) { (void)url; return -1; }
void hls_bridge_stop(void) {}
const char *hls_bridge_get_url(void) { return ""; }

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    if (argc >= 3 && strcmp(argv[2], "cycle") == 0) {
        if (firmware_dlna_start(argv[1], "Canal & teste") < 0) return 3;
        if (!firmware_dlna_active()) return 4;
        usleep(50000);
        firmware_dlna_stop();
        if (firmware_dlna_active()) return 5;
        if (firmware_dlna_start(argv[1], "Canal & teste 2") < 0) return 6;
        if (!firmware_dlna_active()) return 7;
        usleep(50000);
        firmware_dlna_stop();
        return firmware_dlna_active() ? 8 : 0;
    }
    if (firmware_dlna_start(argv[1], "Canal & teste") < 0) return 3;
    if (!firmware_dlna_active()) return 4;
    usleep(100000);
    firmware_dlna_stop();
    return firmware_dlna_active() ? 5 : 0;
}
