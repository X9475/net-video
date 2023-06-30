// 存储服务器
// 声明数据库访问类
#pragma once
#include <string>
#include <mysql.h>

// 数据库访问类
class db_c {
public:
	// 构造函数
	db_c(void);
	// 析构函数
	~db_c(void);

	// 连接数据库
	int connect(void);

	// 根据文件ID获取其对应的路径及大小
	int get(char const* appid, char const* userid, char const* fileid, std::string& filepath,
		long long* filesize) const;
	// 设置文件ID和路径及大小的对应关系
	int set(char const* appid, char const* userid, char const* fileid, char const* filepath, 
		long long filesize) const;
	// 删除文件ID
	int del(char const* appid, char const*userid, char const* fileid) const;

private:
	// 根据用户ID获取其对应的表名
	std::string table_of_user(char const* userid) const;

	// 计算哈希值
	unsigned int hash(char const* buf, size_t len) const;

	MYSQL* m_mysql; // MySQL对象
};
