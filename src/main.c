#include "mcc.h"

// 打印使用说明.
static void usage(int status) {
    fprintf(stderr,
            "usage: mcc [options] <file>...\n"
            "\n"
            "  -E                   仅预处理; 不执行编译, 汇编或链接.\n"
            "  -S                   仅编译; 不执行汇编或链接.\n"
            "  -c                   编译并汇编; 不执行链接.\n"
            "  -o <file>            将输出写入 <file>.\n"
            "  -dump-raw-tokens     转储未经预处理的原始 token (调试用).\n"
            "  -###                 只打印将要执行的命令, 不执行.\n"
            "  --help               显示本帮助.\n");
    exit(status);
}

// 是否为"选项"(以 '-' 开头且后面还有字符).
#define IS_CMD(arg) (((arg)[0] == '-') && (arg)[1])
// 是否为"值"(不以 '-' 开头或只有 '-' 自身).
#define IS_ARG(arg) (((arg)[0] != '-') || !(arg)[1])

// 解析命令行参数.
static void parse_args(MccState *s, int argc, char **argv) {
    // 约定: 如果传入 -cc1 它必须是第一个参数.
    if (argc > 1 && !strcmp(argv[1], "-cc1")) {
        s->opt_cc1 = true;
        // 检查 -cc1-input <infile>
        if (argc <= 2 || strcmp(argv[2], "-cc1-input")) fatal("missing argument: -cc1-input");
        if (argc <= 3) fatal("missing value for -cc1-input");
        char *in_arg = argv[3];
        if (!IS_ARG(in_arg)) fatal("invalid value for -cc1-input: %s", in_arg);
        s->base_file = in_arg;

        // 检查 -cc1-output <outfile>
        if (argc <= 4 || strcmp(argv[4], "-cc1-output")) fatal("missing argument: -cc1-output");
        if (argc <= 5) fatal("missing value for -cc1-output");
        char *out_arg = argv[5];
        if (!IS_ARG(out_arg)) fatal("invalid value for -cc1-output: %s", out_arg);
        s->output_file = out_arg;

        // 解析其他参数
        for (int i = 6; i < argc; i++) {
            char *arg = argv[i];
            if (!strcmp(arg, "-dump-raw-tokens")) {
                s->opt_dump_raw_tokens = true;
                continue;
            }

            if (!strcmp(arg, "-E")) {
                s->opt_E = true;
                continue;
            }
            if (!strcmp(arg, "-S")) {
                s->opt_S = true;
                continue;
            }
            if (!strcmp(arg, "-emit-llvm")) {
                s->opt_ll = true;
                continue;
            }
            if (!strcmp(arg, "-c")) {
                s->opt_c = true;
                continue;
            }
            fatal("unexpected argument: %s", arg);
        }
    } else {
        for (int i = 1; i < argc; i++) {
            char *arg = argv[i];
            if (!strcmp(arg, "--help")) usage(0);

            if (!strcmp(arg, "-###")) {
                s->opt_hash3 = true;
                continue;
            }

            if (!strcmp(arg, "-dump-raw-tokens")) {
                s->cmd[s->num_cmd++] = arg;
                s->opt_dump_raw_tokens = true;
                continue;
            }
            if (!strcmp(arg, "-E")) {
                s->cmd[s->num_cmd++] = arg;
                s->opt_E = true;
                continue;
            }
            if (!strcmp(arg, "-S")) {
                s->cmd[s->num_cmd++] = arg;
                s->opt_S = true;
                continue;
            }
            if (!strcmp(arg, "-emit-llvm")) {
                s->cmd[s->num_cmd++] = arg;
                s->opt_ll = true;
                continue;
            }
            if (!strcmp(arg, "-c")) {
                s->cmd[s->num_cmd++] = arg;
                s->opt_c = true;
                continue;
            }

            if (!strcmp(arg, "-o")) {
                arg = argv[++i];
                if (!arg || !IS_ARG(arg)) fatal("missing argument: -o");
                s->opt_o = arg;
                continue;
            }
            // 紧贴写法 -oFILE (gcc 支持). 注意 "-o=FILE" 不是 '=' 语法:
            // gcc 把它读作 -o 加值 "=FILE", 所以这里也照 gcc 处理.
            if (!strncmp(arg, "-o", 2)) {
                s->opt_o = arg + 2;
                continue;
            }

            if (IS_CMD(arg)) fatal("unknown argument: %s", arg);
            s->input_paths[s->num_input++] = arg;
        }
        if (!s->num_input) fatal("no input files");
        if (s->num_input > 1 && s->opt_o && (s->opt_c || s->opt_S || s->opt_E))
            fatal("cannot specify '-o' with '-c' ,'-S' or '-E' with multiple files");
    }

    if (s->opt_ll && !s->opt_S) fatal("-emit-llvm must be used with -S");
}

// 运行子进程
static void run_subprocess(MccState *s, char **args) {
    // 如果 -###, 打印新命令行参数并返回.
    if (s->opt_hash3) {
        fprintf(stderr, "%s", args[0]);
        for (int i = 1; args[i]; i++) fprintf(stderr, " %s", args[i]);
        fprintf(stderr, "\n");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) fatal("fork failed: %s", strerror(errno));
    if (pid == 0) {
        // 用构造的新命令行参数运行子进程.
        execvp(args[0], args);
        fprintf(stderr, "exec failed: %s: %s\n", args[0], strerror(errno));
        _exit(1);
    }

    // 等待子进程运行完毕
    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) fatal("waitpid failed: %s", strerror(errno));
    }

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "mcc: internal compiler error: %s killed by signal %d\n", args[0], WTERMSIG(status));
        exit(1);
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        exit(1);
    }
}

// 修改文件后缀名
static char *replace_extn(char *tmpl, char *extn) {
    char *tmp = strdup(tmpl);
    // 丢掉目录部分: 排除目录名里的 '.'
    char *filename = basename(tmp);
    char *dot = strrchr(filename, '.');
    if (dot) *dot = '\0';
    char *result = format("%s%s", filename, extn);
    free(tmp);  // 释放 strdup 的指针
    return result;
}

// 核心编译函数
static void cc1(MccState *s) {
    // 打印未经预处理的原始token, 调试用
    if (s->opt_dump_raw_tokens) {
        tokenize(s);
        exit(0);
    }
    // TODO: 编译流水线. 尚未实现
    fatal("cc1: compilation pipeline not implemented yet (input: %s)", s->base_file ? s->base_file : "(none)");
}

static void run_cc1(MccState *s, char *input, char *output) {
    ASSERT(s->self, "argv[0] is empty");
    char **args = emalloc((s->num_cmd + 7) * sizeof(char *));
    args[0] = s->self;
    args[1] = "-cc1";
    args[2] = "-cc1-input";
    args[3] = input;
    args[4] = "-cc1-output";
    args[5] = output;

    memcpy(args + 6, s->cmd, s->num_cmd * sizeof(char *));
    // 显式添加终止符
    args[s->num_cmd + 6] = NULL;
    run_subprocess(s, args);
}

int main(int argc, char **argv) {
    MccState s = {};
    s.input_paths = emalloc(argc * sizeof(char *));
    s.cmd = emalloc(argc * sizeof(char *));
    s.self = argv[0];
    parse_args(&s, argc, argv);

    // 命令行直接传入 -cc1 或 mcc 执行自己
    if (s.opt_cc1) {
        cc1(&s);
        return 0;
    }

    for (int i = 0; i < s.num_input; i++) {
        char *input = s.input_paths[i];
        // TODO: 文件类型识别和分派尚未完成
        char *output;
        if (s.opt_o) {
            output = s.opt_o;
        } else if (s.opt_E) {
            output = "-";
        } else if (s.opt_S && s.opt_ll) {
            output = replace_extn(input, ".ll");
        } else if (s.opt_S) {
            output = replace_extn(input, ".s");
        } else {
            output = replace_extn(input, ".o");
        }
        run_cc1(&s, input, output);
    }

    return 0;
}
