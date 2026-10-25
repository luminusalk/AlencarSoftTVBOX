/* Direct Linux syscall test for C-SKY without libc runtime */

void _start(void) {
    const char msg[] = "HELLO_CSKY_NATIVE_OK\n";
    register int sys_no __asm__("r7") = 64; /* __NR_write */
    register int fd __asm__("r2") = 1;      /* stdout */
    register const char *buf __asm__("r3") = msg;
    register int len __asm__("r4") = sizeof(msg) - 1;
    __asm__ volatile("trap 0" : "+r"(fd) : "r"(sys_no), "r"(buf), "r"(len) : "memory");

    register int exit_no __asm__("r7") = 93; /* __NR_exit */
    register int code __asm__("r2") = 0;
    __asm__ volatile("trap 0" : : "r"(exit_no), "r"(code) : "memory");
}
