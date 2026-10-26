/* App-lifetime guardian: hold only the firmware key thread in ptrace-stop.
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
#include <fcntl.h>
#include <poll.h>
#include "ir_guard.h"

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
static int guard_child(int life, int ready, pid_t parent) {
    DIR *proc, *tasks; struct dirent *p, *t;
    char path[256]; pid_t tid = -1, owner = -1; int found = 0, status, i;
    done = 0;
    signal(SIGPIPE, SIG_IGN);
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
    fprintf(stderr, "[IR guard] firmware=%d key_thread=%d\n", owner, tid);
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
    if (write(ready, "Y", 1) != 1) done = 1;
    close(ready);
    while (!done && getppid() == parent) {
        struct pollfd pfd = {life, POLLIN, 0};
        int rc = poll(&pfd, 1, 250);
        if (rc > 0 || (rc < 0 && errno != EINTR)) break;
    }
    if (ptrace(PTRACE_DETACH, tid, 0, 0) < 0) { perror("PTRACE_DETACH"); return 10; }
    fprintf(stderr, "[IR guard] RELEASED\n");
    return 0;
}

static pid_t guard_pid = -1;
static int guard_life = -1;

int ir_guard_active(void) {
    pid_t result;
    if (guard_pid <= 0) return 0;
    result = waitpid(guard_pid, NULL, WNOHANG);
    if (result < 0 && errno == EINTR) return 1;
    if (result != 0) {
        guard_pid = -1;
        if (guard_life >= 0) close(guard_life);
        guard_life = -1;
        return 0;
    }
    return 1;
}

void ir_guard_stop(void) {
    if (guard_life >= 0) close(guard_life);
    guard_life = -1;
    if (guard_pid > 0) {
        for (int i = 0; i < 100; i++) {
            pid_t result = waitpid(guard_pid, NULL, WNOHANG);
            if (result == guard_pid || (result < 0 && errno == ECHILD)) {
                guard_pid = -1; return;
            }
            usleep(10000);
        }
        /* Killing only our tracer releases its tracee; never kill firmware. */
        kill(guard_pid, SIGKILL);
        while (waitpid(guard_pid, NULL, 0) < 0 && errno == EINTR) {}
        guard_pid = -1;
    }
}

int ir_guard_start(void) {
    int life[2], ready[2]; pid_t parent = getpid(); char result = 0;
    struct pollfd pfd;
    if (ir_guard_active()) return 0;
    if (pipe(life) < 0) return -1;
    if (pipe(ready) < 0) { close(life[0]); close(life[1]); return -1; }
    fcntl(life[1], F_SETFD, FD_CLOEXEC);
    guard_pid = fork();
    if (guard_pid == 0) {
        close(life[1]); close(ready[0]);
        _exit(guard_child(life[0], ready[1], parent));
    }
    close(life[0]); close(ready[1]);
    if (guard_pid < 0) { close(life[1]); close(ready[0]); return -1; }
    guard_life = life[1];
    pfd.fd = ready[0]; pfd.events = POLLIN; pfd.revents = 0;
    if (poll(&pfd, 1, 3500) > 0) {
        if (read(ready[0], &result, 1) != 1) result = 0;
    }
    close(ready[0]);
    if (result != 'Y') { ir_guard_stop(); return -1; }
    return 0;
}
