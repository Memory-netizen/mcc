#include "mcc.h"

#define CH_EOF (-1)

typedef struct {
    char *true_name;       // 原始文件名
    MemoryBuffer mem_buf;  // 只读数据区
    size_t ptr;            // 当前读取位置
    uint32_t line_num;     // 当前行号
    int uget[4];           // 回退字符缓冲区
    int idx;               // 回退区指针
} FileReader;

// 获取下一个字符并推进读取游标, 自动处理 "\\n"
int next_ch(FileReader *f) {
    // 回退区非空直接取出
    if (f->idx > 0) return f->uget[--f->idx];

    const char *data = f->mem_buf.data;
    size_t size = f->mem_buf.size;
    size_t ptr = f->ptr;
    if (ptr >= size) return CH_EOF;

    int ch;
    while ((ch = (unsigned char)data[ptr++]) == '\\') {
        if (ptr >= size) fatal("stray '\\' in program");  // 文件末尾孤立反斜杠
        ch = (unsigned char)data[ptr];                    // 查看下一个, 不推进游标
        if (ch != '\n' && ch != '\r') break;
        if (ch == '\n') {  // 处理 "\\\n" 行尾
            ++ptr;
        } else {
            if (data[ptr + 1] == '\n') {  // 处理 "\\\r\n" 行尾
                ptr += 2;
            } else {  // 处理 "\\\r" 行尾
                ++ptr;
            }
        }
        f->line_num++;
        if (ptr >= size) {
            ch = CH_EOF;
            break;
        }
    }

    // 归一化 "\\\r" | "\\\r\n" => "\n"
    if (ch == '\r') {
        if (ptr < size && data[ptr] == '\n') ++ptr;
        ch = '\n';
    }
    if (ch == '\n') f->line_num++;
    f->ptr = ptr;
    return ch;
}

// 放回一个字符
void uget_ch(FileReader *f, int ch) {
    ASSERT(f->idx + 1 <= 4, "uget buffer overflow");  // 不可能回退 4 个以上字符
    f->uget[f->idx++] = ch;                           // 放回缓冲区
}

int next_rawtok(FileReader *f) {
    (void)f;
    return 0;
}

void tokenize(MccState *s) {
    MemoryBuffer mb = {};
    if (open_file(&mb, s->base_file)) fatal("%s: %s", s->base_file, strerror(errno));

    FileReader f = {.true_name = s->base_file, .mem_buf = mb, .line_num = 1};
    while (1) {
        int ch = next_ch(&f);
        if (ch == CH_EOF) {
            printf("%s:%d: <EOF>\n", f.true_name, f.line_num);
            break;
        }
        printf("%s:%d: %c\n", f.true_name, f.line_num, ch);
    }
}
