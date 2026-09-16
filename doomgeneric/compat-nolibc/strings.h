//
// strings.h for a nolibc build, which has no strings.h of its own.
//

#ifndef COMPAT_STRINGS_H
#define COMPAT_STRINGS_H

static inline int strcasecmp(const char *s1, const char *s2)
{
    int c1, c2;

    do
    {
        c1 = tolower(*(const unsigned char *) s1++);
        c2 = tolower(*(const unsigned char *) s2++);
    } while (c1 && c1 == c2);

    return c1 - c2;
}

static inline int strncasecmp(const char *s1, const char *s2, size_t n)
{
    int c1 = 0, c2 = 0;

    while (n--)
    {
        c1 = tolower(*(const unsigned char *) s1++);
        c2 = tolower(*(const unsigned char *) s2++);

        if (!c1 || c1 != c2)
        {
            break;
        }
    }

    return c1 - c2;
}

#endif
