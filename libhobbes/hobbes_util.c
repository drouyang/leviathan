#include "hobbes_util.h"
 


int
smart_atoi(int dflt, char * str) 
{
    char * end  = NULL;
    int    tmp  = 0;
    int    base = 10;
    
    if ((str == NULL) || (*str == '\0')) {
        /*  String was either NULL or empty */
        return dflt;
    }

   if (strlen(str) > 2) {
        if ((*(str + 1) == 'x') ||
            (*(str + 1) == 'X')) {
            base = 16;
        }
    }

    tmp = strtol(str, &end, base);

    if (*end) {
        /* String contained non-numerics */
        return dflt;
    }
    
    return tmp;
}


uint64_t
smart_atou64(uint64_t dflt, char * str) 
{
    char     * end  = NULL;
    uint64_t   tmp  = 0;
    int        base = 10;
    
    if ((str == NULL) || (*str == '\0')) {
        /*  String was either NULL or empty */
        return dflt;
    }

   if (strlen(str) > 2) {
        if ((*(str + 1) == 'x') ||
            (*(str + 1) == 'X')) {
            base = 16;
        }
    }

    tmp = strtoull(str, &end, base);

    if (*end) {
        /* String contained non-numerics */
        return dflt;
    }
    
    return tmp;
}

int64_t
smart_atoi64(int64_t dflt, char * str) 
{
    char    * end  = NULL;
    int64_t   tmp  = 0;
    int       base = 10;
    
    if ((str == NULL) || (*str == '\0')) {
        /*  String was either NULL or empty */
        return dflt;
    }

   if (strlen(str) > 2) {
        if ((*(str + 1) == 'x') ||
            (*(str + 1) == 'X')) {
            base = 16;
        }
    }

    tmp = strtoll(str, &end, base);

    if (*end) {
        /* String contained non-numerics */
        return dflt;
    }
    
    return tmp;
}
