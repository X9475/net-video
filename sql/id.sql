DROP DATABASE IF EXISTS tnv_idsdb;
CREATE DATABASE tnv_idsdb;
USE tnv_idsdb;

CREATE TABLE `t_id_gen` (
  `id` varchar(64) NOT NULL DEFAULT '',
  `id_value` bigint(20) DEFAULT NULL,
  `create_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `update_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
