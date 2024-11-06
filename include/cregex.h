#ifndef __REGEX_H
#define __REGEX_H 1

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @return If @a str matches @a pattern
*/
int string_matches(const char *str, const char *pattern);

#ifdef __cplusplus
}
#endif

#endif
