// 存储服务器
// 实现文件操作O_TMPFILE类
#include <unistd.h>
#include <fcntl.h>
#include <lib_acl.hpp>
#include "01_types.h"
#include "07_file.h"

// 构造函数
file_c::file_c(void): m_fd(-1) {
}

// 析构函数
file_c::~file_c(void) {
	close();
}

// 打开文件
int file_c::open(char const* path, char flag) {
	// 检查路径
	if (!path || !strlen(path)) {
		logger_error("path is null");
		return ERROR;
	}
	// 打开文件
	if (flag == O_READ)
		m_fd = ::open(path, O_RDONLY);
	else if (flag == O_WRITE)
		m_fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	else {
		logger_error("unknown open flag: %c", flag);
		return ERROR;
	}
	// 打开失败
	if (m_fd == -1) {
		logger_error("call open fail: %s, path: %s, flag: %c", strerror(errno), path, flag);
		return ERROR;
	}

	return OK;
}

// 关闭文件
int file_c::close(void) {
	if (m_fd != -1) {
		// 关闭文件
		if (::close(m_fd) == -1) {
			logger_error("call close fail: %s", strerror(errno));
			return ERROR;
		}
		m_fd = -1;
	}
	return OK;
}

// 读取文件
int file_c::read(void* buf, size_t count) const {
	// 检查文件描述符
	if (m_fd == -1) {
		logger_error("invalid file handle");
		return ERROR;
	}
	// 检查文件缓冲区
	if (!buf) {
		logger_error("invalid file buffer");
		return ERROR;
	}
	// 读取给定字节数
	ssize_t bytes = ::read(m_fd, buf, count);
	if (bytes == -1) {
		logger_error("call read fail: %s", strerror(errno));
		return ERROR;
	}
	if ((size_t)bytes != count) {
		logger_error("unable to read expected bytes: %ld != %lu", bytes, count);
		return ERROR;
	}

	return OK;
}

// 写入文件
int file_c::write(void const* buf, size_t count) const {
	// 检查文件描述符
	if (m_fd == -1) {
		logger_error("invalid file handle");
		return ERROR;
	}
	// 检查文件缓冲区
	if (!buf) {
		logger_error("invalid file buffer");
		return ERROR;
	}
	// 写入给定字节数
	if (::write(m_fd, buf, count) == -1) {
		logger_error("call write fail: %s", strerror(errno));
		return ERROR;
	}

	return OK;
}

// 设置偏移
int file_c::seek(off_t offset) const {
	// 检查文件描述符
	if (m_fd == -1) {
		logger_error("invalid file handle");
		return ERROR;
	}
	// 检查文件偏移量
	if (offset < 0) {
		logger_error("invalid file offset");
		return ERROR;
	}
	// 设置文件偏移量
	if (lseek(m_fd, offset, SEEK_SET) == -1) {
		logger_error("call lseek fail: %s, offset: %ld", strerror(errno), offset);
		return ERROR;
	}

	return OK;
}

// 删除文件
int file_c::del(char const* path) {
	// 检查路径
	if (!path || !strlen(path)) {
		logger_error("path is null");
		return ERROR;
	}
	// 删除文件
	if (unlink(path) == -1) {
		logger_error("call unlink fail: %s, path: %s", strerror(errno), path);
		return ERROR;
	}

	return OK;
}
