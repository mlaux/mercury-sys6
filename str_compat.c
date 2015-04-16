/* Some string functions that aren't available with MPW */
#include <stdlib.h>
#include <string.h>


#include "str_compat.h"


void strncpy_s(char *dest, const char *src, int n)
{
	strncpy(dest, src, n);
	dest[n - 1] = 0;
}

char *strdup(const char *str)
{
	int len;
	char *ret;
	
	len = strlen(str) + 1;
	ret = malloc(len);
	strncpy_s(ret, str, len);
	return ret;
}

void P2C(unsigned char *str)
{
	int len = str[0], k;
	for(k = 0; k < len; ++k)
		str[k] = str[k + 1];
	str[len] = 0;
}
