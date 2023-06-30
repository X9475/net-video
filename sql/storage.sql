DROP DATABASE IF EXISTS tnv_storagedb;
CREATE DATABASE tnv_storagedb;
USE tnv_storagedb;

CREATE TABLE `t_file_01` (
  `id` varchar(256) NOT NULL DEFAULT '' COMMENT '文件ID',
  `appid` varchar(32) DEFAULT NULL,
  `userid` varchar(128) DEFAULT NULL,
  `status` tinyint(4) DEFAULT NULL,
  `file_path` varchar(512) DEFAULT NULL,
  `file_size` bigint(20) DEFAULT NULL,
  `create_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `update_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `t_file_02` (
  `id` varchar(256) NOT NULL DEFAULT '' COMMENT '文件ID',
  `appid` varchar(32) DEFAULT NULL,
  `userid` varchar(128) DEFAULT NULL,
  `status` tinyint(4) DEFAULT NULL,
  `file_path` varchar(512) DEFAULT NULL,
  `file_size` bigint(20) DEFAULT NULL,
  `create_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `update_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `t_file_03` (
  `id` varchar(256) NOT NULL DEFAULT '' COMMENT '文件ID',
  `appid` varchar(32) DEFAULT NULL,
  `userid` varchar(128) DEFAULT NULL,
  `status` tinyint(4) DEFAULT NULL,
  `file_path` varchar(512) DEFAULT NULL,
  `file_size` bigint(20) DEFAULT NULL,
  `create_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `update_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
