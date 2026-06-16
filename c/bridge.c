// Define POSIX C source version
#define _POSIX_C_SOURCE 200809L

// Define RISC-V sysroot path
#define RISCV_SYSROOT "/usr/riscv64-linux-gnu"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "bridge.h"

static char qemu_path[PATH_MAX] = "qemu-riscv64";
static char app_path[PATH_MAX]  = "./build/app";
static char last_error[2048]    = "";

static void set_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(last_error, sizeof(last_error), fmt, ap);
    va_end(ap);
}

const char *bridge_last_error(void) {
    return last_error;
}

void bridge_set_qemu_path(const char *path) {
    if (path && *path) snprintf(qemu_path, sizeof(qemu_path), "%s", path);
}

void bridge_set_app_path(const char *path) {
    if (path && *path) snprintf(app_path, sizeof(app_path), "%s", path);
}

static int write_all(int fd, const uint8_t *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, buf + off, len - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

static int run_riscv(
    char *const argv[],
    const uint8_t *in,
    size_t in_sz,
    uint8_t *out,
    size_t out_sz,
    char **text_out
) {
    int in_pipe[2], out_pipe[2];

    if (pipe(in_pipe) || pipe(out_pipe)) {
        set_error("pipe failed");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        set_error("fork failed");
        return -1;
    }

    if (pid == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);

        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);

        execvp(argv[0], argv);
        _exit(127);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);

    if (write_all(in_pipe[1], in, in_sz) != 0) {
        int saved_errno = errno;
        close(in_pipe[1]);

        char *err_buf = NULL;
        size_t err_cap = 0, err_len = 0;
        char tmp[4096];
        ssize_t n;
        while ((n = read(out_pipe[0], tmp, sizeof(tmp))) > 0) {
            if (err_len + (size_t)n + 1 > err_cap) {
                err_cap = err_cap ? err_cap * 2 : 8192;
                err_buf = realloc(err_buf, err_cap);
            }
            memcpy(err_buf + err_len, tmp, (size_t)n);
            err_len += (size_t)n;
        }
        close(out_pipe[0]);

        if (err_buf) {
            err_buf[err_len] = '\0';
            set_error("write stdin failed: %s; child output:\n%s", strerror(saved_errno), err_buf);
            free(err_buf);
        } else {
            set_error("write stdin failed: %s", strerror(saved_errno));
        }
        return -1;
    }
    close(in_pipe[1]);

    char *buf = NULL;
    size_t cap = 0, len = 0;
    char tmp[4096];

    ssize_t n;
    while ((n = read(out_pipe[0], tmp, sizeof(tmp))) > 0) {
        if (out) {
            if (len + (size_t)n <= out_sz)
                memcpy(out + len, tmp, (size_t)n);
        }

        if (len + (size_t)n + 1 > cap) {
            cap = cap ? cap * 2 : 8192;
            buf = realloc(buf, cap);
        }
        memcpy(buf + len, tmp, (size_t)n);
        len += (size_t)n;
    }

    close(out_pipe[0]);

    int status;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        if (buf) {
            buf[len] = '\0';
            set_error("child failed: %s", buf);
        } else {
            set_error("child failed");
        }
        free(buf);
        return -1;
    }

    if (text_out) {
        buf[len] = '\0';
        *text_out = buf;
    } else {
        free(buf);
    }

    return 0;
}

static double parse_avg(const char *s) {
    const char *p = strstr(s, "avg_ns=");
    if (!p) return -1;
    return strtod(p + 7, NULL);
}

// Wrappers
int bridge_invert(const uint8_t *in, uint8_t *out, int size) {
    char s[16];
    snprintf(s, sizeof(s), "%d", size);

    char *argv[] = {
        qemu_path, "-L", RISCV_SYSROOT,
        app_path, "invert", s,
        NULL
    };

    return run_riscv(argv, in, size, out, size, NULL);
}

int bridge_threshold(const uint8_t *in, uint8_t *out, int size, uint8_t T) {
    char t[16], s[16];
    snprintf(t, sizeof(t), "%u", T);
    snprintf(s, sizeof(s), "%d", size);

    char *argv[] = {
        qemu_path, "-L", RISCV_SYSROOT,
        app_path, "threshold", s, t,
        NULL
    };

    return run_riscv(argv, in, size, out, size, NULL);
}

int bridge_brightness(const uint8_t *in, uint8_t *out, int size, int B) {
    char b[16], s[16];
    snprintf(b, sizeof(b), "%d", B);
    snprintf(s, sizeof(s), "%d", size);

    char *argv[] = {
        qemu_path, "-L", RISCV_SYSROOT,
        app_path, "brightness", s, b,
        NULL
    };

    return run_riscv(argv, in, size, out, size, NULL);
}

int bridge_blur(const uint8_t *in, uint8_t *out, int w, int h) {
    int size = w * h;
    char ws[16], hs[16];
    snprintf(ws, sizeof(ws), "%d", w);
    snprintf(hs, sizeof(hs), "%d", h);

    char *argv[] = {
        qemu_path, "-L", RISCV_SYSROOT,
        app_path, "blur", ws, hs,
        NULL
    };

    return run_riscv(argv, in, size, out, size, NULL);
}

// Benches
double bridge_bench_invert(const uint8_t *in, int size, int iters) {
    char it[16], s[16];
    snprintf(it, sizeof(it), "%d", iters);
    snprintf(s, sizeof(s), "%d", size);

    char *argv[] = {
        qemu_path, "-L", RISCV_SYSROOT,
        app_path, "bench", "invert", s, it,
        NULL
    };

    char *txt = NULL;
    if (run_riscv(argv, in, size, NULL, 0, &txt) != 0) return -1;
    double v = parse_avg(txt ? txt : "");
    free(txt);
    return v;
}

double bridge_bench_threshold(const uint8_t *in, int size, uint8_t T, int iters) {
    char t[16], it[16], s[16];
    snprintf(t, sizeof(t), "%u", T);
    snprintf(it, sizeof(it), "%d", iters);
    snprintf(s, sizeof(s), "%d", size);

    char *argv[] = {
        qemu_path, "-L", RISCV_SYSROOT,
        app_path, "bench", "threshold", s, t, it,
        NULL
    };

    char *txt = NULL;
    if (run_riscv(argv, in, size, NULL, 0, &txt) != 0) return -1;
    double v = parse_avg(txt ? txt : "");
    free(txt);
    return v;
}

double bridge_bench_brightness(const uint8_t *in, int size, int B, int iters) {
    char b[16], it[16], s[16];
    snprintf(b, sizeof(b), "%d", B);
    snprintf(it, sizeof(it), "%d", iters);
    snprintf(s, sizeof(s), "%d", size);

    char *argv[] = {
        qemu_path, "-L", RISCV_SYSROOT,
        app_path, "bench", "brightness", s, b, it,
        NULL
    };

    char *txt = NULL;
    if (run_riscv(argv, in, size, NULL, 0, &txt) != 0) return -1;
    double v = parse_avg(txt ? txt : "");
    free(txt);
    return v;
}

double bridge_bench_blur(const uint8_t *in, int w, int h, int iters) {
    int size = w * h;
    char ws[16], hs[16], it[16];
    snprintf(ws, sizeof(ws), "%d", w);
    snprintf(hs, sizeof(hs), "%d", h);
    snprintf(it, sizeof(it), "%d", iters);

    char *argv[] = {
        qemu_path, "-L", RISCV_SYSROOT,
        app_path, "bench", "blur", ws, hs, it,
        NULL
    };

    char *txt = NULL;
    if (run_riscv(argv, in, size, NULL, 0, &txt) != 0) return -1;
    double v = parse_avg(txt ? txt : "");
    free(txt);
    return v;
}