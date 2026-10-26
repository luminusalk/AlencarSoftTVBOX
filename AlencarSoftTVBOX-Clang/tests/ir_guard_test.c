#include "ir_guard.h"
#include <sys/prctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile unsigned *ticks;
static void *keys(void *unused) {
    (void)unused;
    prctl(PR_SET_NAME, "key_task", 0, 0, 0);
    for (;;) { __sync_fetch_and_add(&ticks[0], 1); usleep(10000); }
    return NULL;
}
int main(void) {
    pid_t fw, app; unsigned before, other; int failed = 0, ready[2]; char c;
    ticks = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (ticks == MAP_FAILED) return 2;
    fw = fork();
    if (fw == 0) {
        pthread_t t;
        prctl(PR_SET_NAME, "dvbapp", 0, 0, 0);
        prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
        pthread_create(&t, NULL, keys, NULL);
        for (;;) { __sync_fetch_and_add(&ticks[1], 1); usleep(10000); }
    }
    if (fw < 0) return 2;
    for (int i = 0; i < 200 && ticks[0] < 2; i++) usleep(10000);
    if (ir_guard_start() < 0) { failed = 3; goto cleanup; }
    before = ticks[0]; other = ticks[1]; usleep(150000);
    if (!ir_guard_active() || ticks[0] != before || ticks[1] <= other) failed = 4;
    ir_guard_stop(); usleep(150000);
    if (ticks[0] <= before || ir_guard_active()) failed = 5;
    if (failed) goto cleanup;
    /* Abrupt app death must release keys even if inherited pipe FDs remain. */
    if (pipe(ready)) { failed = 6; goto cleanup; }
    app = fork();
    if (app == 0) {
        close(ready[0]);
        c = ir_guard_start() == 0 ? 'Y' : 'N';
        if (write(ready[1], &c, 1) != 1) _exit(10);
        for (;;) pause();
    }
    close(ready[1]);
    if (app < 0) { close(ready[0]); failed = 7; goto cleanup; }
    if (read(ready[0], &c, 1) != 1 || c != 'Y') failed = 8;
    close(ready[0]); before = ticks[0];
    kill(app, SIGKILL); waitpid(app, NULL, 0);
    for (int i = 0; i < 200 && ticks[0] == before; i++) usleep(10000);
    if (ticks[0] == before) failed = 9;
cleanup:
    ir_guard_stop(); kill(fw, SIGTERM); waitpid(fw, NULL, 0);
    fprintf(stderr, "ir_guard lifecycle result=%d\n", failed);
    return failed;
}
