#ifndef TCP_H
#define TCP_H

#include <MacTCP.h>
#include <MacTypes.h>

OSErr tcp_init(void);
StreamPtr tcp_create_stream(void);
OSErr tcp_connect(StreamPtr stream, ip_addr ip, tcp_port port);
OSErr tcp_write(StreamPtr stream, const char *buf, int len);
OSErr tcp_read(StreamPtr stream, Ptr buf, unsigned short *len);
unsigned short tcp_available(StreamPtr stream);
OSErr tcp_release_stream(StreamPtr stream);
void tcp_cleanup(void);

OSErr dns_init(const char *hosts_file);
ip_addr dns_lookup(const char *host);
const char *dns_format_ip(unsigned long ip);
OSErr dns_cleanup(void);

#endif
