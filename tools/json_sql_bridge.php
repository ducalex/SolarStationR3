<?php
require_once 'Database/db.sqlite.php';
//require_once 'Database/db.mysql.php';

const DB_HOSTNAME = '';
const DB_USERNAME = '';
const DB_PASSWORD = '';
const DB_DATABASE = 'test.sqlite';

$base_schema_status = [
	'server_time' => 'integer',
	'time'        => ['integer', null, Db::PRIMARY],
	'station'     => 'string|64',
	'version'     => 'string|64',
	'build'       => 'string|64',
	'uptime'      => 'integer',
	'cycles'      => 'integer',
	'ntp_delta'   => 'integer',
];

$base_schema_sensors = [
	'server_time' => 'integer',
	'time'        => ['integer', null, Db::PRIMARY],
	'station'     => 'string|64',
	'status'      => 'integer',
];

$server_time = ['server_time' => microtime(true) * 1000];

$body = file_get_contents('php://input');
$json = json_decode($body, true);

if (!$json) {
	http_response_code(415);
	die;
}
try {
	Db::Connect(DB_HOSTNAME, DB_USERNAME, DB_PASSWORD, DB_DATABASE, "{$json['group']}_");

	// Create missing tables and indexes
	if (!Db::TableExists('status')) {
		Db::CreateTable('status', $base_schema_status);
		Db::AddIndex('status', 'index', ['station']);
	}

	if (!Db::TableExists('sensors')) {
		Db::CreateTable('sensors', $base_schema_sensors);
		Db::AddIndex('sensors', 'index', ['station']);
	}

	Db::Insert('status', $server_time + array_intersect_key($json, $base_schema_status), true);

	if (!empty($json['data'])) {
		// Add new/missing sensor columns
		foreach($json['data'][0] as $col => $val) {
			if (!isset($base_schema_sensors[$col])) {
				Db::AddColumnIfNotExists('sensors', $col, 'float');
			}
		}
		// Insert sensors data
		foreach($json['data'] as $entry) {
			Db::Insert('sensors', $server_time + $entry, true);
		}
	}
	http_response_code(204);
	die;
}
catch (Exception $e) {
	http_response_code(500);
	die(json_encode(['error' => $e->getMessage()]));
}
