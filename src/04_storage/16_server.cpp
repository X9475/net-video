// 存储服务器
// 实现服务器类
#include <unistd.h>
#include "02_proto.h"
#include "03_util.h"
#include "01_globals.h"
#include "11_service.h"
#include "15_server.h"

// 进程切换用户后被调用
void server_c::proc_on_init(void) {
	// 隶属组名
	if (strlen(cfg_gpname) > STORAGE_GROUPNAME_MAX)
		logger_fatal("groupname too big %lu > %d", strlen(cfg_gpname), STORAGE_GROUPNAME_MAX);

	// 绑定端口号
	if (cfg_bindport <= 0)
		logger_fatal("invalid bind port %d <= 0", cfg_bindport);

	// 存储路径表
	if (!cfg_spaths || !strlen(cfg_spaths))
		logger_fatal("storage paths is null");
	split(cfg_spaths, g_spaths);
	if (g_spaths.empty())
		logger_fatal("storage paths is empty");

	// 跟踪服务器地址表
	if (!cfg_taddrs || !strlen(cfg_taddrs))
		logger_fatal("tracker addresses is null");
	split(cfg_taddrs, g_taddrs);
	if (g_taddrs.empty())
		logger_fatal("tracker addresses is empty");

	// ID服务器地址表
	if (!cfg_iaddrs || !strlen(cfg_iaddrs))
		logger_fatal("id addresses is null");
	split(cfg_iaddrs, g_iaddrs);
	if (g_iaddrs.empty())
		logger_fatal("id addresses is empty");

	// MySQL地址表
	if (!cfg_maddrs || !strlen(cfg_maddrs))
		logger_fatal("mysql addresses is null");
	split(cfg_maddrs, g_maddrs);
	if (g_maddrs.empty())
		logger_fatal("mysql addresses is empty");

	// Redis地址表
	if (!cfg_raddrs || !strlen(cfg_raddrs))
		logger_error("redis addresses is null");
	else {
		split(cfg_raddrs, g_raddrs);
		if (g_raddrs.empty())
			logger_error("redis addresses is empty");
		else {
			// 遍历Redis地址表，尝试创建连接池
			for (std::vector<std::string>::const_iterator raddr = g_raddrs.begin(); 
				raddr != g_raddrs.end(); ++raddr)
				if ((g_rconns = new acl::redis_client_pool(raddr->c_str(), cfg_maxconns))) {
					// 设置Redis连接超时和读写超时
					g_rconns->set_timeout(cfg_ctimeout, cfg_rtimeout);
					break;
				}
			if (!g_rconns)
				logger_error("create redis connection pool fail, cfg_raddrs: %s", cfg_raddrs);
		}
	}

	// 主机名
	char hostname[256+1] = {};
	if (gethostname(hostname, sizeof(hostname) - 1))
		logger_error("call gethostname fail: %s", strerror(errno));
	g_hostname = hostname;
	// 启动时间
	g_stime = time(NULL);

	// 创建并启动连接每台跟踪服务器的客户机线程
	for (std::vector<std::string>::const_iterator taddr = g_taddrs.begin(); taddr != g_taddrs.end(); ++taddr) {
		tracker_c* tracker = new tracker_c(taddr->c_str());
		tracker->set_detachable(false);
		tracker->start();
		m_trackers.push_back(tracker);
	}

	// 打印配置信息
	logger("cfg_gpname: %s, cfg_spaths: %s, cfg_taddrs: %s, cfg_iaddrs: %s, cfg_maddrs: %s, cfg_raddrs: %s, "
		"cfg_bindport: %d, cfg_interval: %d, cfg_mtimeout: %d, cfg_maxconns: %d, cfg_ctimeout: %d, cfg_rtimeout: %d, "
		"cfg_ktimeout: %d", cfg_gpname, cfg_spaths, cfg_taddrs, cfg_iaddrs, cfg_maddrs, cfg_raddrs, cfg_bindport, 
		cfg_interval, cfg_mtimeout, cfg_maxconns, cfg_ctimeout, cfg_rtimeout, cfg_ktimeout);
}

// 子进程意图退出时被调用
// 返回true，子进程立即退出，否则
// 若配置项ioctl_quick_abort非0，子进程立即退出，否则
// 待所有客户机连接都关闭后，子进程再退出
bool server_c::proc_exit_timer(size_t nclients, size_t nthreads) {
	for (std::list<tracker_c*>::iterator tracker = m_trackers.begin(); tracker != m_trackers.end(); ++tracker)
		// 终止跟踪客户机线程
		(*tracker)->stop();

	if (!nclients || !nthreads) {
		logger("nclients: %lu, nthreads: %lu", nclients, nthreads);
		return true;
	}

	return false;
}

// 进程退出前被调用
void server_c::proc_on_exit(void) {
	for (std::list<tracker_c*>::iterator tracker = m_trackers.begin(); tracker != m_trackers.end(); ++tracker) {
		// 回收跟踪客户机线程
		if (!(*tracker)->wait(NULL))
			logger_error("wait thread #%lu fail", (*tracker)->thread_id());
		// 销毁跟踪客户机线程
		delete *tracker;
	}

	m_trackers.clear();

	// 销毁Redis连接池
	if (g_rconns) {
		delete g_rconns;
		g_rconns = NULL;
	}
}

// 线程获得连接时被调用
// 返回true，连接将被用于后续通信，否则
// 函数返回后即关闭连接
bool server_c::thread_on_accept(acl::socket_stream* conn) {
	logger("connect, from: %s", conn->get_peer());
	return true;
}

// 与线程绑定的连接可读时被调用
// 返回true，保持长连接，否则
// 函数返回后即关闭连接
bool server_c::thread_on_read(acl::socket_stream* conn) {
	// 接收包头
	char head[HEADLEN];
	if (conn->read(head, HEADLEN) < 0) {
		if (conn->eof())
			logger("connection has been closed, from: %s", conn->get_peer());
		else
			logger_error("read fail: %s, from: %s", acl::last_serror(), conn->get_peer());
		return false;
	}

	// 业务处理
	service_c service;
	return service.business(conn, head);
}

// 线程读写连接超时时被调用
// 返回true，继续等待下一次读写，否则
// 函数返回后即关闭连接
bool server_c::thread_on_timeout(acl::socket_stream* conn) {
	logger("read timeout, from: %s", conn->get_peer());
	return true;
}

// 与线程绑定的连接关闭时被调用
void server_c::thread_on_close(acl::socket_stream* conn) {
	logger("client disconnect, from: %s", conn->get_peer());
}
