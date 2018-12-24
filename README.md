# IOTBuoy
Source code for a remote measurement buoy.

Use the RadioHead library from https://github.com/huukit/RadioHead and poll for interrupts if using additional modules on the SPI-bus.

For the database, run: 

CREATE TABLE `data` (
  `row` bigint(20) NOT NULL AUTO_INCREMENT,
  `id` bigint(20) DEFAULT NULL,
  `rssi` bigint(20) DEFAULT NULL,
  `timestamp` datetime DEFAULT NULL,
  `batterymV` bigint(20) DEFAULT NULL,
  `airTemp` double DEFAULT NULL,
  `airHumidity` double DEFAULT NULL,
  `airPressureHpa` bigint(20) DEFAULT NULL,
  `waterArrayCount` bigint(20) DEFAULT NULL,
  `waterTemp1` double DEFAULT NULL,
  `waterTemp2` double DEFAULT NULL,
  `waterTemp3` double DEFAULT NULL,
  `waterTemp4` double DEFAULT NULL,
  `waterTemp5` double DEFAULT NULL,
  `waterTemp6` double DEFAULT NULL,
  PRIMARY KEY (`row`),
  KEY `identifier` (`id`),
  CONSTRAINT `identifier` FOREIGN KEY (`id`) REFERENCES `poijut` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=135220 DEFAULT CHARSET=utf8mb4


CREATE TABLE `poijut` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `name` text,
  `position` text,
  `date_added` datetime DEFAULT NULL,
  `timestamp` datetime DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=11 DEFAULT CHARSET=utf8mb4
