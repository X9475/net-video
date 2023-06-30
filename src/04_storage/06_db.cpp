// 存储服务器
// 实现数据库访问类
#include "01_types.h"
#include "01_globals.h"
#include "03_cache.h"
#include "05_db.h"

// 构造函数
db_c::db_c(void): m_mysql(mysql_init(NULL)) { // 创建MySQL对象
	if (!m_mysql)
		logger_error("create dao fail: %s", mysql_error(m_mysql));
}

// 析构函数
db_c::~db_c(void) {
	// 销毁MySQL对象
	if (m_mysql) {
		mysql_close(m_mysql);
		m_mysql = NULL;
	}
}

// 连接数据库
int db_c::connect(void) {
	MYSQL* mysql = m_mysql;

	// 遍历MySQL地址表，尝试连接数据库
	for (std::vector<std::string>::const_iterator maddr = g_maddrs.begin(); maddr != g_maddrs.end(); ++maddr)
		if ((m_mysql = mysql_real_connect(mysql, maddr->c_str(), "root", "123456", "tnv_storagedb", 0, NULL, 0)))
			return OK;

	logger_error("connect database fail: %s", mysql_error(m_mysql = mysql));
	return ERROR;
}

// 根据文件ID获取其对应的路径及大小
int db_c::get(char const* appid, char const* userid, char const* fileid, std::string& filepath, 
	long long* filesize) const {
	// 先尝试从缓存中获取与文件ID对应的路径及大小
	cache_c cache;
	acl::string key;
	key.format("uid:fid:%s:%s", userid, fileid);
	acl::string value;
	if (cache.get(key, value) == OK) {
		std::vector<acl::string> size_path = value.split2(";");
		if (size_path.size() == 2) {
			filepath = size_path[1].c_str();
			*filesize = atoll(size_path[0].c_str());
			if (!filepath.empty() && *filesize > 0) {
				logger("from cache, appid: %s, userid: %s, fileid: %s, filepath: %s, filesize: %lld",
					appid, userid, fileid, filepath.c_str(), *filesize);
				return OK;
			}
		}
	}
	// 缓存中没有再查询数据库
	std::string tablename = table_of_user(userid);
	if (tablename.empty()) {
		logger_error("tablename is empty, appid: %s, userid: %s, fileid: %s", appid, userid, fileid);
		return ERROR;
	}
	acl::string sql;
	sql.format("SELECT file_path, file_size FROM %s WHERE id='%s';", tablename.c_str(), fileid);
	if (mysql_query(m_mysql, sql.c_str())) {
		logger_error("query database fail: %s, sql: %s", mysql_error(m_mysql), sql.c_str());
		return ERROR;
	}

	// 获取查询结果
	MYSQL_RES* res = mysql_store_result(m_mysql);
	if (!res) {
		logger_error("result is null: %s, sql: %s", mysql_error(m_mysql), sql.c_str());
		return ERROR;
	}

	// 获取结果记录
	MYSQL_ROW row = mysql_fetch_row(res);
	if (!row) {
		logger_error("result is empty: %s, sql: %s", mysql_error(m_mysql), sql.c_str());
		return ERROR;
	}
	filepath = row[0];
	*filesize = atoll(row[1]);
	logger("from database, appid: %s, userid: %s, fileid: %s, filepath: %s, filesize: %lld",
		appid, userid, fileid, filepath.c_str(), *filesize);

	// 将文件ID和路径及大小的对应关系保存在缓存中，用";"分隔
	value.format("%lld;%s", *filesize, filepath.c_str());
	// 添加到缓存中
    cache.set(key, value.c_str());
	return OK;
}

// 设置文件ID和路径及大小的对应关系
int db_c::set(char const* appid, char const* userid, char const* fileid, char const* filepath, 
	long long filesize) const {
	// 根据用户ID获取其对应的表名
	std::string tablename = table_of_user(userid);
	if (tablename.empty()) {
		logger_error("tablename is empty, appid: %s, userid: %s, fileid: %s", appid, userid, fileid);
		return ERROR;
	}

	// 插入一条记录
	acl::string sql;
	sql.format("INSERT INTO %s SET id='%s', appid='%s', userid='%s', status=0, file_path='%s', file_size=%lld;",
		tablename.c_str(), fileid, appid, userid, filepath, filesize);
	if (mysql_query(m_mysql, sql.c_str())) {
		logger_error("insert database fail: %s, sql: %s", mysql_error(m_mysql), sql.c_str());
		return ERROR;
	}

	// 检查插入结果
	MYSQL_RES* res = mysql_store_result(m_mysql);
	// mysqli_field_count() 函数返回最近查询的列数
	if (!res && mysql_field_count(m_mysql)) {
		logger_error("insert database fail: %s, sql: %s", mysql_error(m_mysql), sql.c_str());
		return ERROR;
	}

	return OK;
}

// 删除文件ID
int db_c::del(char const* appid, char const*userid, char const* fileid) const {
	// 先从缓存中删除文件ID
	cache_c cache;
	acl::string key;
	key.format("uid:fid:%s:%s", userid, fileid);
	if (cache.del(key) != OK)
		logger_warn("delete cache fail: appid: %s, userid: %s, fileid: %s", appid, userid, fileid);

	// 再从数据库中删除文件ID
	std::string tablename = table_of_user(userid);
	if (tablename.empty()) {
		logger_error("tablename is empty, appid: %s, userid: %s, fileid: %s", appid, userid, fileid);
		return ERROR;
	}
	acl::string sql;
	sql.format("DELETE FROM %s WHERE id='%s';", tablename.c_str(), fileid);
	if (mysql_query(m_mysql, sql.c_str())) {
		logger_error("delete database fail: %s, sql: %s", mysql_error(m_mysql), sql.c_str());
		return ERROR;
	}

	// 检查删除结果
	MYSQL_RES* res = mysql_store_result(m_mysql);
	if (!res && mysql_field_count(m_mysql)) {
		logger_error("delete database fail: %s, sql: %s", mysql_error(m_mysql), sql.c_str());
		return ERROR;
	}

	return OK;
}

// 根据用户ID获取其对应的表名
std::string db_c::table_of_user(char const* userid) const {
	char tablename[10];

	sprintf(tablename, "t_file_%02d", (hash(userid, strlen(userid)) & 0x7FFFFFFF) % 3 + 1);

	return tablename;
}

// 计算哈希值(由用户ID产生)
unsigned int db_c::hash(char const* buf, size_t len) const {
	unsigned int h = 0;

	for (size_t i = 0; i < len; ++i)
		h ^= i&1 ? ~(h<<11^buf[i]^h>>5) : h<<7^buf[i]^h>>3;

	return h;
}
