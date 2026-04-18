#include <string.h>

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (n && (*d++ = *src++)) {
        n--;
    }
    while (n--) {
        *d++ = '\0';
    }
    return dest;
}

// strtok implementation
static char* strtok_saveptr = NULL;

char* strtok(char* str, const char* delim) {
    char* start;
    char* end;
    
    if (str != NULL) {
        strtok_saveptr = str;
    }
    
    if (strtok_saveptr == NULL) {
        return NULL;
    }
    
    // Skip leading delimiters
    start = strtok_saveptr;
    while (*start && strchr(delim, *start)) {
        start++;
    }
    
    if (*start == '\0') {
        strtok_saveptr = NULL;
        return NULL;
    }
    
    // Find end of token
    end = start;
    while (*end && !strchr(delim, *end)) {
        end++;
    }
    
    if (*end != '\0') {
        *end = '\0';
        strtok_saveptr = end + 1;
    } else {
        strtok_saveptr = NULL;
    }
    
    return start;
}

// strchr is already in string.h