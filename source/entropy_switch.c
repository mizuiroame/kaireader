// newlib's getentropy needs a _getentropy_r from the system: MuPDF asks it for
// random bytes (fz_memrnd). Here they come from the console's random source.
#ifdef __SWITCH__
#include <errno.h>
#include <reent.h>
#include <stddef.h>

#include <switch.h>

int _getentropy_r(struct _reent *r, void *buffer, size_t length)
{
    if (length > 256) {  // As getentropy promises: no more than 256 bytes a call.
        r->_errno = EIO;
        return -1;
    }
    randomGet(buffer, length);
    return 0;
}
#endif
