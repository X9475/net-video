// 公共模块
// 声明几个实用函数
#pragma once
#include <string>
#include <vector>

// long long类型整数主机序转网络序
void llton(long long ll, char* n);
// long long类型整数网络序转主机序
long long ntoll(char const* n);

// long类型整数主机序转网络序
void lton(long l, char* n);
// long类型整数网络序转主机序
long ntol(char const* n);

// short类型整数主机序转网络序
void ston(short s, char* n);
// short类型整数网络序转主机序
short ntos(char const* n);

// 字符串是否合法，即是否只包含26个英文字母大小写和0到9十个阿拉伯数字
int valid(char const* str);

// 以分号为分隔符将一个字符串拆分为多个子字符串
int split(char const* str, std::vector<std::string>& substrs);
