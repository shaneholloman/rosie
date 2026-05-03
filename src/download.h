#ifndef SPM_DOWNLOAD_H
#define SPM_DOWNLOAD_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char *owner;
    char *repo;
    char *ref;            // Branch or tag, defaults to "main" if not given
    bool ref_explicit;    // true if user passed @ref; false if defaulted
} PackageSpec;

// Parse "owner/repo" or "owner/repo@ref" into PackageSpec
PackageSpec *package_spec_parse(const char *spec);
void package_spec_free(PackageSpec *spec);

typedef enum {
    REF_KIND_BRANCH,   // archive/refs/heads/<ref>.tar.gz
    REF_KIND_TAG,      // archive/refs/tags/<ref>.tar.gz
} RefKind;

// Build GitHub tarball URL from package spec for a specific ref kind
char *build_tarball_url(const PackageSpec *spec, RefKind kind);

// Download URL to a file, returns 0 on success
int download_file(const char *url, const char *output_path);

// Download a package tarball, trying branch first and falling back to tag on 404.
// Used when we don't yet know whether the ref names a branch or a tag.
int download_package_tarball(const PackageSpec *spec, const char *output_path);

// Initialize/cleanup curl (call once at program start/end)
int download_init(void);
void download_cleanup(void);

#endif // SPM_DOWNLOAD_H
