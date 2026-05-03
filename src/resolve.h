#ifndef SPM_RESOLVE_H
#define SPM_RESOLVE_H

#include <stdbool.h>
#include "download.h"

typedef struct {
    char *ref;       // e.g. "v1.2.0" or "main"
    char *sha;       // 40-char hex commit SHA
    bool is_tag;     // true if ref came from refs/tags/
} ResolvedRef;

// Query the remote and return the highest semver tag (skipping pre-releases).
// Returns NULL on network error or if no semver-shaped tags exist.
ResolvedRef *resolve_latest_tag(const PackageSpec *spec);

// Resolve a specific ref name (branch or tag) to its SHA.
// Returns NULL if the ref isn't found in the remote's advertisement.
ResolvedRef *resolve_ref(const PackageSpec *spec, const char *ref_name);

void resolved_ref_free(ResolvedRef *r);

#endif // SPM_RESOLVE_H
