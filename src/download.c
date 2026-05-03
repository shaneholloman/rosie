#include "download.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

static bool curl_initialized = false;

int download_init(void) {
    if (curl_initialized) return 0;

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        log_error("Failed to initialize curl");
        return -1;
    }

    curl_initialized = true;
    return 0;
}

void download_cleanup(void) {
    if (curl_initialized) {
        curl_global_cleanup();
        curl_initialized = false;
    }
}

PackageSpec *package_spec_parse(const char *spec) {
    if (!spec) return NULL;

    PackageSpec *ps = spm_malloc(sizeof(PackageSpec));
    ps->owner = NULL;
    ps->repo = NULL;
    ps->ref = NULL;
    ps->ref_explicit = false;

    // Make a working copy
    char *work = str_dup(spec);

    // Check for @ref suffix
    char *at = strchr(work, '@');
    if (at) {
        *at = '\0';
        ps->ref = str_dup(at + 1);
        ps->ref_explicit = true;
    } else {
        ps->ref = str_dup("main");
        ps->ref_explicit = false;
    }

    // Parse owner/repo
    char *slash = strchr(work, '/');
    if (!slash) {
        log_error("Invalid package spec: %s (expected owner/repo)", spec);
        spm_free(work);
        spm_free(ps->ref);
        spm_free(ps);
        return NULL;
    }

    *slash = '\0';
    ps->owner = str_dup(work);
    ps->repo = str_dup(slash + 1);

    spm_free(work);

    if (!ps->owner[0] || !ps->repo[0]) {
        log_error("Invalid package spec: %s (empty owner or repo)", spec);
        package_spec_free(ps);
        return NULL;
    }

    return ps;
}

void package_spec_free(PackageSpec *spec) {
    if (!spec) return;
    spm_free(spec->owner);
    spm_free(spec->repo);
    spm_free(spec->ref);
    spm_free(spec);
}

char *build_tarball_url(const PackageSpec *spec, RefKind kind) {
    if (!spec) return NULL;

    const char *kind_segment = (kind == REF_KIND_TAG) ? "tags" : "heads";
    const char *fmt = "https://github.com/%s/%s/archive/refs/%s/%s.tar.gz";

    size_t len = strlen(fmt) + strlen(spec->owner) + strlen(spec->repo)
                 + strlen(kind_segment) + strlen(spec->ref) + 1;
    char *url = spm_malloc(len);
    snprintf(url, len, fmt, spec->owner, spec->repo, kind_segment, spec->ref);

    return url;
}

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    FILE *fp = (FILE *)userp;
    return fwrite(contents, size, nmemb, fp);
}

// Internal: returns 0 on transport success and writes HTTP code to *out_http_code.
// Returns -1 on transport failure (curl error). Does not log on HTTP errors;
// caller decides what to do with the status.
static int download_file_internal(const char *url, const char *output_path, long *out_http_code) {
    if (!curl_initialized) {
        log_error("Curl not initialized");
        return -1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        log_error("Failed to create curl handle");
        return -1;
    }

    FILE *fp = fopen(output_path, "wb");
    if (!fp) {
        log_error("Cannot create file: %s", output_path);
        curl_easy_cleanup(curl);
        return -1;
    }

    log_debug("Downloading: %s", url);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "rosie/1.0");
    // Note: no CURLOPT_FAILONERROR — we want to inspect the HTTP code ourselves
    // so callers (like the branch-then-tag fallback) can react to a 404 quietly.

    if (g_verbose) {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }

    CURLcode res = curl_easy_perform(curl);

    fclose(fp);

    if (res != CURLE_OK) {
        log_error("Download failed: %s", curl_easy_strerror(res));
        remove(output_path);
        curl_easy_cleanup(curl);
        return -1;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (out_http_code) *out_http_code = http_code;

    curl_easy_cleanup(curl);

    if (http_code >= 400) {
        // Caller will decide whether to log; remove the partial file regardless.
        remove(output_path);
        return 0;  // transport ok; HTTP failure is signaled via http_code
    }

    log_debug("Downloaded to: %s", output_path);
    return 0;
}

int download_file(const char *url, const char *output_path) {
    long http_code = 0;
    if (download_file_internal(url, output_path, &http_code) != 0) {
        return -1;
    }
    if (http_code >= 400) {
        log_error("HTTP error: %ld", http_code);
        return -1;
    }
    return 0;
}

int download_package_tarball(const PackageSpec *spec, const char *output_path) {
    if (!spec) return -1;

    // Try as a branch first.
    char *url = build_tarball_url(spec, REF_KIND_BRANCH);
    long http_code = 0;
    int transport = download_file_internal(url, output_path, &http_code);
    spm_free(url);

    if (transport != 0) {
        return -1;  // network/curl error already logged
    }
    if (http_code < 400) {
        return 0;  // success as a branch
    }
    if (http_code != 404) {
        log_error("HTTP error: %ld", http_code);
        return -1;
    }

    // Branch path 404'd — try as a tag.
    log_debug("Ref '%s' not found as branch, trying as tag", spec->ref);
    url = build_tarball_url(spec, REF_KIND_TAG);
    transport = download_file_internal(url, output_path, &http_code);
    spm_free(url);

    if (transport != 0) {
        return -1;
    }
    if (http_code >= 400) {
        log_error("Ref '%s' not found as branch or tag (HTTP %ld)", spec->ref, http_code);
        return -1;
    }
    return 0;
}
