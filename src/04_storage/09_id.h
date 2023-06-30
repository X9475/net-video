// 存储服务器
// 声明ID客户机类
#pragma once

// ID客户机类
class id_c {
public:
	// 从ID服务器获取与ID键相对应的值
	long get(char const* key) const;

private:
	// 向ID服务器发送请求，接收并解析响应，从中获得ID值
	long client(char const* requ, long long requlen) const;
};
