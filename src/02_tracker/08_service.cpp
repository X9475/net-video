// 跟踪服务器
// 处理业务类 
#include <algorithm>
#include "02_proto.h"
#include "03_util.h"
#include "01_globals.h"
#include "05_db.h"
#include "07_service.h"

// 业务处理
bool service_c::business(acl::socket_stream* conn, char const* head) const {
	// |包体长度|命令|状态|  包体  |
	// |   8  |  1 |  1 |包体长度|
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
		case CMD_TRACKER_JOIN:
			// 处理来自存储服务器的加入包
			result = join(conn, bodylen);
			break;
		case CMD_TRACKER_BEAT:
			// 处理来自存储服务器的心跳包
			result = beat(conn, bodylen);
			break;
		case CMD_TRACKER_SADDRS:
			// 处理来自客户机的获取存储服务器地址列表请求
			result = saddrs(conn, bodylen);
			break;
		case CMD_TRACKER_GROUPS:
			// 处理来自客户机的获取组列表请求
			result = groups(conn);
			break;
		default:
			error(conn, -1, "unknown command: %d", command);
			return false;
	}

	return result;
}

/*********************************************************************************/

// 处理来自存储服务器的加入包
bool service_c::join(acl::socket_stream* conn, long long bodylen) const {
	// |包体长度|命令|状态|storage_join_body_t|
	// |   8  |  1 |  1 |      包体长度      |
	// 检查包体长度
	long long expected = sizeof(storage_join_body_t); // 期望包体长度
	if (bodylen != expected) {
		error(conn, -1, "invalid body length: %lld != %lld", bodylen, expected);
		return false;
	}
	// 接收包体
	char body[bodylen];
	if (conn->read(body, bodylen) < 0) {
		logger_error("read fail: %s, bodylen: %lld, from: %s", acl::last_serror(), bodylen, conn->get_peer());
		return false;
	}
	// 解析包体
	storage_join_t sj;
	storage_join_body_t* sjb = (storage_join_body_t*)body;
	// 版本
	strcpy(sj.sj_version, sjb->sjb_version);
	// 组名
	strcpy(sj.sj_groupname, sjb->sjb_groupname);
	if (valid(sj.sj_groupname) != OK) {
		error(conn, -1, "invalid groupname: %s", sj.sj_groupname);
		return false;
	}
	// 主机名
	strcpy(sj.sj_hostname, sjb->sjb_hostname);
	// 端口号
	sj.sj_port = ntos(sjb->sjb_port);
	if (!sj.sj_port) {
		error(conn, -1, "invalid port: %u", sj.sj_port);
		return false;
	}
	// 启动时间
	sj.sj_stime = ntol(sjb->sjb_stime);
	// 加入时间
	sj.sj_jtime = ntol(sjb->sjb_jtime);
	logger("storage join, version: %s, groupname: %s, hostname: %s, port: %u, stime: %s, jtime: %s",
		sj.sj_version, sj.sj_groupname, sj.sj_hostname, sj.sj_port, std::string(ctime(&sj.sj_stime)).c_str(),
		std::string(ctime(&sj.sj_jtime)).c_str());

	// 将存储服务器加入组表
	if (join(&sj, conn->get_peer()) != OK) {
		error(conn, -1, "join into groups fail");
		return false;
	}

	return ok(conn);
}

// 处理来自存储服务器的心跳包
bool service_c::beat(acl::socket_stream* conn, long long bodylen) const {
	// |包体长度|命令|状态|storage_beat_body_t|
	// |    8   |  1 |  1 |     包体长度      |
	// 检查包体长度
	long long expected = sizeof(storage_beat_body_t); // 期望包体长度
	if (bodylen != expected) {
		error(conn, -1, "invalid body length: %lld != %lld", bodylen, expected);
		return false;
	}
	// 接收包体
	char body[bodylen];
	if (conn->read(body, bodylen) < 0) {
		logger_error("read fail: %s, bodylen: %lld, from: %s", acl::last_serror(), bodylen, conn->get_peer());
		return false;
	}
	// 解析包体
	storage_beat_body_t* sbb = (storage_beat_body_t*)body;
	// 组名
	char groupname[STORAGE_GROUPNAME_MAX+1];
	strcpy(groupname, sbb->sbb_groupname);
	// 主机名
	char hostname[STORAGE_HOSTNAME_MAX+1];
	strcpy(hostname, sbb->sbb_hostname);
	logger("storage beat, groupname: %s, hostname: %s", groupname, hostname);

	// 将存储服务器标为活动
	if (beat(groupname, hostname, conn->get_peer()) != OK) {
		error(conn, -1, "mark storage as active fail");
		return false;
	}

	return ok(conn);
}

// 处理来自客户机的获取存储服务器地址列表请求
bool service_c::saddrs(acl::socket_stream* conn, long long bodylen) const {
	// |包体长度|命令|状态|应用ID|用户ID|文件ID|
	// |    8   |  1 |  1 |  16  |  256 |  128 |
	// 检查包体长度
	long long expected = APPID_SIZE + USERID_SIZE + FILEID_SIZE;
	if (bodylen != expected) {
		error(conn, -1, "invalid body length: %lld != %lld", bodylen, expected);
		return false;
	}
	// 接收包体
	char body[bodylen];
	if (conn->read(body, bodylen) < 0) {
		logger_error("read fail: %s, bodylen: %lld, from: %s", acl::last_serror(), bodylen, conn->get_peer());
		return false;
	}
	// 解析包体
	char appid[APPID_SIZE];
	strcpy(appid, body);
	char userid[USERID_SIZE];
	strcpy(userid, body + APPID_SIZE);
	char fileid[FILEID_SIZE];
	strcpy(fileid, body + APPID_SIZE + USERID_SIZE);

	// 响应客户机存储服务器地址列表
	if (saddrs(conn, appid, userid) != OK)
		return false;
	return true;
}

// 处理来自客户机的获取组列表请求
bool service_c::groups(acl::socket_stream* conn) const { // 互斥锁加锁
	if ((errno = pthread_mutex_lock(&g_mutex))) {
		logger_error("call pthread_mutex_lock fail: %s", strerror(errno));
		return false;
	}

	acl::string gps; // 全组字符串
	gps.format("         COUNT OF GROUPS: %lu\n", g_groups.size());

	// 遍历组表中的每一个组
	for (std::map<std::string, std::list<storage_info_t> >::const_iterator group = g_groups.begin(); 
		group != g_groups.end(); ++group) {
		acl::string grp; // 单组字符串
		grp.format("               GROUPNAME: %s\n"
			"       COUNT OF STORAGES: %lu\n"
			"COUNT OF ACTIVE STORAGES: %s\n",
			group->first.c_str(),
			group->second.size(),
			"%d");

		int act = 0; // 活动存储服务器数

		// 遍历该组中的每一台存储服务器
		for (std::list<storage_info_t>::const_iterator si = group->second.begin(); si != group->second.end(); ++si) {
			acl::string stg; // 存储服务器字符串
			stg.format("                 VERSION: %s\n"
				"                HOSTNAME: %s\n"
				"                 ADDRESS: %s:%u\n"
				"            STARTUP TIME: %s"
				"               JOIN TIME: %s"
				"               BEAT TIME: %s"
				"                  STATUS: ",
				si->si_version,
				si->si_hostname,
				si->si_addr, si->si_port,
				std::string(ctime(&si->si_stime)).c_str(),
				std::string(ctime(&si->si_jtime)).c_str(),
				std::string(ctime(&si->si_btime)).c_str());

			switch (si->si_status) {
				case STORAGE_STATUS_OFFLINE:
					stg += "OFFLINE";
					break;
				case STORAGE_STATUS_ONLINE:
					stg += "ONLINE";
					break;
				case STORAGE_STATUS_ACTIVE:
					stg += "ACTIVE";
					++act;
					break;
				default:
					stg += "UNKNOWN";
					break;
			}
			grp += stg + "\n";
		}
		gps += grp.format(grp, act);
	}
	gps = gps.left(gps.size() - 1);

	// 互斥锁解锁
	if ((errno = pthread_mutex_unlock(&g_mutex))) {
		logger_error("call pthread_mutex_unlock fail: %s", strerror(errno));
		return false;
	}

	// |包体长度|命令|状态| 组列表 |
	// |    8   |  1 |  1 |包体长度|
	// 构造响应
	long long bodylen = gps.size() + 1;
	long long resplen = HEADLEN + bodylen;
	char resp[resplen] = {};
	llton(bodylen, resp);
	resp[BODYLEN_SIZE] = CMD_TRACKER_REPLY;
	resp[BODYLEN_SIZE+COMMAND_SIZE] = 0;
	strcpy(resp + HEADLEN, gps.c_str());

	// 发送响应
	if (conn->write(resp, resplen) < 0) {
		logger_error("write fail: %s, resplen: %lld, to: %s", acl::last_serror(), resplen, conn->get_peer());
		return false;
	}

	return true;
}

/*********************************************************************************/

// 将存储服务器加入组表
int service_c::join(storage_join_t const* sj, char const* saddr) const {
	// 互斥锁加锁
	if ((errno = pthread_mutex_lock(&g_mutex))) {
		logger_error("call pthread_mutex_lock fail: %s", strerror(errno));
		return ERROR;
	}

	// 在组表中查找待加入存储服务器所隶属的组
	std::map<std::string, std::list<storage_info_t> >::iterator group = g_groups.find(sj->sj_groupname);
	if (group != g_groups.end()) { // 若找到该组
		// 遍历该组的存储服务器列表
		std::list<storage_info_t>::iterator si;
		for (si = group->second.begin(); si != group->second.end(); ++si)
			// 若待加入存储服务器已在该列表中
			if (!strcmp(si->si_hostname, sj->sj_hostname) && !strcmp(si->si_addr, saddr)) {
				// 更新该列表中的相应记录
				strcpy(si->si_version, sj->sj_version); // 版本
				si->si_port   = sj->sj_port;            // 端口号
				si->si_stime  = sj->sj_stime;           // 启动时间
				si->si_jtime  = sj->sj_jtime;           // 加入时间
				si->si_btime  = sj->sj_jtime;           // 心跳时间
				si->si_status = STORAGE_STATUS_ONLINE;  // 状态
				break;
			}
		// 若待加入存储服务器不在该列表中
		if (si == group->second.end()) {
			// 将待加入存储服务器加入该列表,storage_info_t
			storage_info_t si;
			strcpy(si.si_version,  sj->sj_version);  // 版本
			strcpy(si.si_hostname, sj->sj_hostname); // 主机名
			strcpy(si.si_addr,     saddr);           // IP地址
			si.si_port   = sj->sj_port;              // 端口号
			si.si_stime  = sj->sj_stime;             // 启动时间
			si.si_jtime  = sj->sj_jtime;             // 加入时间
			si.si_btime  = sj->sj_jtime;             // 心跳时间
			si.si_status = STORAGE_STATUS_ONLINE;    // 状态
			group->second.push_back(si);
		}
	}
	else { // 若没有该组
		// 将待加入存储服务器所隶属的组加入组表
		g_groups[sj->sj_groupname] = std::list<storage_info_t>();
		// 将待加入存储服务器加入该组的存储服务器列表
		storage_info_t si;
		strcpy(si.si_version,  sj->sj_version);  // 版本
		strcpy(si.si_hostname, sj->sj_hostname); // 主机名
		strcpy(si.si_addr,     saddr);           // IP地址
		si.si_port   = sj->sj_port;              // 端口号
		si.si_stime  = sj->sj_stime;             // 启动时间
		si.si_jtime  = sj->sj_jtime;             // 加入时间
		si.si_btime  = sj->sj_jtime;             // 心跳时间
		si.si_status = STORAGE_STATUS_ONLINE;    // 状态
		g_groups[sj->sj_groupname].push_back(si);
	}

	// 互斥锁解锁
	if ((errno = pthread_mutex_unlock(&g_mutex))) {
		logger_error("call pthread_mutex_unlock fail: %s", strerror(errno));
		return ERROR;
	}

	return OK;
}

// 将存储服务器标为活动
int service_c::beat(char const* groupname, char const* hostname, char const* saddr) const {
	// 互斥锁加锁
	if ((errno = pthread_mutex_lock(&g_mutex))) {
		logger_error("call pthread_mutex_lock fail: %s", strerror(errno));
		return ERROR;
	}
	int result = OK;

	// 在组表中查找待标记存储服务器所隶属的组
	std::map<std::string, std::list<storage_info_t> >::iterator group = g_groups.find(groupname);
	if (group != g_groups.end()) { // 若找到该组
		// 遍历该组的存储服务器列表
		std::list<storage_info_t>::iterator si;
		for (si = group->second.begin(); si != group->second.end(); ++si)
			// 若待标记存储服务器已在该列表中
			if (!strcmp(si->si_hostname, hostname) && !strcmp(si->si_addr, saddr)) {
				// 更新该列表中的相应记录
				si->si_btime  = time(NULL);            // 心跳时间
				si->si_status = STORAGE_STATUS_ACTIVE; // 状态
				break;
			}
		// 若待标记存储服务器不在该列表中
		if (si == group->second.end()) {
			logger_error("storage not found, groupname: %s, hostname: %s, saddr: %s", groupname, hostname, saddr);
			result = ERROR;
		}
	}
	else { // 若没有该组
		logger_error("group not found, groupname: %s", groupname);
		result = ERROR;
	}

	// 互斥锁解锁
	if ((errno = pthread_mutex_unlock(&g_mutex))) {
		logger_error("call pthread_mutex_unlock fail: %s", strerror(errno));
		return ERROR;
	}

	return result;
}

// 响应客户机存储服务器地址列表
int service_c::saddrs(acl::socket_stream* conn, char const* appid, char const* userid) const {
	// 应用ID是否合法
	if (valid(appid) != OK) {
		error(conn, -1, "invalid appid: %s", appid);
		return ERROR;
	}

	// 应用ID是否存在
	if (std::find(g_appids.begin(), g_appids.end(), appid) == g_appids.end()) {
		error(conn, -1, "unknown appid: %s", appid);
		return ERROR;
	}

	// 根据用户ID获取其对应的组名
	std::string groupname;
	if (group_of_user(appid, userid, groupname) != OK) {
		error(conn, -1, "get groupname fail");
		return ERROR;
	}

	// 根据组名获取存储服务器地址列表
	std::string saddrs;
	if (saddrs_of_group(groupname.c_str(), saddrs) != OK) {
		error(conn, -1, "get storage address fail");
		return ERROR;
	}

	logger("appid: %s, userid: %s, groupname: %s, saddrs: %s", appid, userid, groupname.c_str(), saddrs.c_str());

	// |包体长度|命令|状态|组名|存储服务器地址列表|
	// |    8 |  1 | 1 |      包体长度       |
	// 构造响应
	long long bodylen = STORAGE_GROUPNAME_MAX + 1 + saddrs.size() + 1;
	long long resplen = HEADLEN + bodylen;
	char resp[resplen] = {};
	llton(bodylen, resp);
	resp[BODYLEN_SIZE] = CMD_TRACKER_REPLY;
	resp[BODYLEN_SIZE+COMMAND_SIZE] = 0;
	strncpy(resp + HEADLEN, groupname.c_str(), STORAGE_GROUPNAME_MAX);
	strcpy(resp + HEADLEN + STORAGE_GROUPNAME_MAX + 1, saddrs.c_str());

	// 发送响应
	if (conn->write(resp, resplen) < 0) {
		logger_error("write fail: %s, resplen: %lld, to: %s", acl::last_serror(), resplen, conn->get_peer());
		return ERROR;
	}

	return OK;
}

// 根据用户ID获取其对应的组名
int service_c::group_of_user(char const* appid, char const* userid, std::string& groupname) const {
	db_c db; // 数据库访问对象

	// 连接数据库
	if (db.connect() != OK)
		return ERROR;

	// 根据用户ID获取其对应的组名
	if (db.get(userid, groupname) != OK)
		return ERROR;

	// 组名为空表示该用户没有组，为其随机分配一个
	if (groupname.empty()) {
		logger("groupname is empty, appid: %s, userid: %s, allocate one", appid, userid);

		// 获取全部组名
		std::vector<std::string> groupnames;
		if (db.get(groupnames) != OK)
			return ERROR;
		if (groupnames.empty()) {
			logger_error("groupnames is empty, appid: %s, userid: %s", appid, userid);
			return ERROR;
		}

		// 随机抽取组名
		srand(time(NULL));
		groupname = groupnames[rand() % groupnames.size()];

		// 设置用户ID和组名的对应关系
		if (db.set(appid, userid, groupname.c_str()) != OK)
			return ERROR;
	}

	return OK;
}

// 根据组名获取存储服务器地址列表
int service_c::saddrs_of_group(char const* groupname, std::string& saddrs) const {
	// 互斥锁加锁
	if ((errno = pthread_mutex_lock(&g_mutex))) {
		logger_error("call pthread_mutex_lock fail: %s", strerror(errno));
		return ERROR;
	}
	int result = OK;

	// 根据组名在组表中查找特定组
	std::map<std::string, std::list<storage_info_t> >::const_iterator group = g_groups.find(groupname);
	if (group != g_groups.end()) { // 若找到该组
		if (!group->second.empty()) { // 若该组的存储服务器列表非空
			// 在该组的存储服务器列表中，从随机位置开始最多抽取三台处于活动状态的存储服务器
			srand(time(NULL));
			int nsis = group->second.size();
			int nrand = rand() % nsis;
			std::list<storage_info_t>::const_iterator si = group->second.begin();
			int nacts = 0;	//拿到的数量
			for (int i = 0; i < nsis + nrand; ++i, ++si) {
				if (si == group->second.end())
					si = group->second.begin();
				logger("i: %d, nrand: %d, addr: %s, port: %u, status: %d", i, nrand, si->si_addr, si->si_port, si->si_status);
				if (i >= nrand && si->si_status == STORAGE_STATUS_ACTIVE) {
					char saddr[256];
					sprintf(saddr, "%s:%d", si->si_addr, si->si_port);
					saddrs += saddr;
					saddrs += ";";
					if (++nacts >= 3) //拿到三个处于活动的存储服务器地址
						break;
				}
			}
			if (!nacts) { // 若没有处于活动状态的存储服务器
				logger_error("no active storage in group %s", groupname);
				result = ERROR;
			}
		}
		else { // 若该组的存储服务器列表为空
			logger_error("no storage in group %s", groupname);
			result = ERROR;
		}
	}
	else { // 若没有该组
		logger_error("not found group %s", groupname);
		result = ERROR;
	}

	// 互斥锁解锁
	if ((errno = pthread_mutex_unlock(&g_mutex))) {
		logger_error("call pthread_mutex_unlock fail: %s", strerror(errno));
		return ERROR;
	}

	return result;
}

/*********************************************************************************/

// 应答成功
bool service_c::ok(acl::socket_stream* conn) const {
	// |包体长度|命令|状态|
	// |  8  |  1 |  1 |
	// 构造响应
	long long bodylen = 0;
	long long resplen = HEADLEN + bodylen;
	char resp[resplen] = {};
	llton(bodylen, resp);
	resp[BODYLEN_SIZE] = CMD_TRACKER_REPLY;
	resp[BODYLEN_SIZE+COMMAND_SIZE] = 0;

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
	// |  8   |  1 |  1 | 2 | <=1024 |
	// 构造响应
	long long bodylen = ERROR_NUMB_SIZE + desclen;
	long long resplen = HEADLEN + bodylen;
	char resp[resplen] = {};
	llton(bodylen, resp);
	resp[BODYLEN_SIZE] = CMD_TRACKER_REPLY;
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
