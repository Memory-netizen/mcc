# mcc

A mini C compiler.

mcc 是一个小型 C 编译器，实现 C2y 的一个子集，并兼容部分 GNU 扩展。
项目目标是最终能够自编译、编译部分开源项目。

实现参考了 acwj、subc、chibicc、tinycc 和 qbe。

## 标准与目标

- 主要标准：C2y 子集
- 扩展：部分 GNU 扩展
- 目标：自编译、编译部分开源项目
- 实现语言：C
- 后端：llvm

## 当前状态

项目处于开发阶段，尚未完整实现 C2y 标准。

## 支持的特性

- 大部分 C2y 特性
- 部分 GNU 扩展

## 明确不支持

- 十进制浮点类型：
  - `_Decimal32`
  - `_Decimal64`
  - `_Decimal128`
- 十进制浮点后缀：
  - `df`、`dd`、`dl`
  - `DF`、`DD`、`DL`
- 复数类型：
  - `_Complex`
- 复数后缀：
  - `i`、`I`、`j`、`J`

## 构建

依赖：
- C 编译器
- make


构建命令：
```sh
make            # DEBUG=1 (default) -g -O0
make DEBUG=0    # -O2 -DNDEBUG
```

编译选项：
```
-Wall -Wextra -Werror -std=c23 -fno-common -I./src
```

## 参考项目

- acwj:     https://github.com/DoctorWkt/acwj
- subc:     http://t3x.org/subc/
- chibicc:  https://github.com/rui314/chibicc
- tinycc:   https://github.com/TinyCC/tinycc
- qbe:      https://c9x.me/compile/

## 许可证

MIT License
