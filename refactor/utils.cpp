//
// Created by kamenev on 10.01.16.
//

#include <sys/socket.h>
#include <string.h>
#include "utils.h"
#include "debug.h"
#include "handle.h"
bool
str_to_uint16(const char *str, uint16_t *res)
{
    char *end;
    errno = 0;
    intmax_t val = strtoimax(str, &end, 10);
    if (errno == ERANGE || val < 0 || val > UINT16_MAX || end == str || *end != '\0')
        return false;
    *res = (uint16_t) val;
    return true;
}
const std::string currentDateTime()
{
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}
const std::string currentTime()
{
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%X", &tstruct);

    return buf;
}
int getSocketError(const handle& fd)
{
    int error = 0;
    socklen_t errlen = sizeof(error);
    if (getsockopt(fd.get_raw(),
                   SOL_SOCKET,
                   SO_ERROR,
                   (void *) &error,
                   &errlen) == 0) {
#ifdef DEBUG
        if (error != 0) {

            LOG("(%d): Socket error = %s\n", fd.get_raw(), strerror(error));
        }
#endif
    }
    return error;
}