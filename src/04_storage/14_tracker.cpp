// 存储服务器
// 实现跟踪客户机线程类
#include <unistd.h>
#include "02_proto.h"
#include "03_util.h"
#include "01_globals.h"
#include "13_tracker.h"

// 构造函数
tracker_c::tracker_c(char const* taddr): m_stop(false), m_taddr(taddr) {
}

// 终止线程
void tracker_c::stop(void) {
	m_stop = true;
}

// 线程过程
void* tracker_c::run(void) {
	acl::socket_stream conn;

	while (!m_stop) {
		// 连接远程服务器并打开网络连接流
		if (!conn.open(m_taddr, 10, 30)) {
			logger_error("connect tracker fail, taddr: %s", m_taddr.c_str());
			sleep(2);
			continue; // 失败重连
		}

		// 向跟踪服务器发送加入包
		if (join(&conn) != OK) {
			conn.close();
			sleep(2);
			continue; // 失败重连
		}

		time_t last = time(NULL); // 上次心跳
		while (!m_stop) {
			time_t now = time(NULL); // 现在
			// 现在离上次心跳已足够久，再跳一次
			if (now - last >= cfg_interval) {
				// 向跟踪服务器发送心跳包
				if (beat(&conn) != OK)
					break; // 失败重连
				last = now;
			}
			sleep(1);
		}
		conn.close();
	}

	return NULL;
}

// 向跟踪服务器发送加入包
int tracker_c::join(acl::socket_stream* conn) const {
	// |包体长度 |命令 |状态 |storage_join_body_t|
	// |    8   |  1 |  1 |     包体长度       |
	// 构造请求
	long long bodylen = sizeof(storage_join_body_t);
	long long requlen = HEADLEN + bodylen;
	char requ[requlen] = {};
	llton(bodylen, requ);
	requ[BODYLEN_SIZE] = CMD_TRACKER_JOIN;
	requ[BODYLEN_SIZE+COMMAND_SIZE] = 0;
	storage_join_body_t* sjb = (storage_join_body_t*)(requ + HEADLEN);
	strcpy(sjb->sjb_version, g_version);           // 版本
	strcpy(sjb->sjb_groupname, cfg_gpname);        // 组名
	strcpy(sjb->sjb_hostname, g_hostname.c_str()); // 主机名
	ston(cfg_bindport, sjb->sjb_port);             // 端口号
	lton(g_stime, sjb->sjb_stime);                 // 启动时间
	lton(time(NULL), sjb->sjb_jtime);              // 加入时间

	// 发送请求
	if (conn->write(requ, requlen) < 0) {
		logger_error("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, conn->get_peer());
		return SOCKET_ERROR;
	}

	// 接收包头
	char head[HEADLEN];
	if (conn->read(head, HEADLEN) < 0) {
		logger_error("read fail: %s, from: %s", acl::last_serror(), conn->get_peer());
		return SOCKET_ERROR;
	}

	// |包体长度 |命令 |状态 |
	// |    8   |  1 |  1 |
	// 解析包头
	if ((bodylen = ntoll(head)) < 0) { // 包体长度
		logger_error("invalid body length: %lld < 0", bodylen);
		return ERROR;
	}
	int command = head[BODYLEN_SIZE]; // 命令
	int status = head[BODYLEN_SIZE+COMMAND_SIZE]; // 状态
	logger("bodylen: %lld, command: %d, status: %d", bodylen, command, status);

	// 检查命令
	if (command != CMD_TRACKER_REPLY) {
		logger_error("unknown command: %d", command);
		return ERROR;
	}

	// 应答成功
	if (!status) 
		return OK;

	// |包体长度 |命令 |状态 |错误号 |错误描述 |
	// |    8   |  1 |  1 |   2  | <=1024 |
	// 检查包体长度
	long long expected = ERROR_NUMB_SIZE + ERROR_DESC_SIZE;
	if (bodylen > expected) {
		logger_error("invalid body length: %lld > %lld", bodylen, expected);
		return ERROR;
	}

	// 接收包体
	char body[bodylen];
	if (conn->read(body, bodylen) < 0) {
		logger_error("read fail: %s, bodylen: %lld, from: %s", acl::last_serror(), bodylen, conn->get_peer());
		return SOCKET_ERROR;
	}

	// 解析包体
	short errnumb = ntos(body);
	char const* errdesc = "";
	if (bodylen > ERROR_NUMB_SIZE)
		errdesc = body + ERROR_NUMB_SIZE;
		
	logger_error("join fail, errnumb: %d, errdesc: %s", errnumb, errdesc);
	return ERROR;
}

// 向跟踪服务器发送心跳包
int tracker_c::beat(acl::socket_stream* conn) const {
	// |包体长度 |命令 |状态 |storage_beat_body_t|
	// |    8   |  1 |  1 |     包体长度       |
	// 构造请求
	long long bodylen = sizeof(storage_beat_body_t);
	long long requlen = HEADLEN + bodylen;
	char requ[requlen] = {};
	llton(bodylen, requ);
	requ[BODYLEN_SIZE] = CMD_TRACKER_BEAT;
	requ[BODYLEN_SIZE+COMMAND_SIZE] = 0;
	storage_beat_body_t* sbb = (storage_beat_body_t*)(requ + HEADLEN);
	strcpy(sbb->sbb_groupname, cfg_gpname);        // 组名
	strcpy(sbb->sbb_hostname, g_hostname.c_str()); // 主机名

	// 发送请求
	if (conn->write(requ, requlen) < 0) {
		logger_error("write fail: %s, requlen: %lld, to: %s", acl::last_serror(), requlen, conn->get_peer());
		return SOCKET_ERROR;
	}

	// 接收包头
	char head[HEADLEN];
	if (conn->read(head, HEADLEN) < 0) {
		logger_error("read fail: %s, from: %s", acl::last_serror(), conn->get_peer());
		return SOCKET_ERROR;
	}

	// |包体长度 |命令 |状态 |
	// |    8   |  1 |  1 |
	// 解析包头
	if ((bodylen = ntoll(head)) < 0) { // 包体长度
		logger_error("invalid body length: %lld < 0", bodylen);
		return ERROR;
	}
	int command = head[BODYLEN_SIZE]; // 命令
	int status = head[BODYLEN_SIZE+COMMAND_SIZE]; // 状态
	logger("bodylen: %lld, command: %d, status: %d", bodylen, command, status);

	// 检查命令
	if (command != CMD_TRACKER_REPLY) {
		logger_error("unknown command: %d", command);
		return ERROR;
	}

	// 应答成功
	if (!status) 
		return OK;

	// |包体长度 |命令 |状态 |错误号 |错误描述 |
	// |    8   |  1 |  1 |   2  | <=1024 |
	// 检查包体长度
	long long expected = ERROR_NUMB_SIZE + ERROR_DESC_SIZE;
	if (bodylen > expected) {
		logger_error("invalid body length: %lld > %lld", bodylen, expected);
		return ERROR;
	}

	// 接收包体
	char body[bodylen];
	if (conn->read(body, bodylen) < 0) {
		logger_error("read fail: %s, bodylen: %lld, from: %s", acl::last_serror(), bodylen, conn->get_peer());
		return SOCKET_ERROR;
	}

	// 解析包体
	short errnumb = ntos(body);
	char const* errdesc = "";
	if (bodylen > ERROR_NUMB_SIZE)
		errdesc = body + ERROR_NUMB_SIZE;

	logger_error("beat fail, errnumb: %d, errdesc: %s", errnumb, errdesc);
	return ERROR;
}
