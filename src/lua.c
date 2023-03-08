#include "utils.h"

/**
 * @brief Format lua code from sumneko_lua markdown docs
 *
 * @param doc original markdown
 * @param fmt buffer for formatted documentation
 * @param docStart ref to pos of `doc` parsing
 * @param stop '`' or 'p' for <pre> code blocks
 * @return ptr to current `fmt` position
 */
static char *lua_code_fmt(const char *doc, char *fmt, int *docStart, char stop) {
	int i = *docStart;
	*fmt++ = doc[i];
	unsigned char params = 0;
	while (doc[++i]) {
		switch (doc[i]) {
			case '"': // string "..."
				*fmt++ = doc[i];
				while (doc[++i] != '"' && doc[i] != '\n') *fmt++ = doc[i];
				if (doc[i] == '\n') i--;
				else *fmt++ = doc[i];
				break;
			case '\'': // string '...'
				*fmt++ = doc[i];
				while (doc[++i] != '\'' && doc[i] != '\n') *fmt++ = doc[i];
				if (doc[i] == '\n') i--;
				else *fmt++ = doc[i];
				break;
			case '[': // string [[...]]
				*fmt++ = doc[i];
				if (doc[i + 1] != '[') break;
				while (doc[++i] != ']' || doc[i + 1] != ']') *fmt++ = doc[i];
				*fmt++ = doc[i++];
				*fmt++ = doc[i];
				break;
			case '(':
				// check for "function "
				if (fmt[-1] != ' ' || (fmt[-2] == 'n' && fmt[-9] == 'f')) {
					char *fmtTmp = fmt - 1;
					while (*fmtTmp != '\n' && *fmtTmp != ' ') fmtTmp--;
					if (*fmtTmp == ' ' && alike(fmtTmp - 9, "\nfunction")) {
						if (fmt[-1] == ' ') *fmt++ = '_';
						params = 1;
					}
				}
				*fmt++ = '(';
				break;
			case ')':
				*fmt++ = ')';
				if (params) {
					params = 0;
					fmt = append(fmt, " end");
				}
				break;
			case '>': {
				*fmt++ = '>';
				if (fmt[-2] != '-' || doc[i + 1] != ' ') break;
				int j = 0;
				while (doc[i + j] != ':' && doc[i + j] != '\n') j++;
				if (doc[i + j] == ':') break;
				fmt = append(fmt, " ret =");
			}
			case ':':
				if (doc[i + 1] == '\n') {
					i += 2;
					while (doc[i] != '|') i++;
				}
				if (doc[i + 1] == ' ') {
					if (doc[i] == ':') {
						*fmt++ = ' ';
						*fmt++ = '=';
					}
					*fmt++ = ' ';
					i += 2;
					const char *types[] = {"any",     "string",   "table",   "number",
					                       "unknown", "function", "boolean", "fun("};
					do {
						if (doc[i] == ' ') i++;
						int type = 0, len = 0;
						for (; type < 8; type++, len = 0)
							if ((len = alike(doc + i, types[type]))) break;
						if (doc[i + len] == '[') *fmt++ = '{'; // array start
						switch (type) {
							case 1: // string
								if (doc[i + len + 1] != '=') {
									*fmt++ = '"';
									*fmt++ = '"';
								} else i += 3;
								break;
							case 2: // table
								if (doc[i + len + 1] != '{') {
									*fmt++ = '{';
									*fmt++ = '}';
								} else i++;
								break;
							case 3: // number
								*fmt++ = '.';
								*fmt++ = '0';
								break;
							case 0: // any
							case 4: // unknown
								*fmt++ = '?';
								break;
							case 5: // function
								fmt = append(fmt, "fun()");
								break;
							case 6: // boolean
								fmt = append(fmt, "true|false");
								break;
							case 7:
								fmt = append(fmt, "fun(");
								break;
							default: { // other
								char *fmtTmp = fmt;
								while (doc[i] != '|' && doc[i] != ',' && doc[i] != ')' && doc[i] != '\n' &&
								       doc[i] != ' ') {
									if (doc[i] != '`') *fmt++ = doc[i];
									i++;
								}
								if (doc[i + 1] == '=') {
									i += 3;
									fmt = fmtTmp;
								}
							}
						}
						if (len) {
							i += len;
							if (doc[i + 1] == ']') { // array end
								*fmt++ = '}';
								i += 2;
							}
						}
						if (doc[i] == '\n' && doc[i + 5] == '|') i += 5;
						if (doc[i] == '|') *fmt++ = '|';
					} while (doc[i++] == '|');
					i -= 2;
				} else *fmt++ = doc[i];
				break;
			case '\\':
				*fmt++ = '\\';
				*fmt++ = doc[++i];
				break;
			case '`':
				if (stop == '`' && doc[i + 1] == '`' && doc[i + 2] == '`') {
					*docStart = i + 2;
					while (*--fmt == '\n') {}
					return fmt + 1;
				} else *fmt++ = doc[i];
				break;
			case '<':
				if (stop == 'p' && alike(doc + i + 1, "/pre>")) {
					*docStart = i + 5;
					while (*--fmt == '\n' || *fmt == ' ') {}
					return fmt + 1;
				}
			case '-':
				if (doc[i + 1] == '-') {
					while (doc[i] != '\n') *fmt++ = doc[i++];
					*fmt++ = '\n';
				}
			default:
				*fmt++ = doc[i];
		}
	}
	return fmt;
}

char *lua_fmt(const char *doc, char *fmt, int len) {
	int i = 0;
	if (alike(doc, "```lua\n")) {
		fmt = append(fmt, "```lua\n");
		i += 7;
		if (doc[i] == '(') {
			switch (doc[i + 1]) {
				case 'g': // '(global)'
					fmt = append(fmt, "_G.");
					break;
					// other: '(field)'
			}
			i += 5;
			while (doc[i++] != ' ') {}
		}
		fmt = append(lua_code_fmt(doc, fmt, &i, '`'), "```\n\n");
		if (!doc[i]) return fmt - 2;
	} else {
		while (doc[i] != '\n' && doc[i]) *fmt++ = doc[i++];
		*fmt++ = doc[i];
	}
	int indent[] = {0, 0}; // [0] = base, [1] = last noted
	char kind = 0;
	while (++i < len) {
		switch (doc[i]) {
			case '\\':
				*fmt++ = doc[i++];
				*fmt++ = doc[i];
				break;
			case '`': // code with '```'
				if (doc[i + 1] == '`' && doc[i + 2] == '`') {
					while (*--fmt != '\n') {}
					fmt++;
					while (doc[i] != '\n') i++;
					fmt = append(lua_code_fmt(doc, append(fmt, "```lua\n"), &i, '`'), "```");
				} else {
					*fmt++ = '`';
					while (doc[++i] != '`' && doc[i] != '\n' && doc[i]) *fmt++ = doc[i];
					*fmt++ = '`';
					if (doc[i] != '`') i--;
				}
				break;
			case '<': // code with '<pre>'
				if (alike(doc + i + 1, "pre>")) {
					while (*--fmt != '\n') {}
					i += 6;
					fmt = append(lua_code_fmt(doc, append(fmt + 1, "```lua\n"), &i, 'p'), "```");
				} else *fmt++ = doc[i];
				break;
			case '|': // vim help-page-style links '|text|'
				if (doc[i + 1] != ' ') {
					char *fmtTmp = fmt;
					*fmt++ = '[';
					while (doc[++i] != '|' && doc[i] != ' ' && doc[i] != '\n') *fmt++ = doc[i];
					if (doc[i] != '|') {
						*fmtTmp = '|';
						i--;
					} else *fmt++ = ']';
				} else *fmt++ = '|';
				break;
			case '~': // 'See:' links deformed into '~https~ //url'
				if (alike(doc + i + 1, "http") && (doc[i + 5] == '~' || doc[i + 6] == '~')) {
					if (alike(fmt - 3, " * ")) fmt = append(fmt - 3, "- ");
					i++;
					*fmt++ = '[';
					while (doc[i] != '~') *fmt++ = doc[i++];
					i += 2;
					*fmt++ = ':';
					while (doc[i] != ' ' && doc[i] != '\n' && doc[i]) *fmt++ = doc[i++];
					i--;
					*fmt++ = ']';
				} else *fmt++ = '~';
				break;
			case -94: { // • - vim docs
				i += 2;
				fmt = append(fmt - 2, "- ");
				if (doc[i] == '{') {
					*fmt++ = '`';
					while (doc[++i] != '}') *fmt++ = doc[i];
					*fmt++ = '`';
					*fmt++ = ':';
				} else {
					int j = i;
					while (doc[j] != ':' && doc[j] != ' ') j++;
					if (doc[j] == ':' || doc[j + 1] == ':') *fmt++ = '`';
					while (i < j) *fmt++ = doc[i++];
					if (doc[j] == ':') *fmt++ = '`';
					else if (doc[j + 1] == ':') {
						*fmt++ = '`';
						i++;
					}
					*fmt++ = doc[i];
				}
			} break;
			case '{': { // vim references to params as '{param}'
				const char *docTmp = doc + i;
				while (*docTmp != ' ' && *docTmp != '}') *fmt++ = *docTmp++;
				if (*docTmp == ' ') *fmt++ = '{';
				else {
					fmt[doc + i - docTmp] = '`';
					*fmt++ = '`';
				}
				i = docTmp - doc;
			} break;
			case ':':
				if (alike(doc + i + 1, " ~\n")) { // vim sections ' Section: ~'
					int j = 0;
					while (fmt[-j] != '\n') j++;
					j -= 2;
					fmt[-j - 1] = '*';
					if (fmt[-j - 3] == '\n') fmt[-j - 2] = '*';
					else {
						for (int m = 0; m <= j; m++) fmt[-m] = fmt[-m - 1];
						fmt++;
					}
					char kind = fmt[-j];
					if (kind == 'R') *fmt++ = 's'; // 'Return*s*:'
					fmt = append(fmt, ":**");
					if (kind == 'P') i += 2;
					else {
						i += 3;
						if (kind == 'R') fmt = append(fmt, "\n - ");
						else *fmt++ = ' ';
						while (doc[++i] == ' ') {}
						i--;
					}
				} else {
					char *fmtTmp = fmt - 1;
					while ((*fmtTmp >= 'a' && *fmtTmp <= 'z') || *fmtTmp == '_') fmtTmp--;
					if (*fmtTmp >= 'A' && *fmtTmp <= 'Z' && // '\n Example:' -> '\n **Example:**'
					    (fmtTmp[-1] == '\n' || alike(fmtTmp - 2, "\n "))) {
						for (int m = fmt - --fmtTmp; m; m--) fmtTmp[m + 2] = fmtTmp[m];
						*++fmtTmp = '*';
						*++fmtTmp = '*';
						fmt = append(fmt + 2, ":**");
						break;
					} else if (*fmtTmp == ' ' || *fmtTmp == '\n') { // any other mi-line 'text:'
						for (int m = fmt - fmtTmp; m; m--) fmtTmp[m + 1] = fmtTmp[m];
						if (fmtTmp[-1] == '-') { // '- name: desc' -> '- `name`: desc'
							*++fmtTmp = '`';
							*++fmt = '`';
						} else { // 'other:' -> '*other*:'
							*++fmtTmp = '*';
							*++fmt = '*';
						}
						fmt++;
					}
					*fmt++ = ':';
				}
				break;
			case '@': { // format: '@*param* `name` — desc'
				int j = 2;
				indent[0] = 0;
				indent[1] = 3;
				while (doc[i + j] >= 'a' && doc[i + j] <= 'z') j++; // must be a word
				if (doc[i + j] != '*' || doc[i + 1] != '*' || doc[i + j + 1] != ' ') {
					*fmt++ = '@';
					break;
				}
				i += 2;
				fmt = resolveKind(doc, fmt, &i, &kind);
				if (kind == 'r' || kind == 'p') fmt = append(fmt, " - ");
				if (kind == 'r') {       // '@return'
					if (doc[++i] == '`') { // fixing word wrapped as variable name
						while (doc[++i] != '`') *fmt++ = doc[i];
						i += 4;
					} else i += 2;
				} else {
					j = i;
					while (doc[++i] != ' ') *fmt++ = doc[i];
					if (doc[i + 1] == -30 && doc[i + 3] == -108) { // —
						i += 4;
						*fmt++ = ':';
						indent[0] = -1;
						indent[1] = i - j - 2;
					}
					*fmt++ = ' ';
				}
			} break;
			case '\n': {
				if (fmt[-1] == ' ') fmt--;
				if (fmt[-1] != '\n') *fmt++ = '\n';
				if (alike(doc + i + 1, "---\n")) i += 4;
				int j = 1; // get beginning of text on the next line
				while (doc[i + j] == ' ') j++;
				if (doc[i + j] == '-' || doc[i + j] == '+' || doc[i + j] == -30) { // param description
					if (j > 4) {                                                     // 2.nd+ level indent
						if (indent[0] > 0 && indent[0] <= j) i += indent[1];
						else i += indent[1] = (indent[0] = j) - 4; // 1.st occurence of 2.nd level indent
					} else {                                     // back at 1.st level
						indent[0] = 0;
						indent[1] = doc[i + j] == -30 ? 3 : 0; // indent only for '   • {param}'
						if (j == 4) i += j - 2;
					}
				} else if (indent[0] == -1 && j > 3) {
					for (int k = indent[1]; k > 0; k--) *fmt++ = ' ';
					i += j - 1;
				} else if (indent[1] < j) i += indent[1]; // wrapped text alignment
			} break;
			default:
				*fmt++ = doc[i];
		}
	}
	return fmt;
}
