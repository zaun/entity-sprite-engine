#include <stdbool.h>
#include <string.h>
#ifdef __APPLE__
    #include <CoreFoundation/CoreFoundation.h>
#endif
#include "core/memory_manager.h"
#include "platform/filesystem.h"
#include "utility/log.h"

bool filesystem_check_file(const char *filename, const char *ext) {
    if (!filename) return false;
    
    // Check for path traversal
    if (strstr(filename, "..") || 
        strstr(filename, "/") || 
        strstr(filename, "\\") ||
        filename[0] == '.' ||
        filename[0] == '/') {
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
    if (!filename) return NULL;

    #ifdef __APPLE__
        // Get a reference to the main application bundle.
        CFBundleRef mainBundle = CFBundleGetMainBundle();
        if (!mainBundle) {
            return NULL;
        }

        // Convert the filename string into a CFStringRef.
        CFStringRef filenameRef = CFStringCreateWithCString(kCFAllocatorDefault, filename, kCFStringEncodingUTF8);
        if (!filenameRef) {
            return NULL;
        }

        // Find the last dot to split the name and extension.
        const char* lastDot = strrchr(filename, '.');
        CFStringRef nameRef = NULL;
        CFStringRef typeRef = NULL;

        if (lastDot) {
            // Filename has an extension.
            char nameBuffer[1024];
            char typeBuffer[256];
            size_t nameLength = lastDot - filename;

            // Ensure buffers are large enough.
            if (nameLength >= sizeof(nameBuffer) || (strlen(lastDot + 1) >= sizeof(typeBuffer))) {
                CFRelease(filenameRef);
                return NULL;
            }

            strncpy(nameBuffer, filename, nameLength);
            nameBuffer[nameLength] = '\0';
            strcpy(typeBuffer, lastDot + 1);

            nameRef = CFStringCreateWithCString(kCFAllocatorDefault, nameBuffer, kCFStringEncodingUTF8);
            typeRef = CFStringCreateWithCString(kCFAllocatorDefault, typeBuffer, kCFStringEncodingUTF8);
        } else {
            // Filename has no extension.
            nameRef = CFStringCreateWithCString(kCFAllocatorDefault, filename, kCFStringEncodingUTF8);
            typeRef = NULL; // As per the NSBundle documentation, nil for no extension.
        }

        if (!nameRef) {
            if (filenameRef) CFRelease(filenameRef);
            return NULL;
        }

        // Get a reference to the file's URL from the bundle.
        CFURLRef fileURL = CFBundleCopyResourceURL(mainBundle, nameRef, typeRef, NULL);

        if (nameRef) CFRelease(nameRef);
        if (typeRef) CFRelease(typeRef);
        if (filenameRef) CFRelease(filenameRef);

        if (!fileURL) {
            return NULL;
        }

        // Convert the URL reference into a path string reference.
        CFStringRef pathRef = CFURLCopyFileSystemPath(fileURL, kCFURLPOSIXPathStyle);
        CFRelease(fileURL);

        if (!pathRef) {
            return NULL;
        }

        // Convert the CFStringRef path to a C string.
        CFIndex length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(pathRef), kCFStringEncodingUTF8);
        char* cPath = malloc(length + 1);
        if (!cPath) {
            CFRelease(pathRef);
            return NULL;
        }

        if (!CFStringGetCString(pathRef, cPath, length + 1, kCFStringEncodingUTF8)) {
            free(cPath);
            CFRelease(pathRef);
            return NULL;
        }
        CFRelease(pathRef);

        // Use the specified memory manager to duplicate the path.
        char* result = memory_manager.strdup(cPath, MMTAG_GENERAL);
        log_debug("APP", "File: %s", result);
        free(cPath);

        return result;
    #else
        return memory_manager.strdup(filename, MMTAG_GENERAL);
    #endif
}
