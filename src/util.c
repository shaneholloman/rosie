#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>

bool g_verbose = false;

// Memory management
void *spm_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "rosie: out of memory\n");
        exit(1);
    }
    return ptr;
}

void *spm_realloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        fprintf(stderr, "rosie: out of memory\n");
        exit(1);
    }
    return new_ptr;
}

void spm_free(void *ptr) {
    free(ptr);
}

// String utilities
char *str_dup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = spm_malloc(len + 1);
    memcpy(dup, s, len + 1);
    return dup;
}

char *str_trim(char *s) {
    if (!s) return NULL;

    // Trim leading whitespace
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') {
        s++;
    }

    if (*s == '\0') return s;

    // Trim trailing whitespace
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }
    end[1] = '\0';

    return s;
}

bool str_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

bool str_ends_with(const char *s, const char *suffix) {
    if (!s || !suffix) return false;
    size_t s_len = strlen(s);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > s_len) return false;
    return strcmp(s + s_len - suffix_len, suffix) == 0;
}

// Path utilities
char *get_home_dir(void) {
    const char *home = getenv("HOME");
    if (home) return str_dup(home);

    struct passwd *pw = getpwuid(getuid());
    if (pw) return str_dup(pw->pw_dir);

    return NULL;
}

char *get_temp_dir(void) {
    const char *tmp = getenv("TMPDIR");
    if (tmp) return str_dup(tmp);

    tmp = getenv("TMP");
    if (tmp) return str_dup(tmp);

    tmp = getenv("TEMP");
    if (tmp) return str_dup(tmp);

    return str_dup("/tmp");
}

char *path_join(const char *base, const char *name) {
    if (!base || !name) return NULL;

    size_t base_len = strlen(base);
    size_t name_len = strlen(name);

    // Remove trailing slash from base
    while (base_len > 0 && base[base_len - 1] == '/') {
        base_len--;
    }

    // Remove leading slash from name
    while (*name == '/') {
        name++;
        name_len--;
    }

    char *result = spm_malloc(base_len + 1 + name_len + 1);
    memcpy(result, base, base_len);
    result[base_len] = '/';
    memcpy(result + base_len + 1, name, name_len);
    result[base_len + 1 + name_len] = '\0';

    return result;
}

bool dir_exists(const char *path) {
    if (!path) return false;
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool file_exists(const char *path) {
    if (!path) return false;
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int make_dirs(const char *path) {
    if (!path) return -1;
    if (dir_exists(path)) return 0;

    char *tmp = str_dup(path);
    char *p = tmp;

    // Skip leading slash
    if (*p == '/') p++;

    while (*p) {
        if (*p == '/') {
            *p = '\0';
            if (!dir_exists(tmp)) {
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                    spm_free(tmp);
                    return -1;
                }
            }
            *p = '/';
        }
        p++;
    }

    // Create final directory
    if (!dir_exists(tmp)) {
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            spm_free(tmp);
            return -1;
        }
    }

    spm_free(tmp);
    return 0;
}

int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) {
        log_error("Cannot open source file: %s", src);
        return -1;
    }

    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        log_error("Cannot open destination file: %s", dst);
        return -1;
    }

    char buf[8192];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            log_error("Write error: %s", dst);
            return -1;
        }
    }

    fclose(in);
    fclose(out);

    // Preserve executable bit
    struct stat st;
    if (stat(src, &st) == 0) {
        chmod(dst, st.st_mode);
    }

    return 0;
}

int copy_dir_recursive(const char *src, const char *dst) {
    if (make_dirs(dst) != 0) {
        return -1;
    }

    DIR *dir = opendir(src);
    if (!dir) {
        log_error("Cannot open directory: %s", src);
        return -1;
    }

    struct dirent *entry;
    int ret = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char *src_path = path_join(src, entry->d_name);
        char *dst_path = path_join(dst, entry->d_name);

        struct stat st;
        if (stat(src_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (copy_dir_recursive(src_path, dst_path) != 0) {
                    ret = -1;
                }
            } else if (S_ISREG(st.st_mode)) {
                if (copy_file(src_path, dst_path) != 0) {
                    ret = -1;
                }
            }
        }

        spm_free(src_path);
        spm_free(dst_path);

        if (ret != 0) break;
    }

    closedir(dir);
    return ret;
}

// Logging
void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

void log_error(const char *fmt, ...) {
    fprintf(stderr, "rosie: error: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void log_debug(const char *fmt, ...) {
    if (!g_verbose) return;
    printf("[debug] ");
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}
