DROP DATABASE IF EXISTS tnv_trackerdb;
CREATE DATABASE tnv_trackerdb;
USE tnv_trackerdb;

CREATE TABLE `t_groups_info` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `group_name` varchar(32) DEFAULT NULL,
  `create_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `update_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `t_groups_info` (`group_name`) VALUES ('group001');

CREATE TABLE `t_router` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `appid` varchar(32) DEFAULT NULL,
  `userid` varchar(128) DEFAULT NULL,
  `group_name` varchar(32) DEFAULT NULL,
  `create_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `update_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
