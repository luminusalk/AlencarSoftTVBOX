#include "firmware_dlna.h"

#include <unistd.h>

/* This test sends a local TS file, so the HLS transport must not be used. */
int hls_bridge_start(const char *url) { (void)url; return -1; }
void hls_bridge_stop(void) {}
const char *hls_bridge_get_url(void) { return ""; }

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    if (firmware_dlna_start(argv[1], "Canal & teste") < 0) return 3;
    if (!firmware_dlna_active()) return 4;
    usleep(100000);
    firmware_dlna_stop();
    return firmware_dlna_active() ? 5 : 0;
}
