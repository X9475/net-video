// ID服务器
// 实现业务服务类
#include "02_proto.h"
#include "03_util.h"
#include "01_globals.h"
#include "03_db.h"
#include "05_service.h"

// 业务处理
bool service_c::business(acl::socket_stream* conn, char const* head) const {
	// |包体长度|命令|状态|  包体 |
	// |   8  |  1 | 1 |包体长度|
	// 解析包头
	long long bodylen = ntoll(head); // 包体长度
	if (bodylen < 0) {
		error(conn, -1, "invalid body length: %lld < 0", bodylen);
		return false;
	}
	int command = head[BODYLEN_SIZE]; // 命令
	int status = head[BODYLEN_SIZE+COMMAND_SIZE]; // 状态
	logger("bodylen: %lld, command: %d, status: %d", bodylen, command, status);

	bool result;

	// 根据命令执行具体业务处理
	switch (command) {
		case CMD_ID_GET:
			// 处理来自存储服务器的获取ID请求
			result = get(conn, bodylen);
			break;

		default:
			error(conn, -1, "unknown command: %d", command);
			return false;
	}

	return result;
}

/*********************************************************************************/

// 处理来自存储服务器的获取ID请求
bool service_c::get(acl::socket_stream* conn, long long bodylen) const {
	// |包体长度 | 命令| 状态 | ID键 |
	// |    8   |  1 |  1   |64+1 |
	// 检查包体长度
	long long expected = ID_KEY_MAX + 1; // 期望包体长度
	if (bodylen > expected) {
		error(conn, -1, "invalid body length: %lld > %lld", bodylen, expected);
		return false;
	}

	// 接收包体
	char body[bodylen];
	if (conn->read(body, bodylen) < 0) {
		logger_error("read fail: %s, bodylen: %lld, from: %s", acl::last_serror(), bodylen, conn->get_peer());
		return false;
	}

	// 根据ID的键获取其值
	long value = get(body);
	if (value < 0) {
		error(conn, -1, "get id fail, key: %s", body);
		return false;
	}

	logger("get id ok, key: %s, value: %ld", body, value);

	return id(conn, value);
}

// 根据ID的键获取其值
long service_c::get(char const* key) const {
	// 互斥锁加锁
	if ((errno = pthread_mutex_lock(&g_mutex))) {
		logger_error("call pthread_mutex_lock fail: %s", strerror(errno));
		return -1;
	}

	long value = -1;

	// 在ID表中查找ID
	std::vector<id_pair_t>::iterator id;
	for (id = g_ids.begin(); id != g_ids.end(); ++id)
		if (!strcmp(id->id_key, key))
			break;
	if (id != g_ids.end()) { // 找到该ID
		if (id->id_offset < cfg_maxoffset) { // 该ID的偏移未及上限
			value = id->id_value + id->id_offset;
			++id->id_offset;
		} else if ((value = fromdb(key)) >= 0) { // 从数据库中获取ID值
			// 更新ID表中的ID
			id->id_value = value;
			id->id_offset = 1;
		}
	} else if ((value = fromdb(key)) >= 0) { // 从数据库中获取ID值
		// 在ID表中添加ID
		id_pair_t id;
		strcpy(id.id_key, key);
		id.id_value = value;
		id.id_offset = 1;
		g_ids.push_back(id);
	}

	// 互斥锁解锁
	if ((errno = pthread_mutex_unlock(&g_mutex))) {
		logger_error("call pthread_mutex_unlock fail: %s", strerror(errno));
		return -1;
	}

	return value;
}

// 从数据库中获取ID值
long service_c::fromdb(char const* key) const {
	db_c db; // 数据库访问对象

	// 连接数据库
	if (db.connect() != OK)
		return -1;

	long value = -1;

	// 获取ID当前值，同时产生下一个值
	if (db.get(key, cfg_maxoffset, &value) != OK)
		return -1;

	return value;
}

/*********************************************************************************/

// 应答ID
bool service_c::id(acl::socket_stream* conn, long value) const {
	// |包体长度|命令|状态 |ID值|
	// |   8  |  1 |  1 |  8 |
	// 构造响应
	long long bodylen = BODYLEN_SIZE;
	long long resplen = HEADLEN + bodylen;
	char resp[resplen] = {};
	llton(bodylen, resp);
	resp[BODYLEN_SIZE] = CMD_ID_REPLY;
	resp[BODYLEN_SIZE+COMMAND_SIZE] = 0;
	lton(value, resp + HEADLEN);

	// 发送响应
	if (conn->write(resp, resplen) < 0) {
		logger_error("write fail: %s, resplen: %lld, to: %s", acl::last_serror(), resplen, conn->get_peer());
		return false;
	}

	return true;
}

// 应答错误
bool service_c::error(acl::socket_stream* conn, short errnumb, char const* format, ...) const {
	// 错误描述
	char errdesc[ERROR_DESC_SIZE];
	va_list ap;
	va_start(ap, format);
	vsnprintf(errdesc, ERROR_DESC_SIZE, format, ap);
	va_end(ap);
	logger_error("%s", errdesc);
	acl::string desc;
	desc.format("[%s] %s", g_hostname.c_str(), errdesc);
	memset(errdesc, 0, sizeof(errdesc));
	strncpy(errdesc, desc.c_str(), ERROR_DESC_SIZE - 1);
	size_t desclen = strlen(errdesc);
	desclen += desclen != 0;

	// |包体长度|命令|状态|错误号|错误描述|
	// |  8  |  1 |  1 |  2  | <=1024 |
	// 构造响应
	long long bodylen = ERROR_NUMB_SIZE + desclen;
	long long resplen = HEADLEN + bodylen;
	char resp[resplen] = {};
	llton(bodylen, resp);
	resp[BODYLEN_SIZE] = CMD_ID_REPLY;
	resp[BODYLEN_SIZE+COMMAND_SIZE] = STATUS_ERROR;
	ston(errnumb, resp + HEADLEN);
	if (desclen)
		strcpy(resp + HEADLEN + ERROR_NUMB_SIZE, errdesc);

	// 发送响应
	if (conn->write(resp, resplen) < 0) {
		logger_error("write fail: %s, resplen: %lld, to: %s", acl::last_serror(), resplen, conn->get_peer());
		return false;
	}

	return true;
}
