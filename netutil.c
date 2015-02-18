#include <MacTCP.h>
#include <MacTypes.h>

#include <stdio.h>
#include <string.h>

#include "irc.h"
#include "netutil.h"
#include "tcp.h"

/* Formatted writing to StreamPtrs */
/* Need to make sure we don't overflow buf */
OSErr swritef(StreamPtr stream, const char *fmt, ...)
{
	static char buf[IRC_LINE_MAX];
	va_list list;
	int len;
	
	va_start(list, fmt);
	len = vsprintf(buf, fmt, list);
	va_end(list);
	
	return tcp_write(stream, buf, len);
}

/* THIS ASSUMES \R\N WHICH RFC 1459 REQUIRES BUT IDK IF ALL SERVERS COMPLY */
/* Reads as many lines as possible from the network and calls process_line on each */
/* Any unfinished lines are left in 'buf' for the next poll operation */
void spoll(StreamPtr stream, void (*process_line)(char *))
{
	static char buf[IRC_LINE_MAX];
	static unsigned short written;
	unsigned short read;
	char *start, *cr;

	OSErr err;
	
	// First, read as much as possible into the buffer
	while(written < IRC_LINE_MAX && tcp_available(stream) != 0) {
		read = IRC_LINE_MAX - written;
		err = tcp_read(stream, buf + written, &read);
		if(err != noErr) {
			log("read error %d", err);
			return;
		}
		written += read;
	}
	
	// Now, look for \r\n, replace with \0, and process each line
	start = buf;
	while((cr = strchr(start, '\r')) != NULL) {
		*cr = '\0';
		process_line(start);
		written -= cr - start + 2;
		start = cr + 2; // skip \n
	}
	
	// Finally, move any remaining characters to the beginning of the buffer
	// for the next read operation
	strncpy(buf, start, written);
}
