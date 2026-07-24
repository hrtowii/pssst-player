#include "util.h"
#include <string.h>
#include <stdio.h>
int join_path(char* out, size_t outsz, const char* a, const char* b)
{
	if (!out || outsz == 0 || !a || !b)
		return -1;

	if (a[0] == '\0') {
		int n = snprintf(out, outsz, "%s", b);
		if (n < 0 || (size_t)n >= outsz)
			return -1;
		return 0;
	}

	if (b[0] == '\0') {
		int n = snprintf(out, outsz, "%s", a);
		if (n < 0 || (size_t)n >= outsz)
			return -1;
		return 0;
	}

	size_t al = strlen(a);

	/* avoid double slashes */
	if (a[al - 1] == '/')
		return snprintf(out, outsz, "%s%s", a, b) < (int)outsz ? 0 : -1;

	return snprintf(out, outsz, "%s/%s", a, b) < (int)outsz ? 0 : -1;
}
