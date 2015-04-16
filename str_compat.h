#ifndef STR_COMPAT_H
#define STR_COMPAT_H

void strncpy_s(char *dest, const char *src, int n);
char *strdup(const char *str);
void P2C(unsigned char *str);

#endif