#ifndef TEXT_H
#define TEXT_H

static inline int myislower(int c) {
	return (unsigned)c-'a' < 26;
}

static inline int mytoupper(int c) {
	if (myislower(c)) return c & 0x5f;
	return c;
}

int myatoi(const char *s);
const char *TextFormat(const char *text, ...);
void myprintf(const char *format, ...);

#endif
