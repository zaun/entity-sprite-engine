#ifndef ESE_APP_H
#define ESE_APP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

char *filesystem_get_resource(const char *filename);
bool filesystem_check_file(const char *filename, const char *ext);

#ifdef __cplusplus
}
#endif

#endif // ESE_APP_H