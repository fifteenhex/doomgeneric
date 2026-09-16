//
// The handful of libc calls doom makes that nolibc does not carry.
// Force-included after nolibc.h, so everything nolibc provides is
// already in scope and anything defined here fills a real gap.
//

#ifndef COMPAT_NOLIBC_COMPAT_H
#define COMPAT_NOLIBC_COMPAT_H

#include "strings.h"

// nolibc's limits stop at int; doom clamps to shorts here and there
#ifndef SHRT_MAX
#define SHRT_MAX  0x7fff
#define SHRT_MIN  (-SHRT_MAX - 1)
#define USHRT_MAX 0xffff
#define CHAR_BIT  8
#define SCHAR_MAX 0x7f
#define SCHAR_MIN (-SCHAR_MAX - 1)
#define UCHAR_MAX 0xff
#endif

// nolibc's FILE is the fd and its fread/fwrite go straight to the
// kernel, so the stream position is the fd position.
static inline long ftell(FILE *stream)
{
    return (long) lseek(fileno(stream), 0, SEEK_CUR);
}

static inline int remove(const char *path)
{
    return unlink(path);
}

static inline int rename(const char *oldpath, const char *newpath)
{
    return __sysret(__nolibc_syscall5(__NR_renameat2, AT_FDCWD, oldpath,
                                      AT_FDCWD, newpath, 0));
}

// There is no shell in a static nolibc world, and saying so is how a
// caller's fallback path gets taken.
static inline int system(const char *command)
{
    (void) command;

    return -1;
}

// Enough of atof for a config file: an optional sign, digits, a dot,
// more digits. Nobody writes 1e6 in default.cfg.
static inline double atof(const char *s)
{
    double value = 0, scale = 0.1;
    int negative = 0;

    while (isspace(*s))
    {
        s++;
    }

    if (*s == '-' || *s == '+')
    {
        negative = *s++ == '-';
    }

    while (isdigit(*s))
    {
        value = value * 10 + (*s++ - '0');
    }

    if (*s == '.')
    {
        s++;

        while (isdigit(*s))
        {
            value += (*s++ - '0') * scale;
            scale /= 10;
        }
    }

    return negative ? -value : value;
}

// fscanf and feof, which the config loader drives in a pair: fscanf a
// line at a time until feof says stop. nolibc's FILE has nowhere to
// keep an eof flag, so one is kept here per fd; both callers live in
// the same file, so each translation unit seeing its own copy is fine.
static unsigned char compat_eof[128] __attribute__((unused));

static inline int feof(FILE *stream)
{
    int fd = fileno(stream);

    if (fd < 0 || fd >= (int) sizeof(compat_eof) * 8)
    {
        return 0;
    }

    return (compat_eof[fd / 8] >> (fd % 8)) & 1;
}

// A line-based fscanf covering what doom asks of it: literal
// whitespace, %<width>s and %<width>[^\n]. A line longer than the
// buffer is read as two lines, which for a config file means the tail
// fails to match and is skipped.
static inline int fscanf(FILE *stream, const char *format, ...)
{
    char line[256];
    const char *s = line;
    va_list args;
    int matches = 0;
    int fd;

    if (!fgets(line, sizeof(line), stream))
    {
        fd = fileno(stream);

        if (fd >= 0 && fd < (int) sizeof(compat_eof) * 8)
        {
            compat_eof[fd / 8] |= 1 << (fd % 8);
        }

        return EOF;
    }

    va_start(args, format);

    for (; *format; format++)
    {
        char *out;
        int width = 0;
        int n = 0;

        if (isspace(*format))
        {
            while (isspace(*s))
            {
                s++;
            }

            continue;
        }

        if (*format != '%')
        {
            if (*s++ != *format)
            {
                break;
            }

            continue;
        }

        format++;

        while (isdigit(*format))
        {
            width = width * 10 + (*format++ - '0');
        }

        if (!width)
        {
            width = (int) sizeof(line);
        }

        if (*format == 's')
        {
            while (isspace(*s))
            {
                s++;
            }

            out = va_arg(args, char *);

            while (*s && !isspace(*s) && n < width)
            {
                out[n++] = *s++;
            }
        }
        else if (*format == '[')
        {
            // only the %[^\n] a config file uses
            while (*format && *format != ']')
            {
                format++;
            }

            out = va_arg(args, char *);

            while (*s && *s != '\n' && n < width)
            {
                out[n++] = *s++;
            }
        }
        else
        {
            break;
        }

        if (!n)
        {
            break;
        }

        out[n] = '\0';
        matches++;
    }

    va_end(args);

    return matches;
}

#endif
