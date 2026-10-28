/* Temporary diagnostic, never linked into the application.
 * Read registers of one explicitly selected dvbapp thread, then detach.
 * No memory/register writes and no process-wide stop. */
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifndef __WALL
#define __WALL 0x40000000
#endif
static void expired(int sig) { (void)sig; _exit(90); }
static int comm(pid_t pid, pid_t tid, char *name, size_t size) {
    char path[128]; FILE *f;
    snprintf(path, sizeof(path), "/proc/%ld/task/%ld/comm", (long)pid, (long)tid);
    f = fopen(path, "r"); if (!f) return -1;
    if (!fgets(name, size, f)) { fclose(f); return -1; }
    fclose(f); name[strcspn(name, "\r\n")] = 0; return 0;
}
int main(int argc, char **argv) {
    pid_t pid, tid; char name[64], *end; long value;
    unsigned long regs[512], frames[96]; int status, i, attached = 0, rc = 1;
    if (argc != 3) { fprintf(stderr, "Usage: thread_snapshot PID TID\n"); return 2; }
    errno = 0; value = strtol(argv[1], &end, 10);
    if (errno || *end || value <= 1 || value > 4194304) return 2;
    pid = (pid_t)value;
    errno = 0; value = strtol(argv[2], &end, 10);
    if (errno || *end || value <= 1 || value > 4194304) return 2;
    tid = (pid_t)value;
    if (comm(pid, pid, name, sizeof(name)) || strcmp(name, "dvbapp")) return 3;
    if (comm(pid, tid, name, sizeof(name))) return 3;
    /* Existing IR guardian owns key_task. Never interfere with it. */
    if (!strcmp(name, "key_task")) return 3;
    signal(SIGALRM, expired); signal(SIGTERM, expired); signal(SIGINT, expired);
    alarm(4);
    if (ptrace(PTRACE_ATTACH, tid, 0, 0) < 0) { perror("attach"); return 4; }
    attached = 1;
    for (i = 0; i < 100; i++) {
        pid_t got = waitpid(tid, &status, WNOHANG | __WALL);
        if (got == tid) {
            if (!WIFSTOPPED(status)) goto out;
            if (WSTOPSIG(status) == SIGSTOP) break;
            if (ptrace(PTRACE_CONT, tid, 0, WSTOPSIG(status)) < 0) goto out;
        } else if (got < 0 && errno != EINTR) goto out;
        usleep(10000);
    }
    if (i == 100) goto out;
    memset(regs, 0, sizeof(regs));
    if (ptrace(PTRACE_GETREGS, tid, 0, regs) < 0) { perror("getregs"); goto out; }
    /* C-SKY ABIV1: saved r0 is SP. Read a bounded stack window while attached.
     * PEEKDATA does not alter the tracee; addresses are validated below. */
    memset(frames, 0, sizeof(frames));
    if (regs[0] >= 0x1000000UL && regs[0] < 0x80000000UL) {
        for (i = 0; i < 96; i++) {
            long word;
            errno = 0;
            word = ptrace(PTRACE_PEEKDATA, tid,
                          (void *)(regs[0] + (unsigned long)i * sizeof(long)), 0);
            if (errno) break;
            frames[i] = (unsigned long)word;
        }
    }
    /* Release before printing so slow terminal output cannot extend the stop. */
    if (ptrace(PTRACE_DETACH, tid, 0, 0) < 0) { perror("detach"); goto out; }
    attached = 0; alarm(0);
    printf("SNAPSHOT pid=%ld tid=%ld name=%s word=%u\n", (long)pid, (long)tid,
           name, (unsigned)sizeof(unsigned long));
    for (i = 0; i < 64; i++) printf("reg[%02d]=%08lx\n", i, regs[i]);
    for (i = 0; i < 96; i++) {
        if (frames[i] >= 0x8000UL && frames[i] < 0xf0c000UL)
            printf("stack[%02d]=%08lx\n", i, frames[i]);
    }
    rc = 0;
out:
    if (attached) ptrace(PTRACE_DETACH, tid, 0, 0);
    return rc; /* Tracer exit releases tracee even on failed detach. */
}
