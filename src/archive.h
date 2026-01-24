#ifndef SPM_ARCHIVE_H
#define SPM_ARCHIVE_H

// Extract a tarball to a directory
// Returns 0 on success, -1 on failure
int extract_tarball(const char *archive_path, const char *dest_dir);

// Get the root directory name inside the tarball
// (GitHub tarballs have a root dir like "repo-main/")
char *get_archive_root_dir(const char *archive_path);

#endif // SPM_ARCHIVE_H
