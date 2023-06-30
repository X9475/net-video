// 存储服务器
// 声明全局变量
#pragma once
#include <vector>
#include <string>
#include <lib_acl.hpp>

// 配置信息
extern char* cfg_gpname; // 隶属组名
extern char* cfg_spaths; // 存储路径表
extern char* cfg_taddrs; // 跟踪服务器地址表
extern char* cfg_iaddrs; // ID服务器地址表
extern char* cfg_maddrs; // MySQL地址表
extern char* cfg_raddrs; // Redis地址表
extern acl::master_str_tbl cfg_str[]; // 字符串配置表

extern int cfg_bindport; // 绑定端口号
extern int cfg_interval; // 心跳间隔秒数
extern int cfg_mtimeout; // MySQL读写超时
extern int cfg_maxconns; // Redis连接池最大连接数
extern int cfg_ctimeout; // Redis连接超时
extern int cfg_rtimeout; // Redis读写超时
extern int cfg_ktimeout; // Redis键超时
extern acl::master_int_tbl cfg_int[]; // 整型配置表

extern std::vector<std::string> g_spaths; // 存储路径表
extern std::vector<std::string> g_taddrs; // 跟踪服务器地址表
extern std::vector<std::string> g_iaddrs; // ID服务器地址表
extern std::vector<std::string> g_maddrs; // MySQL地址表
extern std::vector<std::string> g_raddrs; // Redis地址表
extern acl::redis_client_pool*  g_rconns; // Redis连接池
extern std::string g_hostname; // 主机名
extern char const* g_version; // 版本
extern time_t g_stime; // 启动时间
