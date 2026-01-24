#include "archive.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <archive.h>
#include <archive_entry.h>

int extract_tarball(const char *archive_path, const char *dest_dir) {
    struct archive *a = archive_read_new();
    struct archive *ext = archive_write_disk_new();
    struct archive_entry *entry;
    int r;

    archive_read_support_format_tar(a);
    archive_read_support_filter_gzip(a);
    archive_read_support_filter_xz(a);

    archive_write_disk_set_options(ext,
        ARCHIVE_EXTRACT_TIME |
        ARCHIVE_EXTRACT_PERM |
        ARCHIVE_EXTRACT_ACL |
        ARCHIVE_EXTRACT_FFLAGS);
    archive_write_disk_set_standard_lookup(ext);

    r = archive_read_open_filename(a, archive_path, 10240);
    if (r != ARCHIVE_OK) {
        log_error("Cannot open archive: %s", archive_error_string(a));
        archive_read_free(a);
        archive_write_free(ext);
        return -1;
    }

    log_debug("Extracting to: %s", dest_dir);

    while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        const char *current_file = archive_entry_pathname(entry);

        // Construct full output path
        char *full_path = path_join(dest_dir, current_file);
        archive_entry_set_pathname(entry, full_path);

        log_debug("  extracting: %s", current_file);

        r = archive_write_header(ext, entry);
        if (r != ARCHIVE_OK) {
            log_error("Error writing header: %s", archive_error_string(ext));
        } else if (archive_entry_size(entry) > 0) {
            const void *buff;
            size_t size;
            la_int64_t offset;

            while ((r = archive_read_data_block(a, &buff, &size, &offset)) == ARCHIVE_OK) {
                r = archive_write_data_block(ext, buff, size, offset);
                if (r != ARCHIVE_OK) {
                    log_error("Error writing data: %s", archive_error_string(ext));
                    break;
                }
            }
        }

        r = archive_write_finish_entry(ext);
        if (r != ARCHIVE_OK) {
            log_error("Error finishing entry: %s", archive_error_string(ext));
        }

        spm_free(full_path);
    }

    if (r != ARCHIVE_EOF) {
        log_error("Error reading archive: %s", archive_error_string(a));
        archive_read_free(a);
        archive_write_free(ext);
        return -1;
    }

    archive_read_free(a);
    archive_write_free(ext);
    return 0;
}

char *get_archive_root_dir(const char *archive_path) {
    struct archive *a = archive_read_new();
    struct archive_entry *entry;

    archive_read_support_format_tar(a);
    archive_read_support_filter_gzip(a);
    archive_read_support_filter_xz(a);

    if (archive_read_open_filename(a, archive_path, 10240) != ARCHIVE_OK) {
        archive_read_free(a);
        return NULL;
    }

    char *root_dir = NULL;

    if (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *path = archive_entry_pathname(entry);
        if (path) {
            // Find first path component
            const char *slash = strchr(path, '/');
            if (slash) {
                size_t len = slash - path;
                root_dir = spm_malloc(len + 1);
                memcpy(root_dir, path, len);
                root_dir[len] = '\0';
            } else {
                root_dir = str_dup(path);
            }
        }
    }

    archive_read_free(a);
    return root_dir;
}
