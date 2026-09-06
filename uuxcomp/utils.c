#include "utils.h"

#include <string.h>


char *uuxcomp_determine_linebreak(const char *s) {

    if (strstr(s,CRLF)!=NULL)
        return(CRLF);
    else if(strstr(s,LF)!=NULL)
        return(LF);
    else if(strstr(s,CR)!=NULL)
        return(CR);
    else
        return(NULL);
}

char *uuxcomp_mem_find(const char *haystack, size_t haystack_len,
                       const char *needle, size_t needle_len) {

    if (needle_len == 0)
        return (char *)haystack;
    if (haystack_len < needle_len)
        return NULL;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (haystack[i] == needle[0] &&
            memcmp(haystack + i, needle, needle_len) == 0)
            return (char *)(haystack + i);
    }
    return NULL;
}

char *uuxcomp_find_blank_line(const char *buf, size_t len, const char *line_break) {

    size_t lb_len = strlen(line_break);

    if (lb_len == 0 || len < 2 * lb_len)
        return NULL;

    for (size_t i = 0; i + (2 * lb_len) <= len; i++) {
        if (memcmp(buf + i, line_break, lb_len) == 0 &&
            memcmp(buf + i + lb_len, line_break, lb_len) == 0)
            return (char *)(buf + i);
    }
    return NULL;
}
