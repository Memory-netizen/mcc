#include "mcc.h"

#define COLOR_RESET "\033[0m"
#define COLOR_BOLD "\033[1m"
#define COLOR_RED "\033[0;1;31m"
#define COLOR_GREEN "\033[0;1;32m"
#define COLOR_MAGENTA "\033[0;1;35m"
#define COLOR_CYAN "\033[0;1;36m"

// 报告致命错误并结束进程
_Noreturn void fatal(char *fmt, ...) {
    fflush(stdout);
    fprintf(stderr, "\n");
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

/*
 * 打开文件并填充 MemoryBuffer
 * 返回 0 成功，-1 失败
 * 编译期间不释放, 进程结束后由 OS 释放
 */
int open_file(MemoryBuffer *mem_buf, const char *path) {
    // 先置空: 任何成功路径下调用方都能安全使用 mem_buf
    mem_buf->data = NULL;
    mem_buf->size = 0;

    // 按照惯例：文件名是 "-" 时, 从标准输入 stdin 读取
    int is_stdin = !strcmp(path, "-");
    FILE *fp = is_stdin ? stdin : fopen(path, "rb");
    if (!fp) return -1;

    int fd = fileno(fp);
    struct stat st;

    // 普通磁盘文件：优先 mmap 零拷贝
    if (!is_stdin && fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
        if (st.st_size > 0) {
            char *m = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
            if (m != MAP_FAILED) {
                mem_buf->data = m;
                mem_buf->size = (size_t)st.st_size;
                fclose(fp);
                return 0;
            }
        }
    }

    // 管道/stdin/无法映射：分段读取
    // 内存 arena 分配连续内存——这是一个 bump 分配器, 在本循环运行期间没有其他分配动作；
    // 所以每次扩容直接接着上一块地址, 不需要拷贝.
    // 唯一中断连续分配的场景：耗尽当前内存池块.

    size_t cap = 4096, len = 0;
    char *buff = emalloc(cap);

    while (1) {
        if (cap - len < 4096) {
            char *more = emalloc(4096);
            if (more == buff + cap) {
                cap += 4096;  // 块连续, 仍然是一整块内存
            } else {
                // 块边界不连续, 分配大缓冲区一次性拷贝
                size_t newcap = cap * 2 + 4096;
                char *big = emalloc(newcap);
                memcpy(big, buff, len);
                buff = big;
                cap = newcap;
            }
        }
        errno = 0;  // 让 fread 的设置可观测
        size_t n = fread(buff + len, 1, cap - len, fp);
        if (n == 0) {
            if (ferror(fp)) {
                int saved = errno ? errno : EIO;  // 保存 errno: fclose 可能覆写 errno
                if (!is_stdin) fclose(fp);
                errno = saved;
                return -1;
            }
            break;
        }
        len += n;
    }

    if (!is_stdin) fclose(fp);

    mem_buf->data = buff;
    mem_buf->size = len;
    return 0;
}
