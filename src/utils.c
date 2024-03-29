#include "utils.h"

char *append(char *dst, const char *str) {
	while (*str) *dst++ = *str++;
	return dst;
}

int alike(const in *str, const char *cmp) {
	const char *from = cmp;
	while (*cmp && *cmp == (char) *str++) cmp++;
	return *cmp ? 0 : cmp - from;
}

void resolveKind(const in **docPtr, char **fmtPtr, char *kind) {
	const in *doc = *docPtr;
	char *fmt     = *fmtPtr;
	// we rely on parsers to catch `@a` and `@p` themselves
	if (!*kind && !alike(doc, "brief")) *fmt++ = '\n';
	const char *section;
	int skip;
	if ((skip = alike(doc, "param"))) section = "**Parameters:**\n";
	else if ((skip = alike(doc, "return"))) section = "**Returns:**\n";
	else if ((skip = alike(doc, "throw"))) section = "**Throws:**\n";
	else if ((skip = alike(doc, "see"))) section = "**See:**\n";
	else if ((skip = alike(doc, "ingroup"))) {
		doc += skip;
		while (*doc >= ' ') doc++;
		*docPtr = doc;
		*fmtPtr = fmt;
		return;
	} else if ((skip = alike(doc, "brief"))) {
		*kind   = 0;
		*docPtr = doc + skip;
		*fmtPtr = fmt;
		return;
	} else { // '@other' -> '**Other**:\n'
		*fmt++ = '*';
		*fmt++ = '*';
		*fmt++ = *kind = *doc++ - 32; // -32 = 'a' -> 'A'
		while (('a' <= *doc && *doc <= 'z') || ('A' <= *doc && *doc <= 'Z')) *fmt++ = *doc++;
		*docPtr = *doc == ' ' ? doc : doc + 1;
		*fmtPtr = append(fmt, "**: ");
		return;
	}
	if (*kind != *doc) fmt = append(fmt, section);
	*kind = *doc;
	doc += skip;
	while (*doc > ' ') doc++;
	*docPtr = doc;
	*fmtPtr = fmt;
	return;
}
