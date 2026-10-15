#include "mcc.h"

void tokenize(MccState *s) {
    MemoryBuffer mb = {};
    if (open_file(&mb, s->base_file)) fatal("%s: %s", s->base_file, strerror(errno));

    // 测试读取
    fwrite(mb.data, 1, mb.size, stdout);
}
