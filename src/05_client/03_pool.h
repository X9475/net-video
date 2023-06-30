// 客户机
// 声明连接池类
#pragma once
#include <lib_acl.hpp>

// 连接池类
class pool_c: public acl::connect_pool {
public:
	// 构造函数
	pool_c(char const* destaddr, int count, size_t index = 0);

	// 设置超时
	void timeouts(int ctimeout = 30, int rtimeout = 60, int itimeout = 90);
	// 获取连接
	acl::connect_client* peek(void);

protected:
	// 创建连接(纯虚函数，子类实现)
	acl::connect_client* create_connect(void);

private:
	int m_ctimeout; // 连接超时
	int m_rtimeout; // 读写超时
	int m_itimeout; // 空闲超时
};
