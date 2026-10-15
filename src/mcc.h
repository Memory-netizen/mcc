#ifndef MCC_H_
#define MCC_H_

#include <ctype.h>
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

#define CH_EOF (-1)
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

//
// main.c
//
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

//
// lexer.c
//
typedef struct {
    const char *data;
    size_t size;
} MemoryBuffer;

typedef struct {
    const char *const true_name;  // 原始文件名
    const char *diag_name;        // 诊断文件名
    const MemoryBuffer mem_buf;   // 只读数据区
    size_t ptr;                   // 当前读取位置
    uint32_t line_num;            // 当前行号
    int uget[4];                  // 回退字符缓冲区
    int idx;                      // 回退区指针
} FileReader;

#include "mcctok.h"
void tokenize(MccState *s);

// 除 '\n' 外的空白符, '\r' 已被归一化为 '\n'
static inline int is_space(int ch) { return ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f'; }

//
// util.c
//
typedef struct {
    const char *diag_name;  // 诊断文件名
    const char *start;      // 诊断所在行行首
    const char *at;         // 诊断所在位置
    const char *end;        // 诊断所在行行末
    int line;               // 诊断行号
    int col;                // 列号
} Loc;

void get_location(Loc *loc, FileReader *f, uint32_t line_no, size_t ptr);
_Noreturn void fatal(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
_Noreturn void error(Loc *loc, const char *msg, ...) __attribute__((format(printf, 2, 3)));
void *emalloc(size_t n);
void *emalloc_big(size_t n);
void *vnew(size_t len, size_t esz);
void *vgrow(void *data, size_t len);
char *format(char *s, ...) __attribute__((format(printf, 1, 2)));
int open_file(MemoryBuffer *mem_buf, const char *path);

#endif  // MCC_H_
