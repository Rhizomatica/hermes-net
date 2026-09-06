#ifndef UTILS_H_
#define UTILS_H_

#include <stddef.h>


#define CRLF "\r\n"
#define DCRLF "\r\n\r\n"
#define LF "\n"
#define CR "\r"

char *uuxcomp_determine_linebreak(const char *s);

/* memmem() work-alike, kept local so we don't depend on _GNU_SOURCE feature
 * macros being set the same way on every build host.  Returns a pointer to the
 * first occurrence of needle within the first haystack_len bytes of haystack,
 * or NULL. */
char *uuxcomp_mem_find(const char *haystack, size_t haystack_len,
                       const char *needle, size_t needle_len);

/* Locate the first blank line (line_break immediately followed by the same
 * line_break) within the first len bytes of buf.  Returns a pointer to the
 * start of the blank line (the first line_break), or NULL. */
char *uuxcomp_find_blank_line(const char *buf, size_t len, const char *line_break);


#endif // UTILS_H_
