#import "platform/filesystem.h"
#import "core/memory_manager.h"
#import <Cocoa/Cocoa.h>

char *filesystem_get_resource(const char *filename) {
  @autoreleasepool {
    NSBundle *bundle = [NSBundle mainBundle];
    NSString *file = [NSString stringWithUTF8String:filename];
    NSString *path =
        [bundle pathForResource:[file stringByDeletingPathExtension]
                         ofType:[file pathExtension]];

    if (!path)
      return NULL;
    char *result =
        memory_manager.strdup([path fileSystemRepresentation], MMTAG_GENERAL);
    return result;
  }
}

bool filesystem_check_file(const char *filename, const char *ext) {
  if (!filename)
    return false;

  // Check for path traversal
  if (strstr(filename, "..") || strstr(filename, "/") ||
      strstr(filename, "\\") || filename[0] == '.' || filename[0] == '/') {
    return false;
  }

  // Only allow .lua extension
  const char *foundExt = strrchr(filename, '.');
  if (!foundExt || strcmp(foundExt, ext) != 0) {
    return false;
  }

  return true;
}
