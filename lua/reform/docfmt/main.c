#include <lauxlib.h>
#include <lua.h>
#include <stdlib.h>

/**
 * @brief Append `str` to `dst`
 *
 * @param dst destination
 * @param str string to append
 * @return ptr to current `dst` position
 */
static char *append(char *dst, const char *str) {
	while (*str) *dst++ = *str++;
	return dst;
}

/**
 * @brief test if `str` starts with `cmp`
 *
 * @param str tested string
 * @param cmp string to compare against
 * @return length of `cmp` if passed, else `0`
 */
static int alike(const char *str, const char *cmp) {
	int j = 0;
	while (cmp[j] == str[j] && cmp[j] && str[j]) j++;
	return cmp[j] == '\0' ? j : 0;
}

/**
 * @brief Parses current doc kind and adds section header when needed.
 *
 * @param doc source docs with original text
 * @param fmt buffer for formatted docs
 * @param docPos ptr to current `doc` index
 * @param fmtPos ptr to current `fmt` index
 * @param kind kind of previous docs line ('@[]..')
 * @return ptr to current `fmt` position
 */
static char *resolveKind(const char *doc, char *fmt, int *docPos, char *kind) {
	int i = *docPos;
	if (*kind != doc[i]) {
		if (!*kind && doc[i] != 'b') *fmt++ = '\n';
		const char *section;
		switch (*kind = doc[i++]) {
			case 'p':
				section = "**Parameters:**\n";
				break;
			case 'r':
				section = "**Returns:**\n";
				break;
			case 's':
				section = "**See:**\n";
				break;
			case 'b': // '@brief'
				*kind = 0;
				section = "";
				break;
			default: // '@other' -> '**Other**:\n'
				*fmt++ = '*';
				*fmt++ = '*';
				*fmt++ = doc[i - 1] - 32; // -32 = 'a' -> 'A'
				while (doc[i] != ' ') *fmt++ = doc[i++];
				section = "**:\n";
				break;
		}
		fmt = append(fmt, section);
	}
	while (doc[i] != ' ') i++;
	*docPos = i;
	return fmt;
}

/**
 * @brief C and CPP doxygen parser
 *
 * @param doc original markdown
 * @param fmt buffer for formatted documentation
 * @param len end of `doc`
 * @return ptr to current `fmt` position
 */
static char *cpp_fmt(const char *doc, char *fmt, int len) {
	int i = -1, end = len;
	if (alike(doc + len - 3, "```")) { // move the end (code declaration) to the beginning
		len -= 6;
		while (doc[len] != '`') len--;
		if (doc[len + 1] == '\n') {
			len += 2;
			while (doc[len] != '\n') *fmt++ = doc[len++];
			return fmt;
		}
		end = (len -= 2); // end before ```
		while (doc[len]) *fmt++ = doc[len++];
		fmt[-4] = ';'; // '\n```' -> ';```' to fix syntax highlighting
		*fmt++ = '\n';
		if (alike(doc, "###")) { // strip type defs - already in code block
			while (doc[++i] != '\n' || doc[++i] == '-' || doc[i] == '\n') {}
			if (alike(doc + i, "Param")) {
				while (doc[++i] != '\n') {}
				while (doc[++i] != '\n' || doc[++i] == '-') {}
			}
			*fmt++ = '\n';
			i--;
		}
	}
	char kind = 0;
	while (++i < end) {
		switch (doc[i]) {
			case '\\':
				*fmt++ = doc[i++];
				*fmt++ = doc[i];
				break;
			case '`':
				if (doc[i + 1] == '`' && doc[i + 2] == '`') {
					i += 2;
					fmt = append(fmt, "```");
					while (doc[i] != '`' || doc[++i] != '`' || doc[++i] != '`') *fmt++ = doc[i++];
					fmt = append(fmt - 1, "```");
				} else {
					*fmt++ = '`';
					while (doc[i] != '`') *fmt++ = doc[i++];
					*fmt++ = '`';
				}
				break;
			case -110:  //→
				fmt -= 2; //→ is a 3-byte char
				i += 3;
				while (doc[i] != '\n' || doc[i + 1] != '\n') i++;
				i++;
				break;
			case '@': {
				int j = 1;
				while (doc[i + j] >= 'a' && doc[i + j] <= 'z') j++;
				if (doc[i + j] != ' ') {
					*fmt++ = '@';
					break;
				}
				if (doc[++i] == 't') i++;
				fmt = resolveKind(doc, fmt, &i, &kind);
				if (kind == 'r' || kind == 'p') {
					fmt = append(fmt, " - ");
					if (kind == 'p') {
						*fmt++ = '`';
						while (doc[++i] != ' ') {
							if (doc[i] == ',') fmt = append(fmt, "`,`");
							else *fmt++ = doc[i];
						}
						fmt = append(fmt, "`: ");
					}
				}
			} break;
			case '\n':
				while (fmt[-1] == ' ') fmt--;
				if (fmt[-1] == '\n' && kind) {
					kind = 0;
					if (doc[i + 1] == '-') i += 4;
					break;
				}
			default:
				*fmt++ = doc[i];
		}
	}
	return fmt;
}

/**
 * @brief JavaDOC parser, needed for code blocks
 *
 * @param doc original markdown
 * @param fmt buffer for formatted documentation
 * @param len end of `doc`
 * @return ptr to current `fmt` position
 */
static char *java_fmt(const char *doc, char *fmt, int len) {
	int i = -1;
	while (++i < len) {
		switch (doc[i]) {
			default:
				*fmt++ = doc[i];
		}
	}
	return fmt;
}

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
				while (doc[++i] != '"') *fmt++ = doc[i];
				*fmt++ = doc[i];
				break;
			case '\'': // string '...'
				*fmt++ = doc[i];
				while (doc[++i] != '\'') *fmt++ = doc[i];
				*fmt++ = doc[i];
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
			case '>':
				*fmt++ = '>';
				if (fmt[-2] != '-') break;
				int j = 0;
				while (doc[i + j] != ':' && doc[i + j] != '\n') j++;
				if (doc[i + j] == ':') break;
				fmt = append(fmt, " ret");
			case ':':
				if (doc[i + 1] == '\n') {
					i += 2;
					while (doc[i] != '|') i++;
				}
				if (doc[i + 1] == ' ') {
					fmt = append(fmt, " = ");
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
							default: // other
								while (doc[i] != '|' && doc[i] != ',' && doc[i] != ')' && doc[i] != '\n') {
									if (doc[i] != '`') *fmt++ = doc[i];
									i++;
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
			default:
				*fmt++ = doc[i];
		}
	}
	return fmt;
}

/**
 * @brief lua + vim-style docs parser
 *
 * @param doc original markdown
 * @param fmt buffer for formatted documentation
 * @param len end of `doc`
 * @return ptr to current `fmt` position
 */
static char *lua_fmt(const char *doc, char *fmt, int len) {
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
					while (doc[++i] != '`') *fmt++ = doc[i];
					*fmt++ = '`';
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
				if (alike(doc + i + 1, "https~ ")) {
					i += 10;
					fmt = append(fmt, "[https://");
					while (doc[i] != ' ' && doc[i] != '\n' && doc[i]) *fmt++ = doc[i++];
					i--;
					*fmt++ = ']';
				} else *fmt++ = '~';
				break;
			case -94: { //• - vim docs
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
					while (*fmtTmp >= 'a' && *fmtTmp <= 'z' || *fmtTmp == '_') fmtTmp--;
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
					while (doc[++i] != ' ') *fmt++ = doc[i];
					if (doc[i + 1] == -30 && doc[i + 3] == -108) { // —
						i += 4;
						*fmt++ = ':';
					}
					*fmt++ = ' ';
				}
			} break;
			case '\n': {
				if (fmt[-1] == ' ') fmt--;
				if (alike(doc + i + 1, "---\n")) i += 4;
				int j = 1; // get beginning of text on the next line
				while (doc[i + j] == ' ') j++;
				if (doc[i + j] == '-' || doc[i + j] == '+' || doc[i + j] == -30) { // param description
					if (j > 4) {                                                     // 2.nd+ level indent
						if (indent[0] && indent[0] <= j) i += indent[1];
						else i += indent[1] = (indent[0] = j) - 4; // 1.st occurence of 2.nd level indent
					} else {                                     // back at 1.st level
						indent[0] = 0;
						indent[1] = doc[i + j] == -30 ? 3 : 0; // indent only for '   • {param}'
						if (j == 4) i += j - 2;
					}
				} else if (indent[1] < j) i += indent[1]; // wrapped text alignment
				if (fmt[-1] != '\n') *fmt++ = '\n';
			} break;
			default:
				*fmt++ = doc[i];
		}
	}
	return fmt;
}

static int l_fmt(lua_State *L) {
	if (lua_gettop(L) != 2) return 0;
	size_t len;
	const char *doc = luaL_checklstring(L, 1, &len);
	while (*doc == '\n' || *doc == ' ') {
		len--;
		doc++;
	}
	const char *ft = luaL_checkstring(L, 2);
	char *fmt, *fmtEnd;
	if (!ft[alike(ft, "cpp")] || (ft[0] == 'c' && ft[1] == 0))
		fmtEnd = cpp_fmt(doc, fmt = (char *) malloc(len + 50), len);
	else if (!ft[alike(ft, "java")])
		fmtEnd = java_fmt(doc, fmt = (char *) malloc(len > 128 ? len + 50 : 256), len);
	else if (!ft[alike(ft, "lua")]) fmtEnd = lua_fmt(doc, fmt = (char *) malloc(len + 50), len);
	else {
		lua_pushstring(L, doc);
		return 1;
	}
	while (fmt != fmtEnd && *--fmtEnd == '\n') {}
	*++fmtEnd = '\0';
	lua_pushstring(L, realloc(fmt, (fmtEnd - fmt + 1) * sizeof(char)));
	return 1;
}

int luaopen_reform_docfmt_main(lua_State *L) {
	// lua_newtable(L);
	lua_pushcfunction(L, l_fmt);
	// lua_setfield(L, -2, "fmt");
	return 1;
}
