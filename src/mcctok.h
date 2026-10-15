#ifndef MCCTOK_H_
#define MCCTOK_H_
enum {
    TK_EOF = -1,
    TK_NONE = 0,
    TK_NL = '\n',
    TK_WS = ' ',

    TK_AS_ADD = 0x80,  // +=
    TK_AS_SUB,         // -=
    TK_AS_MUL,         // *=
    TK_AS_DIV,         // /=
    TK_AS_MOD,         // %=
    TK_AS_AND,         // &=
    TK_AS_OR,          // |=
    TK_AS_XOR,         // ^=
    TK_AS_SHL,         // <<=
    TK_AS_SHR,         // >>=
    TK_LOR,            // ||
    TK_LAND,           // &&
    TK_EQ,             // ==
    TK_NE,             // !=
    TK_LE,             // <=
    TK_GE,             // >=
    TK_SHL,            // <<
    TK_SHR,            // >>
    TK_INC,            // ++
    TK_DEC,            // --
    TK_ARROW,          // ->

    TK_COLONCOLON,  // ::
    TK_ELLIPSIS,    // ...
    TK_HASHHASH,    // ##

    TK_LBRACKET,  // <:,   [  的替代拼法
    TK_RBRACKET,  // :>,   ]  的替代拼法
    TK_LBRACE,    // <%,   {  的替代拼法
    TK_RBRACE,    // %>,   }  的替代拼法
    TK_HASH,      // %:,   #  的替代拼法
    TK_HASH2,     // %:%:, ## 的替代拼法

    TK_PP_CH,              // 未解析的字符字面量
    TK_PP_STR,             // 未解析的字符串字面量
    TK_PP_NUM,             // 未解析的数字字面量
    TK_RAW_IDENT = 0x100,  // 未解析的标识符, 包括关键字
    TK_OTHER,
};
#endif  // MCCTOK_H_
