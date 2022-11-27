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
