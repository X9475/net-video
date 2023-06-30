// 客户机
// 实现连接池类
#include "01_conn.h"
#include "03_pool.h"

// 构造函数
pool_c::pool_c(char const* destaddr, int count, size_t index /* = 0 */):
	// 对基类进行构造 
	connect_pool(destaddr, count, index),
	m_ctimeout(30), m_rtimeout(60), m_itimeout(90) {
}

// 设置超时
void pool_c::timeouts(int ctimeout /* = 30 */, int rtimeout /* = 60 */, int itimeout /* = 90 */) {
	m_ctimeout = ctimeout;
	m_rtimeout = rtimeout;
	m_itimeout = itimeout;
}

// 获取连接
acl::connect_client* pool_c::peek(void) {
	// 检查连接池中空闲的连接，将过期的连接释放掉
	connect_pool::check_idle(m_itimeout);
	return connect_pool::peek(); // 从连接池中尝试性获取一个连接
}

// 创建连接
acl::connect_client* pool_c::create_connect(void) {
	return new conn_c(addr_, m_ctimeout, m_rtimeout); // char addr_[256] 连接池对应的服务器地址，IP:PORT
}
