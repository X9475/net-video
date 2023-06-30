// ID服务器
// 声明服务器类
#pragma once
#include <lib_acl.hpp>

// 服务器类
class server_c: public acl::master_threads {
protected:
	// 进程切换用户后被调用
	void proc_on_init(void);
	// 子进程意图退出时被调用
	// 返回true，子进程立即退出，否则
	// 若配置项ioctl_quick_abort非0，子进程立即退出，否则
	// 待所有客户机连接都关闭后，子进程再退出
	bool proc_exit_timer(size_t nclients, size_t nthreads);

	// 线程获得连接时被调用
	// 返回true，连接将被用于后续通信，否则
	// 函数返回后即关闭连接
	bool thread_on_accept(acl::socket_stream* conn);
	// 与线程绑定的连接可读时被调用
	// 返回true，保持长连接，否则
	// 函数返回后即关闭连接

	bool thread_on_read(acl::socket_stream* conn);
	// 线程读写连接超时时被调用
	// 返回true，继续等待下一次读写，否则
	// 函数返回后即关闭连接
	
	bool thread_on_timeout(acl::socket_stream* conn);
	// 与线程绑定的连接关闭时被调用
	void thread_on_close(acl::socket_stream* conn);
};
