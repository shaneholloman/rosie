#ifndef SPM_DOWNLOAD_H
#define SPM_DOWNLOAD_H

#include <stddef.h>

typedef struct {
    char *owner;
    char *repo;
    char *ref;       // Branch or tag, defaults to "main"
} PackageSpec;

// Parse "owner/repo" or "owner/repo@ref" into PackageSpec
PackageSpec *package_spec_parse(const char *spec);
void package_spec_free(PackageSpec *spec);

// Build GitHub tarball URL from package spec
char *build_tarball_url(const PackageSpec *spec);

// Download URL to a file, returns 0 on success
int download_file(const char *url, const char *output_path);

// Initialize/cleanup curl (call once at program start/end)
int download_init(void);
void download_cleanup(void);

#endif // SPM_DOWNLOAD_H
