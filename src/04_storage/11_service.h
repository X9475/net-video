// 存储服务器
// 声明业务服务类
#pragma once
#include <lib_acl.hpp>

// 业务服务类
class service_c {
public:
	// 业务处理
	bool business(acl::socket_stream* conn, char const* head) const;

private:
	// 处理来自客户机的上传文件请求
	bool upload(acl::socket_stream* conn, long long bodylen) const;
	// 处理来自客户机的询问文件大小请求
	bool filesize(acl::socket_stream* conn, long long bodylen) const;
	// 处理来自客户机的下载文件请求
	bool download(acl::socket_stream* conn, long long bodylen) const;
	// 处理来自客户机的删除文件请求
	bool del(acl::socket_stream* conn, long long bodylen) const;

	// 生成文件路径
	int genpath(char* filepath) const;
	// 将ID转换为512进制
	long id512(long id) const;
	// 用文件ID生成文件路径
	int id2path(char const* spath, long fileid, char* filepath) const;
	// 接收并保存文件
	int save(acl::socket_stream* conn, char const* appid, char const* userid, 
		char const* fileid, long long filesize, char const* filepath) const;

	// 读取并发送文件
	int send(acl::socket_stream* conn, char const* filepath, long long offset, long long size) const;

	// 应答成功
	bool ok(acl::socket_stream* conn) const;
	// 应答错误
	bool error(acl::socket_stream* conn, short errnumb, char const* format, ...) const;
};
