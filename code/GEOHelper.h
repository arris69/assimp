/*
 * GEOHelper.h
 *
 *  Created on: 17.09.2014
 *      Author: zsolt
 */

#ifndef GEOHELPER_H_
#define GEOHELPER_H_

#include "BaseImporter.h"
#include "assimp/ProgressHandler.hpp"
#include "assimp/types.h"
#include <vector>

#include "ParsingUtils.h"
#include "fast_atof.h"

#include "TinyFormatter.h"

#include "GEOColorTable.h"

#include <errno.h>

inline unsigned int hexstrtoul10(const char* in, const char** out = 0) {
	/* http://stackoverflow.com/questions/4132318/how-to-convert-hex-string-to-unsigned-64bit-uint64-t-integer-in-a-fast-and-saf answer 3. */
	//char *in, *end;
	unsigned long long result;
	errno = 0;
	// if (!isxdigit(in[0]) || (in[1] && !isxdigit(in[1])))
	// if (in[0]=='0' && in[1])
	//result = strtoull(in, &end, 16);
	result = strtoull(in, (char **) &out, 16);
	if (result == 0 && *out == in) {
		printf("/* str was not a number */ %s\n", in);
	} else if (result == ULLONG_MAX && errno) {
		printf("/* the value of str does not fit in unsigned long long */ %s\n",
				in);
	} else if (*out) {
		printf(
				"/* str began with a number but has junk left over at the end */ %s\n",
				in);
	} else {
		printf("/* str was a number */ %s <-> %llu rest of line %s\n", in,
				result, *out);
	}
	return (unsigned int) result;
}

// helper macro to determine the size of an array
#if (!defined ARRAYSIZE)
#	define ARRAYSIZE(_array) (int(sizeof(_array) / sizeof(_array[0])))
#endif

#if _MSC_VER
#	define strtoull _strtoui64
#	ifndef NAN
    	static const unsigned long __nan[2] = {0xffffffff, 0x7fffffff};
#		define NAN (*(const float *) __nan)
#	endif
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER) /* TODO: more sense here... */
//#	define snprintf _snprintf
//#	define vsnprintf _vsnprintf
//#	define strcasecmp _stricmp
//#	define strncasecmp _strnicmp
/*
 Windows: stricmp()
 warning: implicit declaration of function ‘_stricmp’
 */
const char *strcasestr(const char *arg1, const char *arg2) {
	const char *a, *b;

	for(;*arg1;*arg1++) {
		a = arg1;
		b = arg2;
		while((*a++ | 32) == (*b++ | 32))
		if(!*b)
		return (arg1);
	}
	return(NULL);
}
/*
 Borland: strcmpi()
 */
int strcasecmp(const char *s1, const char *s2) {
	while(*s1 != 0 && tolower(*s1) == tolower(*s2)) {
		++s1;
		++s2;
	}
	return (*s2 == 0) ? (*s1 != 0) : (*s1 == 0) ? -1 : (tolower(*s1) - tolower(*s2));
}
#endif

#endif /* GEOHELPER_H_ */
