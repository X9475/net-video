// 存储服务器
// 实现缓存类
#include "01_types.h"
#include "01_globals.h"
#include "03_cache.h"

// 根据键获取其值
int cache_c::get(char const* key, acl::string& value) const {
	// 构造键
	acl::string storage_key;
	storage_key.format("%s:%s", STORAGE_REDIS_PREFIX, key);

	// 检查Redis连接池
	if (!g_rconns) {
		logger_warn("redis connection pool is null, key: %s", storage_key.c_str());
		return ERROR;
	}

	// 从连接池中尝试性获取一个连接，当服务器不可用、距上次服务端连接异常
	// 时间间隔未过期或连接池连接个数达到连接上限则将返回 NULL
	acl::redis_client* rconn = (acl::redis_client*)g_rconns->peek();
	if (!rconn) {
		logger_warn("peek redis connection fail, key: %s", storage_key.c_str());
		return ERROR;
	}

	// 持有此连接的Redis对象即为Redis客户机
	acl::redis redis;
	// 在使用非集群方式时，通过本函数将从连接池获得的连接对象
	redis.set_client(rconn);

	// 借助Redis客户机根据键获取其值
	if (!redis.get(storage_key.c_str(), value)) {
		logger_warn("get cache fail, key: %s", storage_key.c_str());
		// 释放一个连接至连接池中，当该连接池对应的服务器不可用或调用者希望关闭
	 	// 该连接时，则该连接将会被直接释放
		g_rconns->put(rconn, false);
		return ERROR;
	}

	// 检查空值
	if (value.empty()) {
		logger_warn("value is empty, key: %s", storage_key.c_str());
		g_rconns->put(rconn, false);
		return ERROR;
	}

	logger("get cache ok, key: %s, value: %s", storage_key.c_str(), value.c_str());
	g_rconns->put(rconn, true);

	return OK;
}

// 设置指定键的值
int cache_c::set(char const* key, char const* value, int timeout /* = -1 */) const {
	// 构造键
	acl::string storage_key;
	storage_key.format("%s:%s", STORAGE_REDIS_PREFIX, key);

	// 检查Redis连接池
	if (!g_rconns) {
		logger_warn("redis connection pool is null, key: %s", storage_key.c_str());
		return ERROR;
	}

	// 从连接池中尝试性获取一个连接，当服务器不可用、距上次服务端连接异常
	// 时间间隔未过期或连接池连接个数达到连接上限则将返回 NULL
	acl::redis_client* rconn = (acl::redis_client*)g_rconns->peek();
	if (!rconn) {
		logger_warn("peek Redis connection fail, key: %s", storage_key.c_str());
		return ERROR;
	}

	// 持有此连接的Redis对象即为Redis客户机
	acl::redis redis;
	redis.set_client(rconn);

	// 借助Redis客户机设置指定键的值
	if (timeout < 0)
		timeout = cfg_ktimeout;
	// 将值 value 关联到 key ，并将 key 的生存时间设为 timeout (以秒为单位)
	if (!redis.setex(storage_key.c_str(), value, timeout)) {
		logger_warn("set cache fail, key: %s, value: %s, timeout: %d", storage_key.c_str(), value, timeout);
		g_rconns->put(rconn, false);
		return ERROR;
	}
	logger("set cache ok, key: %s, value: %s, timeout: %d", storage_key.c_str(), value, timeout);
	// 释放一个连接至连接池中，当该连接池对应的服务器不可用或调用者希望关闭
	// 该连接时，则该连接将会被直接释放
	g_rconns->put(rconn, true);

	return OK;
}

// 删除指定键值对
int cache_c::del(char const* key) const {
	// 构造键
	acl::string storage_key;
	storage_key.format("%s:%s", STORAGE_REDIS_PREFIX, key);

	// 检查Redis连接池
	if (!g_rconns) {
		logger_warn("redis connection pool is null, key: %s", storage_key.c_str());
		return ERROR;
	}

	// 从连接池中获取一个Redis连接
	acl::redis_client* rconn = (acl::redis_client*)g_rconns->peek();
	if (!rconn) {
		logger_warn("peek Redis connection fail, key: %s", storage_key.c_str());
		return ERROR;
	}

	// 持有此连接的Redis对象即为Redis客户机
	acl::redis redis;
	redis.set_client(rconn);

	// 借助Redis客户机删除指定键值对
	// 删除一个或一组 KEY，对于变参的接口，则要求最后一个参数必须以 NULL 结束
	if (!redis.del_one(storage_key.c_str())) {
		logger_warn("delete cache fail, key: %s", storage_key.c_str());
		g_rconns->put(rconn, false);
		return ERROR;
	}
	logger("delete cache ok, key: %s", storage_key.c_str());
	// 释放一个连接至连接池中，当该连接池对应的服务器不可用或调用者希望关闭
	// 该连接时，则该连接将会被直接释放
	g_rconns->put(rconn, true);

	return OK;
}
