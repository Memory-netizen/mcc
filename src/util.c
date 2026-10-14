#include "mcc.h"

#define COLOR_RESET "\033[0m"
#define COLOR_BOLD "\033[1m"
#define COLOR_RED "\033[0;1;31m"
#define COLOR_GREEN "\033[0;1;32m"
#define COLOR_MAGENTA "\033[0;1;35m"
#define COLOR_CYAN "\033[0;1;36m"

// 报告致命错误并返回
_Noreturn void fatal(char *fmt, ...) {
    bool use_color = isatty(fileno(stderr));
    if (use_color)
        fprintf(stderr, COLOR_BOLD "mcc:" COLOR_RESET COLOR_RED " fatal error: " COLOR_RESET);
    else
        fprintf(stderr, "mcc: fatal error: ");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

#define ALIGNMENT _Alignof(max_align_t)
#define POOL_CHUNK_MIN (64 * 1024)
#define POOL_CHUNK_MAX (8 * 1024 * 1024)

static size_t next_chunk = POOL_CHUNK_MIN;

char *arena_pool;
size_t arena_free;

// 分配大块池
void *emalloc_big(size_t n) {
    if (n == 0) return NULL;
    n = ALIGN_UP(n, ALIGNMENT);
    void *p = calloc(1, n);
    if (!p) fatal("emalloc, out of memory");
    return p;
}

// 为大量小对象分配空间
void *emalloc(size_t n) {
    if (n == 0) return NULL;
    n = ALIGN_UP(n, ALIGNMENT);

    if (arena_free < n) {
        size_t chunk = next_chunk;
        while (chunk < n) chunk *= 2;
        if (next_chunk < POOL_CHUNK_MAX) next_chunk *= 2;
        char *p = calloc(1, chunk);
        if (!p) fatal("emalloc, out of memory");
        arena_pool = p;
        arena_free = chunk;
    }

    void *p = arena_pool;
    arena_pool += n;
    arena_free -= n;
    return p;
}

char *format(char *s, ...) {
    va_list ap;
    int n;
    char *p;

    va_start(ap, s);
    n = vsnprintf(NULL, 0, s, ap);
    if (n < 0) fatal("format failed");
    va_end(ap);
    p = emalloc(n + 1);
    va_start(ap, s);
    vsnprintf(p, n + 1, s, ap);
    va_end(ap);
    return p;
}
