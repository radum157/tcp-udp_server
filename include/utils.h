#ifndef __UTILS_H
#define __UTILS_H 1

#define DIE(cond, msg)						\
	do {									\
		if (cond) {							\
			perror(msg);					\
			exit(EXIT_FAILURE);				\
		}									\
	} while (0)

#endif
