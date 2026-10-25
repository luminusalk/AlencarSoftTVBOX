#include "firmware_dlna.h"

#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    if (firmware_dlna_start(argv[1], "TV Cultura") < 0) return 3;
    sleep(1);
    firmware_dlna_stop();
    return 0;
}
