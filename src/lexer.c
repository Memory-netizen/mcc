#include <stdio.h>

#include "mcc.h"
#include "mcctok.h"

int tok;
uint8_t text[4096];
int idx;
Loc loc;

// 获取下一个字符并推进读取游标, 自动处理 "\\n"
int next_ch(FileReader *f) {
    // 回退区非空直接取出
    if (f->idx > 0) return f->uget[--f->idx];

    const char *data = f->mem_buf.data;
    const size_t size = f->mem_buf.size;
    size_t ptr = f->ptr;
    if (ptr >= size) return CH_EOF;

    int ch;
    while ((ch = (unsigned char)data[ptr++]) == '\\') {
        if (ptr >= size) {  // 文件末尾孤立反斜杠
            get_location(&loc, f, f->line_num, ptr - 1);
            error(&loc, "stray '\\' in program");
        }
        ch = (unsigned char)data[ptr];  // 查看下一个, 不推进游标
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
        if (ptr >= size) {
            ch = CH_EOF;
            break;
        }
        f->line_num++;
    }

    // 归一化 "\r" | "\r\n" => "\n"
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

void next_raw(FileReader *f) {
    size_t ptr = f->ptr;
    uint32_t line_num = f->line_num;
    idx = 0;

    int ch = next_ch(f);
    switch (ch) {
        case CH_EOF:
            tok = ch;
            return;
        case '\n':
            do {
                ch = next_ch(f);
            } while (isspace(ch));
            uget_ch(f, ch);
            tok = TK_NL;
            return;
        case ' ':
        case '\t':
        case '\v':
        case '\f':
        handle_space:
            do {
                ch = next_ch(f);
            } while (is_space(ch));
            uget_ch(f, ch);
            tok = TK_WS;
            return;
        case '/':
            ch = next_ch(f);
            if (ch == '*') {
                ch = next_ch(f);
                while (1) {
                    while (ch != '*' && ch != CH_EOF) ch = next_ch(f);
                    do {
                        ch = next_ch(f);
                    } while (ch == '*');
                    if (ch == CH_EOF) {
                        get_location(&loc, f, line_num, ptr - 1);
                        error(&loc, "unterminated /* comment");
                    }
                    if (ch == '/') goto handle_space;
                }
            } else if (ch == '/') {
                do {
                    ch = next_ch(f);
                } while (ch != '\n');
                uget_ch(f, ch);
                tok = TK_WS;
                return;
            } else if (ch == '=') {
                tok = TK_AS_DIV;
            } else {
                uget_ch(f, ch);
                tok = '/';
            }
            return;
        case '"':
        case '\'':
        handle_str: {
            int seq = ch;
            text[idx++] = seq;
            while (1) {
                ch = next_ch(f);
                if (ch == '\n' || ch == CH_EOF) {
                    get_location(&loc, f, line_num, ptr - 1);
                    error(&loc, "missing terminating %c character", seq);
                }
                text[idx++] = ch;
                if (ch == '\\') {
                    ch = next_ch(f);
                    text[idx++] = ch;
                    continue;
                }
                if (ch == seq) break;
            }
            text[idx] = '\0';
            printf("%s\n", text);
            tok = seq == '"' ? TK_PP_STR : TK_PP_CH;
            return;
        }
        case 'u':
            text[idx++] = ch;
            ch = next_ch(f);
            if (ch != '8') goto check_seq;
            text[idx++] = ch;
            ch = next_ch(f);
            goto check_seq;
        case 'U':
        case 'L':
            text[idx++] = ch;
            ch = next_ch(f);
        check_seq:
            if (ch == '"' || ch == '\'') goto handle_str;
            goto handle_ident;
        case '_':
        case 'a' ... 't':
        case 'v' ... 'z':
        case 'A' ... 'K':
        case 'M' ... 'T':
        case 'V' ... 'Z':
        handle_ident:
            while (isalnum(ch) || ch == '_') {
                text[idx++] = ch;
                ch = next_ch(f);
            }
            uget_ch(f, ch);
            text[idx] = '\0';
            tok = TK_RAW_IDENT;
            return;
        case '.':
            ch = next_ch(f);
            if (ch == '.') {
                ch = next_ch(f);
                if (ch == '.') {
                    tok = TK_ELLIPSIS;
                } else {
                    uget_ch(f, ch);
                    uget_ch(f, '.');
                    tok = '.';
                }
                return;
            } else if (isdigit(ch)) {
                text[idx++] = '.';
                goto handle_digit;
            } else {
                uget_ch(f, ch);
                tok = '.';
                return;
            }
        case '0' ... '9':
        handle_digit:
            text[idx++] = ch;
            ch = next_ch(f);
            while (1) {
                if (ch == 'e' || ch == 'E' || ch == 'p' || ch == 'P') {
                    text[idx++] = ch;
                    ch = next_ch(f);
                    if (ch == '+' || ch == '-') {
                        text[idx++] = ch;
                        ch = next_ch(f);
                    }
                } else if (isalnum(ch) || ch == '.' || ch == '_') {
                    text[idx++] = ch;
                    ch = next_ch(f);
                } else {
                    break;
                }
            }
            uget_ch(f, ch);
            text[idx] = '\0';
            tok = TK_PP_NUM;
            return;
        case '#':
            ch = next_ch(f);
            if (ch == '#') {
                tok = TK_HASHHASH;
            } else {
                uget_ch(f, ch);
                tok = '#';
            }
            return;
        case '+':
            ch = next_ch(f);
            if (ch == '+') {
                tok = TK_INC;
            } else if (ch == '=') {
                tok = TK_AS_ADD;
            } else {
                uget_ch(f, ch);
                tok = '+';
            }
            return;
        case '-':
            ch = next_ch(f);
            if (ch == '-') {
                tok = TK_DEC;
            } else if (ch == '=') {
                tok = TK_AS_SUB;
            } else if (ch == '>') {
                tok = TK_ARROW;
            } else {
                uget_ch(f, ch);
                tok = '-';
            }
            return;
        case '&':
            ch = next_ch(f);
            if (ch == '&') {
                tok = TK_LAND;
            } else if (ch == '=') {
                tok = TK_AS_AND;
            } else {
                uget_ch(f, ch);
                tok = '&';
            }
            return;
        case '|':
            ch = next_ch(f);
            if (ch == '|') {
                tok = TK_LOR;
            } else if (ch == '=') {
                tok = TK_AS_OR;
            } else {
                uget_ch(f, ch);
                tok = '|';
            }
            return;
        case '^':
            ch = next_ch(f);
            if (ch == '=') {
                tok = TK_AS_XOR;
            } else {
                uget_ch(f, ch);
                tok = '^';
            }
            return;
        case '*':
            ch = next_ch(f);
            if (ch == '=') {
                tok = TK_AS_MUL;
            } else {
                uget_ch(f, ch);
                tok = '*';
            }
            return;
        case '=':
            ch = next_ch(f);
            if (ch == '=') {
                tok = TK_EQ;
            } else {
                uget_ch(f, ch);
                tok = '=';
            }
            return;
        case '!':
            ch = next_ch(f);
            if (ch == '=') {
                tok = TK_NE;
            } else {
                uget_ch(f, ch);
                tok = '!';
            }
            return;
        case ':':
            ch = next_ch(f);
            if (ch == ':') {
                tok = TK_COLONCOLON;
            } else if (ch == '>') {
                tok = TK_RBRACKET;
            } else {
                uget_ch(f, ch);
                tok = ':';
            }
            return;
        case '<':
            ch = next_ch(f);
            if (ch == '<') {
                ch = next_ch(f);
                if (ch == '=') {
                    tok = TK_AS_SHL;
                } else {
                    tok = TK_SHL;
                    uget_ch(f, ch);
                }
            } else if (ch == '=') {
                tok = TK_LE;
            } else if (ch == ':') {
                tok = TK_LBRACKET;
            } else if (ch == '%') {
                tok = TK_LBRACE;
            } else {
                uget_ch(f, ch);
                tok = '<';
            }
            return;
        case '>':
            ch = next_ch(f);
            if (ch == '>') {
                ch = next_ch(f);
                if (ch == '=') {
                    tok = TK_AS_SHR;
                } else {
                    tok = TK_SHR;
                    uget_ch(f, ch);
                }
            } else if (ch == '=') {
                tok = TK_GE;
            } else {
                uget_ch(f, ch);
                tok = '>';
            }
            return;
        case '%':
            ch = next_ch(f);
            if (ch == ':') {
                ch = next_ch(f);
                if (ch == '%') {
                    ch = next_ch(f);
                    if (ch == ':') {
                        tok = TK_HASH2;
                    } else {
                        uget_ch(f, ch);
                        uget_ch(f, '%');
                        tok = TK_HASH;
                    }
                } else {
                    uget_ch(f, ch);
                    tok = TK_HASH;
                }
            } else if (ch == '=') {
                tok = TK_AS_MOD;
            } else if (ch == '>') {
                tok = TK_RBRACE;
            } else {
                uget_ch(f, ch);
                tok = '%';
            }
            return;
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
        case ',':
        case ';':
        case '~':
        case '?':
            tok = ch;
            return;
        default:
            tok = TK_OTHER;
            return;
    }
}

void tokenize(MccState *s) {
    MemoryBuffer mb;
    if (open_file(&mb, s->base_file)) fatal("%s: %s", s->base_file, strerror(errno));

    FileReader f = {
        .true_name = s->base_file,
        .diag_name = s->base_file,
        .mem_buf = mb,
        .line_num = 1,
    };

    while (tok != TK_EOF) {
        next_raw(&f);
        printf("tokkind = %d", tok);
        if (tok == TK_RAW_IDENT || tok == TK_PP_NUM || tok == TK_PP_STR || tok == TK_PP_CH) printf(" %s", text);
        printf("\n");
    }
}
