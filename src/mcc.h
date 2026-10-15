#ifndef MCC_H_
#define MCC_H_

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ALIGN_UP(value, align) (((value) + (align) - 1) & ~((align) - 1))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#ifndef NDEBUG
#define ASSERT(a, msg, ...)                                                   \
    do {                                                                      \
        if (!(a)) {                                                           \
            fprintf(stderr, "%s:%d: assertion failed: ", __FILE__, __LINE__); \
            fprintf(stderr, msg __VA_OPT__(, ) __VA_ARGS__);                  \
            fprintf(stderr, "\n");                                            \
            abort();                                                          \
        }                                                                     \
    } while (0)
#else
#define ASSERT(a, msg, ...)
#endif

typedef struct {
    char *self;

    char *base_file;
    char *output_file;

    char *opt_o;
    char **input_paths;
    int num_input;

    char **cmd;
    int num_cmd;

    bool opt_cc1;
    bool opt_hash3;
    bool opt_dump_raw_tokens;

    bool opt_E;
    bool opt_S;
    bool opt_ll;
    bool opt_c;
} MccState;

// lexer.c

typedef struct {
    const char *data;
    size_t size;
} MemoryBuffer;

void tokenize(MccState *s);

// util.c
_Noreturn void fatal(char *fmt, ...) __attribute__((format(printf, 1, 2)));
void *emalloc(size_t n);
void *emalloc_big(size_t n);
char *format(char *s, ...) __attribute__((format(printf, 1, 2)));
int open_file(MemoryBuffer *mem_buf, const char *path);

#endif  // MCC_H_
