#ifndef SPM_UTIL_H
#define SPM_UTIL_H

#include <stddef.h>
#include <stdbool.h>

// Path manipulation
char *path_join(const char *base, const char *name);
char *get_home_dir(void);
char *get_temp_dir(void);
bool dir_exists(const char *path);
bool file_exists(const char *path);
int make_dirs(const char *path);
int copy_file(const char *src, const char *dst);
int copy_dir_recursive(const char *src, const char *dst);

// String utilities
char *str_dup(const char *s);
char *str_trim(char *s);
bool str_starts_with(const char *s, const char *prefix);
bool str_ends_with(const char *s, const char *suffix);

// Memory management
void *spm_malloc(size_t size);
void *spm_realloc(void *ptr, size_t size);
void spm_free(void *ptr);

// Logging
void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);
void log_debug(const char *fmt, ...);

extern bool g_verbose;

#endif // SPM_UTIL_H
