#include <stdio.h>

#include "stolenlib.h"

/* TODO maybe unaligned is okay not sure */
#define STB_SPRINTF_NOUNALIGNED

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

static inline int myisdigit(int c) {
	return (unsigned)c-'0' < 10;
}

/* atoi stolen from musl */
int myatoi(const char *s) {
	int n=0, neg=0;
	while (*s == ' ') s++;
	switch (*s) {
	case '-': neg=1;
	case '+': s++;
	}
	/* Compute n as a negative number to avoid overflow on INT_MIN */
	while (myisdigit(*s))
		n = 10*n - (*s++ - '0');
	return neg ? n : -n;
}

#if 0
char *strncpy(char *restrict d, const char *restrict s, size_t n) {
	for (; n && (*d=*s); n--, s++, d++);
	memset(d, 0, n);
	return d;
}
#endif

void myprintf(const char *format, ...) {
#define MAX_TEXT_BUFFER_LENGTH 1024
	char buf[MAX_TEXT_BUFFER_LENGTH];
	va_list args;
	va_start(args, format);
	stbsp_vsnprintf(buf, MAX_TEXT_BUFFER_LENGTH, format, args);
	va_end(args);
	fputs(buf, stdout);
}

#if 0
int fprintf(FILE *restrict stream, const char *restrict format, ...) {
#define MAX_TEXT_BUFFER_LENGTH 1024
	char buf[MAX_TEXT_BUFFER_LENGTH];
	va_list args;
	va_start(args, format);
	stbsp_vsnprintf(buf, MAX_TEXT_BUFFER_LENGTH, format, args);
	va_end(args);
	fputs(buf, stream);
	return 1;
}
#endif

/* Stolen from Raylib */
const char *TextFormat(const char *text, ...) {
#define MAX_TEXT_BUFFER_LENGTH 1024
#define MAX_TEXTFORMAT_BUFFERS 4
	static char buffers[MAX_TEXTFORMAT_BUFFERS][MAX_TEXT_BUFFER_LENGTH] = { 0 };
	static int index = 0;

	char *currentBuffer = buffers[index];
	memset(currentBuffer, 0, MAX_TEXT_BUFFER_LENGTH);

	va_list args;
	va_start(args, text);
	stbsp_vsnprintf(currentBuffer, MAX_TEXT_BUFFER_LENGTH, text, args);
	va_end(args);

	index += 1;
	if (index >= MAX_TEXTFORMAT_BUFFERS) index = 0;

	return currentBuffer;
}
