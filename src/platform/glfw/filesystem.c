#include "platform/filesystem.h"
#include "core/memory_manager.h"
#include "utility/log.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

bool filesystem_check_file(const char *filename, const char *ext) {
    if (!filename)
        return false;

    // Check for path traversal
    if (strstr(filename, "..") || strstr(filename, "/") || strstr(filename, "\\") ||
        filename[0] == '.' || filename[0] == '/') {
        return false;
    }

    // Only allow .lua extension
    const char *foundExt = strrchr(filename, '.');
    if (!foundExt || strcmp(foundExt, ext) != 0) {
        return false;
    }

    return true;
}

char *filesystem_get_resource(const char *filename) {
    if (!filename)
        return NULL;

    // Cross-platform approach: look for resources in the resources/ directory
    // next to the executable First try to get the executable path using
    // platform-specific methods

    char exe_path[4096];
    char *exe_dir = NULL;

#ifdef __linux__
    // Linux: check APPDIR env var first, fallback to /proc/self/exe
    const char *appdir = getenv("APPDIR");
    if (appdir) {
        exe_dir = appdir;
    } else {
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len != -1) {
            exe_path[len] = '\0';
            exe_dir = exe_path;
        }
    }
#elif defined(__APPLE__)
    // macOS: use _NSGetExecutablePath
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) == 0) {
        exe_dir = exe_path;
    }
#endif

    if (exe_dir) {
        // Find the directory containing the executable
        char *last_slash = strrchr(exe_dir, '/');
        if (last_slash) {
            *last_slash = '\0'; // Remove executable name, keep directory path

            // Construct the full resource path:
            // executable_dir/resources/filename
            size_t resource_path_len =
                strlen(exe_dir) + strlen("/resources/") + strlen(filename) + 1;
            char *resource_path = memory_manager.malloc(resource_path_len, MMTAG_GENERAL);
            if (resource_path) {
                snprintf(resource_path, resource_path_len, "%s/resources/%s", exe_dir, filename);

                // Check if the file exists
                FILE *test_file = fopen(resource_path, "r");
                if (test_file) {
                    fclose(test_file);
                    char *result = memory_manager.strdup(resource_path, MMTAG_GENERAL);
                    memory_manager.free(resource_path);
                    log_debug("CROSS_PLATFORM", "Resource file found: %s", result);
                    return result;
                }
                memory_manager.free(resource_path);
            }
        }
    }

    // Fallback: return the filename as-is if we can't construct the resource
    // path
    return memory_manager.strdup(filename, MMTAG_GENERAL);
}
