#include <string.h>

#include "debug.h"

/* define the tmp buffer size (change if desired) */
#define PRINTF_BUF 80 

#define WIDTH	        16
#define my_isprint(x)   (x >= 0x20 && x <= 0x7e)

void dhexdump(size_t saddr, const void *vbuf, size_t count)
{
    const unsigned char *buf = (const unsigned char *) vbuf;
    size_t pos = 0;
    size_t i;
    char buf1[10 + (WIDTH * 4)];

    while (pos < count) {
	size_t bound = count - pos;
	char *b = buf1;

	if (bound > WIDTH)
	    bound = WIDTH;

	b += sprintf(b, "[%4.4zx] ", saddr + pos);

	for (i = 0; i < bound; i++) {
	    b += sprintf(b, "%2.2x ", buf[i]);
	}

	for (; i < WIDTH; i++) {
	    b += sprintf(b, "   ");
	}

	for (i = 0; i < bound; i++) {
	    b += sprintf(b, "%c", my_isprint(buf[i]) ? buf[i] : '.');
	}

	dmsg("%s\n", buf1);

	pos += bound;
	buf += bound;
    }
}

void dmsg(const char *format, ...)
{
    char *p, buf[PRINTF_BUF];
    va_list ap;

    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    for (p = &buf[0]; *p; p++) { // emulate cooked mode for newlines
	if (*p == '\n')
	    debug_write("\r", 1);
	debug_write(p, 1);
    }
    va_end(ap);
}
