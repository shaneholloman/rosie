#ifndef SPM_LINK_H
#define SPM_LINK_H

#include <stdbool.h>

// Create a "symlink-like" reference at link_path pointing to target.
//
// POSIX (Linux, macOS, FreeBSD; also the path taken by the WASM build):
//   symlink() straight through. Works for both files and directories. target
//   may be relative or absolute; passed through unchanged.
//
// Windows (when _WIN32 is defined — i.e. native MSVC/MinGW build):
//   - is_dir=true:  creates a junction (NTFS reparse point, IO_REPARSE_TAG_
//     MOUNT_POINT). Requires no elevation. Target is resolved to an absolute
//     path first since junctions don't support relative targets.
//   - is_dir=false: tries CreateHardLinkW; falls back to a plain file copy if
//     hard-linking fails (different volume, etc.). Junctions can't link files.
//
// Returns 0 on success, -1 on failure (log_error already emitted on failure).
int rosie_create_link(const char *target, const char *link_path, bool is_dir);

#endif // SPM_LINK_H
