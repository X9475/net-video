// ID服务器
// 声明全局变量
#pragma once
#include <vector>
#include <lib_acl.hpp>
#include "01_types.h"

// 配置信息
extern char* cfg_maddrs; // MySQL地址表
extern acl::master_str_tbl cfg_str[]; // 字符串配置表

extern int cfg_mtimeout;  // MySQL读写超时
extern int cfg_maxoffset; // 最大偏移
extern acl::master_int_tbl cfg_int[]; // 整型配置表

extern std::vector<std::string> g_maddrs; // MySQL地址表
extern std::string g_hostname; // 主机名
extern std::vector<id_pair_t> g_ids; // ID表
extern pthread_mutex_t g_mutex; // 互斥锁
