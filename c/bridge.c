#define _POSIX_C_SOURCE 200809L

#include "bridge.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define RISCV_SYSROOT "/usr/riscv64-linux-gnu"

static char g_qemu_path[PATH_MAX] = "qemu-riscv64";
static char g_app_path[PATH_MAX] = "./build/app";
static char g_last_error[2048] = "";

static void set_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, ap);
    va_end(ap);
}

const char *bridge_last_error(void) {
    return g_last_error;
}

void bridge_set_qemu_path(const char *path) {
    if (path && *path) {
        snprintf(g_qemu_path, sizeof(g_qemu_path), "%s", path);
    }
}

void bridge_set_app_path(const char *path) {
    if (path && *path) {
        snprintf(g_app_path, sizeof(g_app_path), "%s", path);
    }
}

static int write_file(const char *path, const uint8_t *data, size_t len) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        set_error("open(%s) failed: %s", path, strerror(errno));
        return -1;
    }

    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, data + off, len - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            set_error("write(%s) failed: %s", path, strerror(errno));
            close(fd);
            return -1;
        }
        off += (size_t)n;
    }

    close(fd);
    return 0;
}

static int file_size_is(const char *path, size_t expected) {
    struct stat st;
    if (stat(path, &st) != 0) {
        set_error("stat(%s) failed: %s", path, strerror(errno));
        return -1;
    }
    if ((size_t)st.st_size != expected) {
        set_error("output size mismatch: expected %zu, got %zu", expected, (size_t)st.st_size);
        return -1;
    }
    return 0;
}

static int read_file_exact(const char *path, uint8_t *data, size_t len) {
    if (file_size_is(path, len) != 0) {
        return -1;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        set_error("open(%s) failed: %s", path, strerror(errno));
        return -1;
    }

    size_t off = 0;
    while (off < len) {
        ssize_t n = read(fd, data + off, len - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            set_error("read(%s) failed: %s", path, strerror(errno));
            close(fd);
            return -1;
        }
        if (n == 0) {
            set_error("unexpected EOF reading %s", path);
            close(fd);
            return -1;
        }
        off += (size_t)n;
    }

    close(fd);
    return 0;
}

static int make_temp_paths(char *dir_out, size_t dir_sz,
                           char *in_out, size_t in_sz,
                           char *out_out, size_t out_sz,
                           bool need_output) {
    char tmpl[] = "/tmp/coal_bridge_XXXXXX";
    char *dir = mkdtemp(tmpl);
    if (!dir) {
        set_error("mkdtemp failed: %s", strerror(errno));
        return -1;
    }

    snprintf(dir_out, dir_sz, "%s", dir);
    snprintf(in_out, in_sz, "%s/input.raw", dir_out);
    if (need_output) {
        snprintf(out_out, out_sz, "%s/output.raw", dir_out);
    } else {
        out_out[0] = '\0';
    }
    return 0;
}

static void cleanup_temp(const char *dir, const char *in_path, const char *out_path) {
    if (in_path && *in_path) unlink(in_path);
    if (out_path && *out_path) unlink(out_path);
    if (dir && *dir) rmdir(dir);
}

static int run_command_capture(char *const argv[], char **captured_text) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        set_error("pipe failed: %s", strerror(errno));
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        set_error("fork failed: %s", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execvp(argv[0], argv);
        fprintf(stderr, "execvp(%s) failed: %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    close(pipefd[1]);

    char *buf = NULL;
    size_t len = 0;
    size_t cap = 0;
    char tmp[4096];

    for (;;) {
        ssize_t n = read(pipefd[0], tmp, sizeof(tmp));
        if (n < 0) {
            if (errno == EINTR) continue;
            set_error("read from child failed: %s", strerror(errno));
            free(buf);
            close(pipefd[0]);
            (void)waitpid(pid, NULL, 0);
            return -1;
        }
        if (n == 0) break;

        if (len + (size_t)n + 1 > cap) {
            size_t new_cap = cap ? cap * 2 : 8192;
            while (new_cap < len + (size_t)n + 1) new_cap *= 2;
            char *new_buf = (char*)realloc(buf, new_cap);
            if (!new_buf) {
                set_error("realloc failed");
                free(buf);
                close(pipefd[0]);
                (void)waitpid(pid, NULL, 0);
                return -1;
            }
            buf = new_buf;
            cap = new_cap;
        }

        memcpy(buf + len, tmp, (size_t)n);
        len += (size_t)n;
    }

    close(pipefd[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        set_error("waitpid failed: %s", strerror(errno));
        free(buf);
        return -1;
    }

    if (captured_text) {
        if (!buf) {
            buf = (char*)calloc(1, 1);
            if (!buf) {
                set_error("calloc failed");
                return -1;
            }
        } else {
            char *new_buf = (char*)realloc(buf, len + 1);
            if (!new_buf) {
                set_error("realloc failed");
                free(buf);
                return -1;
            }
            buf = new_buf;
            buf[len] = '\0';
        }
        *captured_text = buf;
    } else {
        free(buf);
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        if (captured_text && *captured_text && (*captured_text)[0] != '\0') {
            set_error("%s", *captured_text);
        } else {
            set_error("command failed with status %d", WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        }
        return -1;
    }

    return 0;
}

static double parse_avg_ns(const char *text) {
    const char *p = strstr(text, "avg_ns=");
    if (!p) return -1.0;
    p += strlen("avg_ns=");
    char *end = NULL;
    double v = strtod(p, &end);
    if (end == p) return -1.0;
    return v;
}

static int invoke_riscv_normal(const char *op,
                               const char *input_path,
                               const char *output_path,
                               const char *arg4,
                               const char *arg5) {
    char *argv[12];
    int i = 0;
    argv[i++] = g_qemu_path;
    argv[i++] = "-L";
    argv[i++] = RISCV_SYSROOT;
    argv[i++] = g_app_path;
    argv[i++] = (char *)op;
    argv[i++] = (char *)input_path;
    argv[i++] = (char *)output_path;
    if (arg4) argv[i++] = (char *)arg4;
    if (arg5) argv[i++] = (char *)arg5;
    argv[i] = NULL;

    char *captured = NULL;
    int rc = run_command_capture(argv, &captured);
    if (rc != 0) {
        if (captured && *captured) set_error("%s", captured);
        free(captured);
        return -1;
    }

    free(captured);
    return 0;
}

static double invoke_riscv_bench(const char *op,
                                 const char *input_path,
                                 const char *arg4,
                                 const char *arg5,
                                 const char *arg6) {
    char *argv[13];
    int i = 0;
    argv[i++] = g_qemu_path;
    argv[i++] = "-L";
    argv[i++] = RISCV_SYSROOT;
    argv[i++] = g_app_path;
    argv[i++] = "bench";
    argv[i++] = (char *)op;
    argv[i++] = (char *)input_path;
    argv[i++] = (char *)arg4;
    if (arg5) argv[i++] = (char *)arg5;
    if (arg6) argv[i++] = (char *)arg6;
    argv[i] = NULL;

    char *captured = NULL;
    int rc = run_command_capture(argv, &captured);
    if (rc != 0) {
        if (captured && *captured) set_error("%s", captured);
        free(captured);
        return -1.0;
    }

    double avg = parse_avg_ns(captured ? captured : "");
    if (avg < 0.0) {
        set_error("Failed to parse bench output: %s", captured ? captured : "(empty)");
        free(captured);
        return -1.0;
    }

    free(captured);
    return avg;
}

int bridge_invert(const uint8_t *input, uint8_t *output, int size) {
    if (!input || !output || size <= 0) {
        set_error("bridge_invert: invalid arguments");
        return -1;
    }

    char dir[PATH_MAX], in_path[PATH_MAX], out_path[PATH_MAX];
    if (make_temp_paths(dir, sizeof(dir), in_path, sizeof(in_path), out_path, sizeof(out_path), true) != 0) {
        return -1;
    }

    char size_s[32];
    snprintf(size_s, sizeof(size_s), "%d", size);

    int rc = -1;
    if (write_file(in_path, input, (size_t)size) == 0 &&
        invoke_riscv_normal("invert", in_path, out_path, size_s, NULL) == 0 &&
        read_file_exact(out_path, output, (size_t)size) == 0) {
        rc = 0;
    }

    cleanup_temp(dir, in_path, out_path);
    return rc;
}

int bridge_threshold(const uint8_t *input, uint8_t *output, int size, uint8_t T) {
    if (!input || !output || size <= 0) {
        set_error("bridge_threshold: invalid arguments");
        return -1;
    }

    char dir[PATH_MAX], in_path[PATH_MAX], out_path[PATH_MAX];
    if (make_temp_paths(dir, sizeof(dir), in_path, sizeof(in_path), out_path, sizeof(out_path), true) != 0) {
        return -1;
    }

    char size_s[32], t_s[32];
    snprintf(size_s, sizeof(size_s), "%d", size);
    snprintf(t_s, sizeof(t_s), "%u", (unsigned)T);

    int rc = -1;
    if (write_file(in_path, input, (size_t)size) == 0 &&
        invoke_riscv_normal("threshold", in_path, out_path, size_s, t_s) == 0 &&
        read_file_exact(out_path, output, (size_t)size) == 0) {
        rc = 0;
    }

    cleanup_temp(dir, in_path, out_path);
    return rc;
}

int bridge_brightness(const uint8_t *input, uint8_t *output, int size, int B) {
    if (!input || !output || size <= 0) {
        set_error("bridge_brightness: invalid arguments");
        return -1;
    }

    char dir[PATH_MAX], in_path[PATH_MAX], out_path[PATH_MAX];
    if (make_temp_paths(dir, sizeof(dir), in_path, sizeof(in_path), out_path, sizeof(out_path), true) != 0) {
        return -1;
    }

    char size_s[32], b_s[32];
    snprintf(size_s, sizeof(size_s), "%d", size);
    snprintf(b_s, sizeof(b_s), "%d", B);

    int rc = -1;
    if (write_file(in_path, input, (size_t)size) == 0 &&
        invoke_riscv_normal("brightness", in_path, out_path, size_s, b_s) == 0 &&
        read_file_exact(out_path, output, (size_t)size) == 0) {
        rc = 0;
    }

    cleanup_temp(dir, in_path, out_path);
    return rc;
}

int bridge_blur(const uint8_t *input, uint8_t *output, int width, int height) {
    if (!input || !output || width <= 0 || height <= 0) {
        set_error("bridge_blur: invalid arguments");
        return -1;
    }

    size_t size = (size_t)width * (size_t)height;

    char dir[PATH_MAX], in_path[PATH_MAX], out_path[PATH_MAX];
    if (make_temp_paths(dir, sizeof(dir), in_path, sizeof(in_path), out_path, sizeof(out_path), true) != 0) {
        return -1;
    }

    char w_s[32], h_s[32];
    snprintf(w_s, sizeof(w_s), "%d", width);
    snprintf(h_s, sizeof(h_s), "%d", height);

    int rc = -1;
    if (write_file(in_path, input, size) == 0 &&
        invoke_riscv_normal("blur", in_path, out_path, w_s, h_s) == 0 &&
        read_file_exact(out_path, output, size) == 0) {
        rc = 0;
    }

    cleanup_temp(dir, in_path, out_path);
    return rc;
}

double bridge_bench_invert(const uint8_t *input, int size, int iterations) {
    if (!input || size <= 0 || iterations <= 0) {
        set_error("bridge_bench_invert: invalid arguments");
        return -1.0;
    }

    char dir[PATH_MAX], in_path[PATH_MAX], out_path[PATH_MAX];
    if (make_temp_paths(dir, sizeof(dir), in_path, sizeof(in_path), out_path, sizeof(out_path), false) != 0) {
        return -1.0;
    }

    char size_s[32], it_s[32];
    snprintf(size_s, sizeof(size_s), "%d", size);
    snprintf(it_s, sizeof(it_s), "%d", iterations);

    double avg = -1.0;
    if (write_file(in_path, input, (size_t)size) == 0) {
        avg = invoke_riscv_bench("invert", in_path, size_s, it_s, NULL);
    }

    cleanup_temp(dir, in_path, out_path);
    return avg;
}

double bridge_bench_threshold(const uint8_t *input, int size, uint8_t T, int iterations) {
    if (!input || size <= 0 || iterations <= 0) {
        set_error("bridge_bench_threshold: invalid arguments");
        return -1.0;
    }

    char dir[PATH_MAX], in_path[PATH_MAX], out_path[PATH_MAX];
    if (make_temp_paths(dir, sizeof(dir), in_path, sizeof(in_path), out_path, sizeof(out_path), false) != 0) {
        return -1.0;
    }

    char size_s[32], t_s[32], it_s[32];
    snprintf(size_s, sizeof(size_s), "%d", size);
    snprintf(t_s, sizeof(t_s), "%u", (unsigned)T);
    snprintf(it_s, sizeof(it_s), "%d", iterations);

    double avg = -1.0;
    if (write_file(in_path, input, (size_t)size) == 0) {
        avg = invoke_riscv_bench("threshold", in_path, size_s, t_s, it_s);
    }

    cleanup_temp(dir, in_path, out_path);
    return avg;
}

double bridge_bench_brightness(const uint8_t *input, int size, int B, int iterations) {
    if (!input || size <= 0 || iterations <= 0) {
        set_error("bridge_bench_brightness: invalid arguments");
        return -1.0;
    }

    char dir[PATH_MAX], in_path[PATH_MAX], out_path[PATH_MAX];
    if (make_temp_paths(dir, sizeof(dir), in_path, sizeof(in_path), out_path, sizeof(out_path), false) != 0) {
        return -1.0;
    }

    char size_s[32], b_s[32], it_s[32];
    snprintf(size_s, sizeof(size_s), "%d", size);
    snprintf(b_s, sizeof(b_s), "%d", B);
    snprintf(it_s, sizeof(it_s), "%d", iterations);

    double avg = -1.0;
    if (write_file(in_path, input, (size_t)size) == 0) {
        avg = invoke_riscv_bench("brightness", in_path, size_s, b_s, it_s);
    }

    cleanup_temp(dir, in_path, out_path);
    return avg;
}

double bridge_bench_blur(const uint8_t *input, int width, int height, int iterations) {
    if (!input || width <= 0 || height <= 0 || iterations <= 0) {
        set_error("bridge_bench_blur: invalid arguments");
        return -1.0;
    }

    size_t size = (size_t)width * (size_t)height;

    char dir[PATH_MAX], in_path[PATH_MAX], out_path[PATH_MAX];
    if (make_temp_paths(dir, sizeof(dir), in_path, sizeof(in_path), out_path, sizeof(out_path), false) != 0) {
        return -1.0;
    }

    char w_s[32], h_s[32], it_s[32];
    snprintf(w_s, sizeof(w_s), "%d", width);
    snprintf(h_s, sizeof(h_s), "%d", height);
    snprintf(it_s, sizeof(it_s), "%d", iterations);

    double avg = -1.0;
    if (write_file(in_path, input, size) == 0) {
        avg = invoke_riscv_bench("blur", in_path, w_s, h_s, it_s);
    }

    cleanup_temp(dir, in_path, out_path);
    return avg;
}