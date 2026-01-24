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

    // Make a working copy
    char *work = str_dup(spec);

    // Check for @ref suffix
    char *at = strchr(work, '@');
    if (at) {
        *at = '\0';
        ps->ref = str_dup(at + 1);
    } else {
        ps->ref = str_dup("main");
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

char *build_tarball_url(const PackageSpec *spec) {
    if (!spec) return NULL;

    // GitHub tarball URL format:
    // https://github.com/owner/repo/archive/refs/heads/branch.tar.gz
    // or for tags:
    // https://github.com/owner/repo/archive/refs/tags/tag.tar.gz

    const char *fmt = "https://github.com/%s/%s/archive/refs/heads/%s.tar.gz";

    size_t len = strlen(fmt) + strlen(spec->owner) + strlen(spec->repo) + strlen(spec->ref) + 1;
    char *url = spm_malloc(len);
    snprintf(url, len, fmt, spec->owner, spec->repo, spec->ref);

    return url;
}

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    FILE *fp = (FILE *)userp;
    return fwrite(contents, size, nmemb, fp);
}

int download_file(const char *url, const char *output_path) {
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
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    // For debugging
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

    curl_easy_cleanup(curl);

    if (http_code >= 400) {
        log_error("HTTP error: %ld", http_code);
        remove(output_path);
        return -1;
    }

    log_debug("Downloaded to: %s", output_path);
    return 0;
}
