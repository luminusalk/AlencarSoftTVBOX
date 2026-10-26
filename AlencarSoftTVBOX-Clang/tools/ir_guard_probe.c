/* Bounded diagnostic: hold only the firmware key thread in ptrace-stop.
 * No register/memory writes, no group SIGSTOP, no persistent changes.
 * Tracer exit also releases the tracee (PTRACE_O_EXITKILL is never enabled).
 */
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifndef __WALL
#define __WALL 0x40000000
#endif
static volatile sig_atomic_t done;
static void finish(int sig) { (void)sig; done = 1; }
static int named(const char *path, const char *expected) {
    char name[64]; FILE *f = fopen(path, "r");
    if (!f) return 0;
    if (!fgets(name, sizeof(name), f)) name[0] = 0;
    fclose(f); name[strcspn(name, "\r\n")] = 0;
    return !strcmp(name, expected);
}
int main(int argc, char **argv) {
    DIR *proc, *tasks; struct dirent *p, *t;
    char path[256]; pid_t tid = -1, owner = -1; int found = 0, status, i;
    int seconds = argc == 2 ? atoi(argv[1]) : 15;
    if (seconds < 1 || seconds > 30) return 2;
    signal(SIGTERM, finish); signal(SIGINT, finish); signal(SIGHUP, finish);
    proc = opendir("/proc"); if (!proc) return 3;
    while ((p = readdir(proc))) {
        int pid = atoi(p->d_name); if (pid <= 1) continue;
        snprintf(path, sizeof(path), "/proc/%d/comm", pid);
        if (!named(path, "dvbapp")) continue;
        snprintf(path, sizeof(path), "/proc/%d/task", pid);
        tasks = opendir(path); if (!tasks) continue;
        while ((t = readdir(tasks))) {
            int candidate = atoi(t->d_name); if (candidate <= 1 || candidate == pid) continue;
            snprintf(path, sizeof(path), "/proc/%d/task/%d/comm", pid, candidate);
            if (named(path, "key_task")) { tid = candidate; owner = pid; found++; }
        }
        closedir(tasks);
    }
    closedir(proc);
    if (found != 1) { fprintf(stderr, "Expected one key_task; found %d\n", found); return 4; }
    fprintf(stderr, "[IR guard] firmware=%d key_thread=%d duration=%d\n", owner, tid, seconds);
    if (ptrace(PTRACE_ATTACH, tid, 0, 0) < 0) { perror("PTRACE_ATTACH"); return 5; }
    /* Never block indefinitely waiting for a thread stuck inside the kernel. */
    for (i = 0; i < 200 && !done; i++) {
        pid_t got = waitpid(tid, &status, WNOHANG | __WALL);
        if (got < 0 && errno != EINTR) { perror("waitpid"); return 6; }
        if (got == tid) {
            if (!WIFSTOPPED(status)) return 7;
            if (WSTOPSIG(status) == SIGSTOP) break;
            if (ptrace(PTRACE_CONT, tid, 0, WSTOPSIG(status)) < 0) return 8;
        }
        usleep(10000);
    }
    if (i == 200 || done) { fprintf(stderr, "Attach cancelled/timed out; exiting tracer\n"); return 9; }
    fprintf(stderr, "[IR guard] ACTIVE; only key thread held\n");
    for (i = 0; i < seconds * 10 && !done; i++) usleep(100000);
    if (ptrace(PTRACE_DETACH, tid, 0, 0) < 0) { perror("PTRACE_DETACH"); return 10; }
    fprintf(stderr, "[IR guard] RELEASED\n");
    return 0;
}
