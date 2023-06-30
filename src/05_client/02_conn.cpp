// 客户机
// 实现连接类
#include <sys/sendfile.h>
#include <lib_acl.h>
#include "02_proto.h"
#include "03_util.h"
#include "01_conn.h"

// 构造函数
conn_c::conn_c(char const* destaddr, int ctimeout /* = 30 */, int rtimeout /* = 60 */):
    m_ctimeout(ctimeout), m_rtimeout(rtimeout), m_conn(NULL) {
	// 检查目的地址
	acl_assert(destaddr && *destaddr);
	// 复制目的地址
	m_destaddr = acl_mystrdup(destaddr);
}

// 析构函数
conn_c::~conn_c(void) {
	// 关闭连接
	close();
	// 释放目的地址
	acl_myfree(m_destaddr);
}

// 从跟踪服务器获取存储服务器地址列表
int conn_c::saddrs(char const* appid, char const* userid, char const* fileid, std::string& saddrs) {
	// |包体长度|命令|状态|应用ID|用户ID|文件ID|
	// |    8   |  1 |  1 |  16  |  256 |  128 |
	// 构造请求
	long long bodylen = APPID_SIZE + USERID_SIZE + FILEID_SIZE;
	long long requlen = HEADLEN + bodylen;
	char requ[requlen];
	if (makerequ(CMD_TRACKER_SADDRS, appid, userid, fileid, requ) != OK)
		return ERROR;
	llton(bodylen, requ);
    // 尝试打开
	if (!open())
		return SOCKET_ERROR;

	// 发送请求
	if (m_conn->write(requ, requlen) < 0) {
		logger_error("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, m_conn->get_peer());
		m_errnumb = -1;
		m_errdesc.format("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, m_conn->get_peer());
		close();
		return SOCKET_ERROR;
	}

	char* body = NULL; // 包体指针

	// 接收包体
	int result = recvbody(&body, &bodylen); // 此时的包体长度为响应的包体长度

	// 解析包体
	if (result == OK)
		// |包体长度|命令|状态|组名|存储服务器地址列表|
		// |    8   |  1 |  1 |16+1|  包体长度-(16+1) |
		saddrs = body + STORAGE_GROUPNAME_MAX + 1;
	else if (result == STATUS_ERROR) {
		// |包体长度|命令|状态|错误号|错误描述|
		// |    8   |  1 |  1 |   2  | <=1024 |
		m_errnumb = ntos(body);
		m_errdesc = bodylen > ERROR_NUMB_SIZE ? body + ERROR_NUMB_SIZE : "";
	}

	// 释放包体
	if (body) {
		free(body);
		body = NULL;
	}

	return result;
}

// 从跟踪服务器获取组列表
int conn_c::groups(std::string& groups) {
	// |包体长度|命令|状态|
	// |    8   |  1 |  1 |
	// 构造请求
	long long bodylen = 0;
	long long requlen = HEADLEN + bodylen;
	char requ[requlen] = {};
	llton(bodylen, requ);
	requ[BODYLEN_SIZE] = CMD_TRACKER_GROUPS;
	requ[BODYLEN_SIZE+COMMAND_SIZE] = 0;

	if (!open())
		return SOCKET_ERROR;

	// 发送请求
	if (m_conn->write(requ, requlen) < 0) {
		logger_error("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, m_conn->get_peer());
		m_errnumb = -1;
		m_errdesc.format("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, m_conn->get_peer());
		close();
		return SOCKET_ERROR;
	}

	char* body = NULL; // 包体指针

	// 接收包体
	int result = recvbody(&body, &bodylen);

	// 解析包体
	if (result == OK)
		// |包体长度|命令|状态| 组列表 |
		// |    8   |  1 |  1 |包体长度|
		groups = body;
	else if (result == STATUS_ERROR) {
		// |包体长度|命令|状态|错误号|错误描述|
		// |    8   |  1 |  1 |   2  | <=1024 |
		m_errnumb = ntos(body);
		m_errdesc = bodylen > ERROR_NUMB_SIZE ? body + ERROR_NUMB_SIZE : "";
	}

	// 释放包体
	if (body) {
		free(body);
		body = NULL;
	}

	return result;
}

// 向存储服务器上传文件
int conn_c::upload(char const* appid, char const* userid, char const* fileid, char const* filepath) {
	// |包体长度 |命令 |状态 |应用ID|用户ID |文件ID|文件大小 |文件内容 |
	// |    8   |  1 |  1 |  16  |  256 |  128 |    8   |文件大小|
	// 构造请求
	long long bodylen = APPID_SIZE + USERID_SIZE + FILEID_SIZE + BODYLEN_SIZE;
	long long requlen = HEADLEN + bodylen;
	char requ[requlen];
	if (makerequ(CMD_STORAGE_UPLOAD, appid, userid, fileid, requ) != OK)
		return ERROR;
	acl::ifstream ifs; // 输入文件流
	if (!ifs.open_read(filepath)) {
		logger_error("open file fail, filepath: %s", filepath);
		return ERROR;
	}
	long long filesize = ifs.fsize(); // 文件大小
	llton(filesize, requ + HEADLEN + APPID_SIZE + USERID_SIZE + FILEID_SIZE);
	bodylen += filesize; 
	llton(bodylen, requ);

	if (!open())
		return SOCKET_ERROR;

	// 发送请求
	if (m_conn->write(requ, requlen) < 0) {
		logger_error("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, m_conn->get_peer());
		m_errnumb = -1;
		m_errdesc.format("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, m_conn->get_peer());
		close();
		return SOCKET_ERROR;
	}

	// 发送文件
	long long remain = filesize; // 未发送字节数
	off_t offset = 0; // 文件读写位置
	while (remain) { // 还有未发送数据
		// 发送数据
		long long bytes = std::min(remain, (long long)STORAGE_RCVWR_SIZE);
		long long count = sendfile(m_conn->sock_handle(), ifs.file_handle(), &offset, bytes);
		if (count < 0) {
			logger_error("send file fail, filesize: %lld, remain: %lld", filesize, remain);
			m_errnumb = -1;
			m_errdesc.format("send file fail, filesize: %lld, remain: %lld", filesize, remain);
			close();
			return SOCKET_ERROR;
		}
		// 剩余字节数
		remain -= count;
	}
	ifs.close();

	char* body = NULL; // 包体指针

	// 接收包体
	int result = recvbody(&body, &bodylen);

	// 解析包体
	if (result == STATUS_ERROR) {
		// |包体长度|命令|状态|错误号|错误描述|
		// |    8   |  1 |  1 |   2  | <=1024 |
		m_errnumb = ntos(body);
		m_errdesc = bodylen > ERROR_NUMB_SIZE ? body + ERROR_NUMB_SIZE : "";
	}

	// 释放包体
	if (body) {
		free(body);
		body = NULL;
	}

	return result;
}

// 向存储服务器上传文件
int conn_c::upload(char const* appid, char const* userid, char const* fileid, char const* filedata, long long filesize) {
	// |包体长度|命令|状态|应用ID|用户ID|文件ID|文件大小|文件内容|
	// |    8   |  1 |  1 |  16  |  256 |  128 |    8   |文件大小|
	// 构造请求
	long long bodylen = APPID_SIZE + USERID_SIZE + FILEID_SIZE + BODYLEN_SIZE;
	long long requlen = HEADLEN + bodylen;
	char requ[requlen];
	if (makerequ(CMD_STORAGE_UPLOAD, appid, userid, fileid, requ) != OK)
		return ERROR;
	llton(filesize, requ + HEADLEN + APPID_SIZE + USERID_SIZE + FILEID_SIZE);
	bodylen += filesize;
	llton(bodylen, requ);

	if (!open())
		return SOCKET_ERROR;

	// 发送请求
	if (m_conn->write(requ, requlen) < 0) {
		logger_error("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, m_conn->get_peer());
		m_errnumb = -1;
		m_errdesc.format("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, m_conn->get_peer());
		close();
		return SOCKET_ERROR;
	}

	// 发送文件
	if (m_conn->write(filedata, filesize) < 0) {
		logger_error("write fail: %s, filesize: %lld, to: %s" , acl::last_serror(), filesize, m_conn->get_peer());
		m_errnumb = -1;
		m_errdesc.format("write fail: %s, filesize: %lld, to: %s", acl::last_serror(), filesize, m_conn->get_peer());
		close();
		return SOCKET_ERROR;
	}

	char* body = NULL; // 包体指针

	// 接收包体
	int result = recvbody(&body, &bodylen);

	// 解析包体
	if (result == STATUS_ERROR) {
		// |包体长度|命令|状态|错误号|错误描述|
		// |    8   |  1 |  1 |   2  | <=1024 |
		m_errnumb = ntos(body);
		m_errdesc = bodylen > ERROR_NUMB_SIZE ? body + ERROR_NUMB_SIZE : "";
	}

	// 释放包体
	if (body) {
		free(body);
		body = NULL;
	}

	return result;
}

// 向存储服务器询问文件大小
int conn_c::filesize(char const* appid, char const* userid, char const* fileid, long long* filesize) {
	// |包体长度|命令|状态|应用ID|用户ID|文件ID|
	// |    8   |  1 |  1 |  16  |  256 |  128 |
	// 构造请求
	long long bodylen = APPID_SIZE + USERID_SIZE + FILEID_SIZE;
	long long requlen = HEADLEN + bodylen;
	char requ[requlen];
	if (makerequ(CMD_STORAGE_FILESIZE, appid, userid, fileid, requ) != OK)
		return ERROR;
	llton(bodylen, requ);

	if (!open())
		return SOCKET_ERROR;

	// 发送请求
	if (m_conn->write(requ, requlen) < 0) {
		logger_error("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, m_conn->get_peer());
		m_errnumb = -1;
		m_errdesc.format("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, m_conn->get_peer());
		close();
		return SOCKET_ERROR;
	}

	char* body = NULL; // 包体指针

	// 接收包体
	int result = recvbody(&body, &bodylen);

	// 解析包体
	if (result == OK)
		// |包体长度|命令|状态|文件大小|
		// |    8   |  1 |  1 |    8   |
		*filesize = ntoll(body);
	else if (result == STATUS_ERROR) {
		// |包体长度|命令|状态|错误号|错误描述|
		// |    8   |  1 |  1 |   2  | <=1024 |
		m_errnumb = ntos(body);
		m_errdesc = bodylen > ERROR_NUMB_SIZE ? body + ERROR_NUMB_SIZE : "";
	}

	// 释放包体
	if (body) {
		free(body);
		body = NULL;
	}

	return result;
}

// 从存储服务器下载文件
int conn_c::download(char const* appid, char const* userid, char const* fileid, long long offset, long long size,
	char** filedata, long long* filesize) {
	// |包体长度 |命令 |状态 |应用ID|用户ID |文件ID |偏移|大小 |
	// |    8   |  1 |  1 |  16  |  256 |  128 |  8 |  8 |
	// 构造请求
	long long bodylen = APPID_SIZE + USERID_SIZE + FILEID_SIZE + BODYLEN_SIZE + BODYLEN_SIZE;
	long long requlen = HEADLEN + bodylen;
	char requ[requlen];
	if (makerequ(CMD_STORAGE_DOWNLOAD, appid, userid, fileid, requ) != OK)
		return ERROR;
	llton(bodylen, requ);
	llton(offset, requ + HEADLEN + APPID_SIZE + USERID_SIZE + FILEID_SIZE);
	llton(size, requ + HEADLEN + APPID_SIZE + USERID_SIZE + FILEID_SIZE + BODYLEN_SIZE);

	if (!open())
		return SOCKET_ERROR;

	// 发送请求
	if (m_conn->write(requ, requlen) < 0) {
		logger_error("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, m_conn->get_peer());
		m_errnumb = -1;
		m_errdesc.format("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, m_conn->get_peer());
		close();
		return SOCKET_ERROR;
	}

	char* body = NULL; // 包体指针

	// 接收包体
	int result = recvbody(&body, &bodylen);

	// 解析包体
	if (result == OK) {
		// |包体长度|命令|状态|文件内容|
		// |    8   |  1 |  1 |内容大小|
		*filedata = body;
		*filesize = bodylen;
		return result;
	}
	if (result == STATUS_ERROR) {
		// |包体长度|命令|状态|错误号|错误描述|
		// |    8   |  1 |  1 |   2  | <=1024 |
		m_errnumb = ntos(body);
		m_errdesc = bodylen > ERROR_NUMB_SIZE ? body + ERROR_NUMB_SIZE : "";
	}

	// 释放包体
	if (body) {
		free(body);
		body = NULL;
	}

	return result;
}

// 删除存储服务器上的文件
int conn_c::del(char const* appid, char const* userid, char const* fileid) {
	// |包体长度|命令|状态|应用ID|用户ID|文件ID|
	// |    8   |  1 |  1 |  16  |  256 |  128 |
	// 构造请求
	long long bodylen = APPID_SIZE + USERID_SIZE + FILEID_SIZE;
	long long requlen = HEADLEN + bodylen;
	char requ[requlen];
	if (makerequ(CMD_STORAGE_DELETE, appid, userid, fileid, requ) != OK)
		return ERROR;
	llton(bodylen, requ);

	if (!open())
		return SOCKET_ERROR;

	// 发送请求
	if (m_conn->write(requ, requlen) < 0) {
		logger_error("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, m_conn->get_peer());
		m_errnumb = -1;
		m_errdesc.format("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, m_conn->get_peer());
		close();
		return SOCKET_ERROR;
	}

	char* body = NULL; // 包体指针

	// 接收包体
	int result = recvbody(&body, &bodylen);

	// 解析包体
	if (result == STATUS_ERROR) {
		// |包体长度|命令|状态|错误号|错误描述|
		// |    8   |  1 |  1 |   2  | <=1024 |
		m_errnumb = ntos(body);
		m_errdesc = bodylen > ERROR_NUMB_SIZE ? body + ERROR_NUMB_SIZE : "";
	}

	// 释放包体
	if (body) {
		free(body);
		body = NULL;
	}

	return result;
}

// 获取错误号
short conn_c::errnumb(void) const {
	return m_errnumb;
}

// 获取错误描述
char const* conn_c::errdesc(void) const {
	return m_errdesc.c_str();
}

// 打开连接
bool conn_c::open(void) {
    // 判断是否有对象
	if (m_conn)
		return true;

	// 创建连接对象
	m_conn = new acl::socket_stream;

	// 连接目的主机
	if (!m_conn->open(m_destaddr, m_ctimeout, m_rtimeout)) {
		logger_error("open %s fail: %s", m_destaddr, acl_last_serror());
		delete m_conn;
		m_conn = NULL;
		m_errnumb = -1;
		m_errdesc.format("open %s fail: %s", m_destaddr, acl_last_serror());
		return false;
	}

	return true;
}

// 关闭连接
void conn_c::close(void) {
	if (m_conn) {
		delete m_conn;
		m_conn = NULL;
	}
}

// 构造请求
int conn_c::makerequ(char command, char const* appid, char const* userid, char const* fileid, char* requ) {
	// |包体长度|命令|状态|应用ID|用户ID|文件ID|
	// |    8   |  1 |  1 |  16  |  256 |  128 |
	requ[BODYLEN_SIZE] = command; // 命令
	requ[BODYLEN_SIZE+COMMAND_SIZE] = 0; // 状态

	// 应用ID
	if (strlen(appid) >= APPID_SIZE) { 
		logger_error("appid too big, %lu >= %d", strlen(appid), APPID_SIZE);
		m_errnumb = -1;
		m_errdesc.format("appid too big, %lu >= %d", strlen(appid), APPID_SIZE);
		return ERROR;
	}
	strcpy(requ + HEADLEN, appid);

	// 用户ID
	if (strlen(userid) >= USERID_SIZE) {
		logger_error("userid too big, %lu >= %d", strlen(userid), USERID_SIZE);
		m_errnumb = -1;
		m_errdesc.format("userid too big, %lu >= %d", strlen(userid), USERID_SIZE);
		return ERROR;
	}
	strcpy(requ + HEADLEN + APPID_SIZE, userid);

	// 文件ID
	if (strlen(fileid) >= FILEID_SIZE) {
		logger_error("fileid too big, %lu >= %d", strlen(fileid), FILEID_SIZE);
		m_errnumb = -1;
		m_errdesc.format("fileid too big, %lu >= %d", strlen(fileid), FILEID_SIZE);
		return ERROR;
	}
	strcpy(requ + HEADLEN + APPID_SIZE + USERID_SIZE, fileid);

	return OK;
}

// 接收包体,二级指针接收动态分配内存并带回
int conn_c::recvbody(char** body, long long* bodylen) {
	// 接收包头
	int result = recvhead(bodylen);

	// 既非本地错误亦非套接字通信错误且包体非空
	if (result != ERROR && result != SOCKET_ERROR && *bodylen) {
		// 分配包体
		if (!(*body = (char*)malloc(*bodylen))) {
			logger_error("call malloc fail: %s, bodylen: %lld", strerror(errno), *bodylen);
			m_errnumb = -1;
			m_errdesc.format("call malloc fail: %s, bodylen: %lld", strerror(errno), *bodylen);
			return ERROR;
		}

		// 接收包体
		if (m_conn->read(*body, *bodylen) < 0) {
			logger_error("read fail: %s, from: %s", acl::last_serror(), m_conn->get_peer());
			m_errnumb = -1;
			m_errdesc.format("read fail: %s, from: %s", acl::last_serror(), m_conn->get_peer());
			free(*body);
			*body = NULL;
			close();
			return SOCKET_ERROR;
		}
	}

	return result;
}

// 接收包头
int conn_c::recvhead(long long* bodylen) {
	if (!open())
		return SOCKET_ERROR;

	char head[HEADLEN]; // 包头缓冲区

	// 接收包头
	if (m_conn->read(head, HEADLEN) < 0) {
		logger_error("read fail: %s, from: %s", acl::last_serror(), m_conn->get_peer());
		m_errnumb = -1;
		m_errdesc.format("read fail: %s, from: %s", acl::last_serror(), m_conn->get_peer());
		close();
		return SOCKET_ERROR;
	}

	// |包体长度|命令|状态|
	// |    8   |  1 |  1 |
	// 解析包头
	if ((*bodylen = ntoll(head)) < 0) { // 响应的包体长度
		logger_error("invalid body length: %lld < 0, from: %s", *bodylen, m_conn->get_peer());
		m_errnumb = -1;
		m_errdesc.format("invalid body length: %lld < 0, from: %s", *bodylen, m_conn->get_peer());
		return ERROR;
	}
	int command = head[BODYLEN_SIZE]; // 命令
	int status = head[BODYLEN_SIZE+COMMAND_SIZE]; // 状态
	if (status) {
		logger_error("response status %d != 0, from: %s", status, m_conn->get_peer());
		return STATUS_ERROR;
	}
	logger("bodylen: %lld, command: %d, status: %d", *bodylen, command, status);

	return OK;
}
