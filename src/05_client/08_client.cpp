// 客户机
// 实现客户机类
#include <lib_acl.h>
#include "01_types.h"
#include "03_util.h"
#include "01_conn.h"
#include "03_pool.h"
#include "05_mngr.h"
#include "07_client.h"

#define MAX_SOCKERRS 10 // 套接字通信错误最大次数

acl::connect_manager* client_c::s_mngr = NULL;
std::vector<std::string> client_c::s_taddrs; // 跟踪服务器地址表
int client_c::s_scount = 8;

// 初始化
int client_c::init(char const* taddrs, int tcount /* = 16 */, int scount /* = 8 */) {
	if (s_mngr)
		return OK;
	// 跟踪服务器地址表
	if (!taddrs || !strlen(taddrs)) {
		logger_error("tracker addresses is null");
		return ERROR;
	}
	split(taddrs, s_taddrs);
	if (s_taddrs.empty()) {
		logger_error("tracker addresses is empty");
		return ERROR;
	}
	// 跟踪服务器连接数上限
	if (tcount <= 0) {
		logger_error("invalid tracker connection pool count %d <= 0", tcount);
		return ERROR;
	}
	// 存储服务器连接数上限
	if (scount <= 0) {
		logger_error("invalid storage connection pool count %d <= 0", scount);
		return ERROR;
	}
	// 存储服务器连接数上限
	s_scount = scount;

	// 创建连接池管理器
	if (!(s_mngr = new mngr_c)) {
		logger_error("create connection pool manager fail: %s", acl_last_serror());
		return ERROR;
	}
	// 初始化跟踪服务器连接池,该函数调用 set 过程添加每个服务的连接池
	// taddrs: IP:PORT:COUNT,IP:PORT:COUNT,IP:PORT;IP:PORT ...
	// taddrs  如：127.0.0.1:7777:50;192.168.1.1:7777:10;127.0.0.1:7778
	s_mngr->init(NULL, taddrs, tcount);

	return OK;
}

// 终结化
void client_c::deinit(void) {
	if (s_mngr) {
		delete s_mngr;
		s_mngr = NULL;
	}
	s_taddrs.clear();
}

// 从跟踪服务器获取存储服务器地址列表
int client_c::saddrs(char const* appid, char const* userid, char const* fileid, std::string& saddrs) {
    // 跟踪服务器列表不能为空
	if (s_taddrs.empty()) {
		logger_error("tracker addresses is empty");
		return ERROR;
	}

	int result = ERROR;

	// 生成有限随机数
	srand(time(NULL));
	int ntaddrs = s_taddrs.size();
	int nrand = rand() % ntaddrs;

	for (int i = 0; i < ntaddrs; ++i) {
		// 随机抽取跟踪服务器地址
		char const* taddr = s_taddrs[nrand].c_str();
		nrand = (nrand + 1) % ntaddrs;

		// 获取跟踪服务器连接池
		pool_c* tpool = (pool_c*)s_mngr->get(taddr);
		if (!tpool) {
			logger_warn("tracker connection pool is null, taddr: %s", taddr);
			continue;
		}

		for (int sockerrs = 0; sockerrs < MAX_SOCKERRS; ++sockerrs) {
			// 获取跟踪服务器连接
			conn_c* tconn = (conn_c*)tpool->peek();
			if (!tconn) {
				logger_warn("tracker connection is null, taddr: %s", taddr);
				break;
			}

			// 从跟踪服务器获取存储服务器地址列表
			result = tconn->saddrs(appid, userid, fileid, saddrs);

			if (result == SOCKET_ERROR) {
				logger_warn("get storage addresses fail: %s", tconn->errdesc());
				// 释放一个连接至连接池中
				tpool->put(tconn, false);
			} else {
				if (result == OK)
					// 释放一个连接至连接池中,针对该连接保持长连接
					tpool->put(tconn, true);
				else {
					logger_error("get storage addresses fail: %s", tconn->errdesc());
					tpool->put(tconn, false);
				}
				return result;
			}
		}
	}

	return result;
}

// 从跟踪服务器获取组列表
int client_c::groups(std::string& groups) {
	if (s_taddrs.empty()) {
		logger_error("tracker addresses is empty");
		return ERROR;
	}

	int result = ERROR;

	// 生成有限随机数
	srand(time(NULL));
	int ntaddrs = s_taddrs.size();
	int nrand = rand() % ntaddrs;

	for (int i = 0; i < ntaddrs; ++i) {
		// 随机抽取跟踪服务器地址
		char const* taddr = s_taddrs[nrand].c_str();
		nrand = (nrand + 1) % ntaddrs;

		// 获取跟踪服务器连接池
		pool_c* tpool = (pool_c*)s_mngr->get(taddr);
		if (!tpool) {
			logger_warn("tracker connection pool is null, taddr: %s", taddr);
			continue;
		}

		for (int sockerrs = 0; sockerrs < MAX_SOCKERRS; ++sockerrs) {
			// 获取跟踪服务器连接
			conn_c* tconn = (conn_c*)tpool->peek();
			if (!tconn) {
				logger_warn("tracker connection is null, taddr: %s", taddr);
				break;
			}

			// 从跟踪服务器获取组列表
			result = tconn->groups(groups);

			if (result == SOCKET_ERROR) {
				logger_warn("get groups fail: %s", tconn->errdesc());
				tpool->put(tconn, false);
			}
			else {
				if (result == OK)
					// 释放一个连接至连接池中,针对该连接保持长连接
					tpool->put(tconn, true);
				else {
					logger_error("get groups fail: %s", tconn->errdesc());
					tpool->put(tconn, false);
				}
				return result;
			}
		}
	}

	return result;
}

// 向存储服务器上传文件
int client_c::upload(char const* appid, char const* userid, char const* fileid, char const* filepath) {
	// 检查参数
	if (!appid || !strlen(appid)) {
		logger_error("appid is null");
		return ERROR;
	}
	if (!userid || !strlen(userid)) {
		logger_error("userid is null");
		return ERROR;
	}
	if (!fileid || !strlen(fileid)) {
		logger_error("fileid is null");
		return ERROR;
	}
	if (!filepath || !strlen(filepath)) {
		logger_error("filepath is null");
		return ERROR;
	}

	// 从跟踪服务器获取存储服务器地址列表
	int result;
	std::string ssaddrs;
	if ((result = saddrs(appid, userid, fileid, ssaddrs)) != OK)
		return result;
	std::vector<std::string> vsaddrs; // 保存存储服务器列表
	split(ssaddrs.c_str(), vsaddrs);
	if (vsaddrs.empty()) {
		logger_error("storage addresses is empty");
		return ERROR;
	}

	result = ERROR;

	for (std::vector<std::string>::const_iterator saddr = vsaddrs.begin(); saddr != vsaddrs.end(); ++saddr) {
		// 获取存储服务器连接池
		pool_c* spool = (pool_c*)s_mngr->get(saddr->c_str());
		if (!spool) {
			// 添加服务器的客户端连接池，该函数可以在程序运行时被调用，内部自动加锁,s_scount 连接池数量限制
			s_mngr->set(saddr->c_str(), s_scount);
			// 确认是否添加到连接池 
			if (!(spool = (pool_c*)s_mngr->get(saddr->c_str()))) {
				logger_warn("storage connection pool is null, saddr: %s", saddr->c_str());
				continue;
			}
		}

		for (int sockerrs = 0; sockerrs < MAX_SOCKERRS; ++sockerrs) {
			// 获取存储服务器连接
			conn_c* sconn = (conn_c*)spool->peek();
			if (!sconn) {
				logger_warn("storage connection is null, saddr: %s", saddr->c_str());
				break;
			}

			// 向存储服务器上传文件
			result = sconn->upload(appid, userid, fileid, filepath);

			if (result == SOCKET_ERROR) {
				logger_warn("upload file fail: %s", sconn->errdesc());
				spool->put(sconn, false);
			} else {
				if (result == OK)
					// 释放一个连接至连接池中,针对该连接保持长连接
					spool->put(sconn, true);
				else {
					logger_error("upload file fail: %s", sconn->errdesc());
					spool->put(sconn, false);
				}
				return result;
			}
		}
	}

	return result;
}

// 向存储服务器上传文件
int client_c::upload(char const* appid, char const* userid, char const* fileid, char const* filedata, long long filesize) {
	// 检查参数
	if (!appid || !strlen(appid)) {
		logger_error("appid is null");
		return ERROR;
	}
	if (!userid || !strlen(userid)) {
		logger_error("userid is null");
		return ERROR;
	}
	if (!fileid || !strlen(fileid)) {
		logger_error("fileid is null");
		return ERROR;
	}
	if (!filedata || !filesize) {
		logger_error("filedata is null");
		return ERROR;
	}

	// 从跟踪服务器获取存储服务器地址列表
	int result;
	std::string ssaddrs;
	if ((result = saddrs(appid, userid, fileid, ssaddrs)) != OK)
		return result;
	std::vector<std::string> vsaddrs;
	split(ssaddrs.c_str(), vsaddrs);
	if (vsaddrs.empty()) {
		logger_error("storage addresses is empty");
		return ERROR;
	}

	result = ERROR;

	for (std::vector<std::string>::const_iterator saddr = vsaddrs.begin(); saddr != vsaddrs.end(); ++saddr) {
		// 获取存储服务器连接池
		pool_c* spool = (pool_c*)s_mngr->get(saddr->c_str());
		if (!spool) {
			// 添加服务器的客户端连接池，该函数可以在程序运行时被调用，内部自动加锁,s_scount 连接池数量限制
			s_mngr->set(saddr->c_str(), s_scount);
			// 确认是否添加到连接池 
			if (!(spool = (pool_c*)s_mngr->get(saddr->c_str()))) {
				logger_warn("storage connection pool is null, saddr: %s", saddr->c_str());
				continue;
			}
		}

		for (int sockerrs = 0; sockerrs < MAX_SOCKERRS; ++sockerrs) {
			// 获取存储服务器连接
			conn_c* sconn = (conn_c*)spool->peek();
			if (!sconn) {
				logger_warn("storage connection is null, saddr: %s", saddr->c_str());
				break;
			}

			// 向存储服务器上传文件
			result = sconn->upload(appid, userid, fileid, filedata, filesize);

			if (result == SOCKET_ERROR) {
				logger_warn("upload file fail: %s", sconn->errdesc());
				spool->put(sconn, false);
			} else {
				if (result == OK)
					// 释放一个连接至连接池中,针对该连接保持长连接
					spool->put(sconn, true);
				else {
					logger_error("upload file fail: %s", sconn->errdesc());
					spool->put(sconn, false);
				}
				return result;
			}
		}
	}

	return result;
}

// 向存储服务器询问文件大小
int client_c::filesize(char const* appid, char const* userid, char const* fileid, long long* filesize) {
	// 检查参数
	if (!appid || !strlen(appid)) {
		logger_error("appid is null");
		return ERROR;
	}
	if (!userid || !strlen(userid)) {
		logger_error("userid is null");
		return ERROR;
	}
	if (!fileid || !strlen(fileid)) {
		logger_error("fileid is null");
		return ERROR;
	}

	// 从跟踪服务器获取存储服务器地址列表
	int result;
	std::string ssaddrs;
	if ((result = saddrs(appid, userid, fileid, ssaddrs)) != OK)
		return ERROR;
	std::vector<std::string> vsaddrs;
	split(ssaddrs.c_str(), vsaddrs);
	if (vsaddrs.empty()) {
		logger_error("storage addresses is empty");
		return ERROR;
	}

	result = ERROR;

	for (std::vector<std::string>::const_iterator saddr = vsaddrs.begin(); saddr != vsaddrs.end(); ++saddr) {
		// 获取存储服务器连接池
		pool_c* spool = (pool_c*)s_mngr->get(saddr->c_str());
		if (!spool) {
			// 添加服务器的客户端连接池，该函数可以在程序运行时被调用，内部自动加锁,s_scount 连接池数量限制
			s_mngr->set(saddr->c_str(), s_scount);
			// 确认是否添加到连接池 
			if (!(spool = (pool_c*)s_mngr->get(saddr->c_str()))) {
				logger_warn("storage connection pool is null, saddr: %s", saddr->c_str());
				continue;
			}
		}

		for (int sockerrs = 0; sockerrs < MAX_SOCKERRS; ++sockerrs) {
			// 获取存储服务器连接
			conn_c* sconn = (conn_c*)spool->peek();
			if (!sconn) {
				logger_warn("storage connection is null, saddr: %s", saddr->c_str());
				break;
			}

			// 向存储服务器询问文件大小
			result = sconn->filesize(appid, userid, fileid, filesize);

			if (result == SOCKET_ERROR) {
				logger_warn("get filesize fail: %s", sconn->errdesc());
				spool->put(sconn, false);
			} else {
				if (result == OK)
					// 释放一个连接至连接池中,针对该连接保持长连接
					spool->put(sconn, true);
				else {
					logger_error("get filesize fail: %s", sconn->errdesc());
					spool->put(sconn, false);
				}
				return result;
			}
		}
	}

	return result;
}

// 从存储服务器下载文件
int client_c::download(char const* appid, char const* userid, char const* fileid, long long offset, long long size,
	char** filedata, long long* filesize) {
	// 检查参数
	if (!appid || !strlen(appid)) {
		logger_error("appid is null");
		return ERROR;
	}
	if (!userid || !strlen(userid)) {
		logger_error("userid is null");
		return ERROR;
	}
	if (!fileid || !strlen(fileid)) {
		logger_error("fileid is null");
		return ERROR;
	}

	// 从跟踪服务器获取存储服务器地址列表
	int result;
	std::string ssaddrs;
	if ((result = saddrs(appid, userid, fileid, ssaddrs)) != OK)
		return ERROR;
	std::vector<std::string> vsaddrs;
	split(ssaddrs.c_str(), vsaddrs);
	if (vsaddrs.empty()) {
		logger_error("storage addresses is empty");
		return ERROR;
	}

	result = ERROR;

	for (std::vector<std::string>::const_iterator saddr = vsaddrs.begin(); saddr != vsaddrs.end(); ++saddr) {
		// 获取存储服务器连接池
		pool_c* spool = (pool_c*)s_mngr->get(saddr->c_str());
		if (!spool) {
			// 添加服务器的客户端连接池，该函数可以在程序运行时被调用，内部自动加锁,s_scount 连接池数量限制
			s_mngr->set(saddr->c_str(), s_scount);
			// 确认是否添加到连接池 
			if (!(spool = (pool_c*)s_mngr->get(saddr->c_str()))) {
				logger_warn("storage connection pool is null, saddr: %s", saddr->c_str());
				continue;
			}
		}

		for (int sockerrs = 0; sockerrs < MAX_SOCKERRS; ++sockerrs) {
			// 获取存储服务器连接
			conn_c* sconn = (conn_c*)spool->peek();
			if (!sconn) {
				logger_warn("storage connection is null, saddr: %s", saddr->c_str());
				break;
			}

			// 从存储服务器下载文件
			result = sconn->download(appid, userid, fileid, offset, size, filedata, filesize);

			if (result == SOCKET_ERROR) {
				logger_warn("download file fail: %s", sconn->errdesc());
				spool->put(sconn, false);
			} else {
				if (result == OK)
					spool->put(sconn, true);
				else {
					logger_error("download file fail: %s", sconn->errdesc());
					spool->put(sconn, false);
				}
				return result;
			}
		}
	}

	return result;
}

// 删除存储服务器上的文件
int client_c::del(char const* appid, char const* userid, char const* fileid) {
	// 检查参数
	if (!appid || !strlen(appid)) {
		logger_error("appid is null");
		return ERROR;
	}
	if (!userid || !strlen(userid)) {
		logger_error("userid is null");
		return ERROR;
	}
	if (!fileid || !strlen(fileid)) {
		logger_error("fileid is null");
		return ERROR;
	}

	// 从跟踪服务器获取存储服务器地址列表
	int result;
	std::string ssaddrs;
	if ((result = saddrs(appid, userid, fileid, ssaddrs)) != OK)
		return ERROR;
	std::vector<std::string> vsaddrs;
	split(ssaddrs.c_str(), vsaddrs);
	if (vsaddrs.empty()) {
		logger_error("storage addresses is empty");
		return ERROR;
	}

	result = ERROR;

	for (std::vector<std::string>::const_iterator saddr = vsaddrs.begin(); saddr != vsaddrs.end(); ++saddr) {
		// 获取存储服务器连接池
		pool_c* spool = (pool_c*)s_mngr->get(saddr->c_str());
		if (!spool) {
			// 添加服务器的客户端连接池，该函数可以在程序运行时被调用，内部自动加锁,s_scount 连接池数量限制
			s_mngr->set(saddr->c_str(), s_scount);
			// 确认是否添加到连接池 
			if (!(spool = (pool_c*)s_mngr->get(saddr->c_str()))) {
				logger_warn("storage connection pool is null, saddr: %s", saddr->c_str());
				continue;
			}
		}

		for (int sockerrs = 0; sockerrs < MAX_SOCKERRS; ++sockerrs) {
			// 获取存储服务器连接
			conn_c* sconn = (conn_c*)spool->peek();
			if (!sconn) {
				logger_warn("storage connection is null, saddr: %s", saddr->c_str());
				break;
			}

			// 删除存储服务器上的文件
			result = sconn->del(appid, userid, fileid);

			if (result == SOCKET_ERROR) {
				logger_warn("delete file fail: %s", sconn->errdesc());
				spool->put(sconn, false);
			} else {
				if (result == OK)
					spool->put(sconn, true);
				else {
					logger_error("delete file fail: %s", sconn->errdesc());
					spool->put(sconn, false);
				}
				return result;
			}
		}
	}

	return result;
}
