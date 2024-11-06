#include <string.h>
#include <stdio.h>

#include "cregex.h"

#define MAX_WORD_SIZE 60

static inline void _advance_one(char **it)
{
	do {
		(*it)++;
	} while (**it != '/' && **it != '\0');
}

int _advance_until(char *str, char *it);

int string_matches(const char *str, const char *pattern)
{
	char buf[MAX_WORD_SIZE];
	size_t pos = 0;

	char *last = (char *)str;
	char *it = (char *)pattern;

	for (; *it != '\0' && *last != '\0'; it++) {
		if (*it != '*' && *it != '+') {
			buf[pos++] = *it;
			continue;
		}

		buf[pos] = '\0';

		char *found = strstr(last, buf);
		if (found != last) {
			return 0;
		}

		last += pos;
		pos = 0;

		if (*it == '*') {
			return _advance_until(last, it);
		}

		_advance_one(&last);
	}

	if (pos != 0) {
		buf[pos] = '\0';
		return strcmp(buf, last) == 0;
	}
	return (*last == '\0' && *it == '\0');
}

int _advance_until(char *str, char *it)
{
	it++;
	if (*it == '\0') {
		return 1;
	}

	size_t len = strspn(it, "/*+");
	it += len;

	len = strcspn(it, "/*+");

	char tmp[MAX_WORD_SIZE];
	strncpy(tmp, it, len);
	tmp[len] = '\0';

	char *found = strstr(str, tmp);
	while (found) {
		if (string_matches(found + len, it + len) == 1) {
			return 1;
		}

		found = strstr(found + 1, tmp);
	}

	return 0;
}
