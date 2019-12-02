<?php
/*
 * Db Singleton abstraction.
 * Copyright (c) 2014, Alex Duchesne <alex@alexou.net>.
 *
 * Licensed under the MIT license:
 * 	http://opensource.org/licenses/MIT
 */

abstract class Database
{
	const PRIMARY = 1;
	const AI = 2;

	public static $db = null;

	public static $throwException = true;
	public static $queryLogging = true;
	public static $queries = array();
	public static $num_queries = 0;
	public static $exec_time = 0;
	public static $errno = 0;
	public static $error = null;
	public static $affected_rows = 0;
	public static $insert_id = 0;

	public static $host, $user, $password, $database, $prefix;

	abstract public static function Connect($host, $user, $password, $database = '', $prefix = '');

	abstract public static function CreateTable($table, $fields, $if_not_exists = false, $drop_if_exists = false);

	abstract public static function DropTable($table, $if_exists = true);

	abstract public static function AddIndex($table, $type, $fields);

	abstract public static function AddColumn($table_name, $col_name, $col_type, $primary = false, $auto_increment = false, $default = null);

	abstract public static function GetColumns($table, $names_only = false);

	abstract public static function TableExists($table);

	abstract public static function GetTables($full_schema = false);

	abstract public static function Truncate($table);

	abstract public static function Import($input, $format = 'sql');

	abstract public static function Export($output = null, $format = 'sql');


	public static function AvailableDrivers()
	{
		$pdo = PDO::getAvailableDrivers();
		$files = array_map('basename', glob(__DIR__.'/db.*.php'));
		$files = str_replace(['db.', '.php'], '', $files);

		return array_intersect($pdo, $files);
	}


	public static function DriverName()
	{
		return self::$db->getAttribute(PDO::ATTR_DRIVER_NAME);
	}


	public static function ServerVersion()
	{
		return self::$db->getAttribute(PDO::ATTR_SERVER_VERSION);
	}


	public static function GetTableName($table)
	{
		if ($table[0] === '"' || $table[0] === '`' || strpos($table, '.') !== false) {
			return $table;
		}
		$table = self::$prefix . trim($table, '{}');

		return $table;
	}


	public static function AddColumnIfNotExists($table_name, $col_name, $col_type, $primary = false, $auto_increment = false, $default = null)
	{
		$columns  = static::GetColumns($table_name, true);

		if (!in_array($col_name, $columns)) {
			return static::AddColumn($table_name, $col_name, $col_type, $primary, $auto_increment, $default);
		}

		return true;
	}


	public static function DropColumnIfExists($table_name, $col_name)
	{
		$columns  = static::GetColumns($table_name, true);

		if (in_array($col_name, $columns)) {
			return static::DropColumn($table_name, $col_name);
		}

		return true;
	}


	public static function escapeValue($value, $quote = true)
	{
		if (ctype_digit($value)) $value = (int) $value;

		switch (gettype($value)) {
			case 'NULL': return 'NULL';
			case 'float':
			case 'double':
			case 'integer': return $value;
			default: return $quote ? self::$db->quote($value) : substr(self::$db->quote($value), 1, -1);
		}
	}


	public static function escapeField($value)
	{
		return '`' . str_replace('`', '``', $value) . '`';
	}


	public static function escape($string)
	{
		return substr(self::$db->quote($string), 1, -1);
	}


	/**
	 * Execute an SQL statement and returns a PDO statement on success
	 *
	 * @param string $query SQL query
	 * @param mixed ...$args placeholder replacements
	 * @param bool $throwExceptionOnError = true
	 * @return PDOStatement|false
	 */
	public static function Query($query, ...$args)
	{
		$query = preg_replace('!([^a-z0-9])\{([_a-z0-9]+)\}([^a-z0-9]|$)!i', '$1' . self::$prefix . '$2$3', $query);
		$throwException = $args && is_bool(end($args)) ? array_pop($args) : self::$throwException;
		self::$db->setAttribute(PDO::ATTR_ERRMODE, $throwException ? PDO::ERRMODE_EXCEPTION : PDO::ERRMODE_SILENT);

		if ($args) {
			if (is_array($args[0])) $args = $args[0]; // Db::Query("SQL", [':named' => params])
			array_unshift($args, ''); // Remove 0 index, PDO is 1-based
			unset($args[0]);
		}

		$start = microtime(true);

		try {
			if ($q = Db::$db->prepare($query)) {
				foreach($args as $i => $arg) {
					if (ctype_digit((string)$arg))
						$q->bindValue($i, (int)$arg, PDO::PARAM_INT);
					elseif($arg === null)
						$q->bindValue($i, $arg, PDO::PARAM_NULL);
					else
						$q->bindValue($i, $arg);
				}

				$q->execute();
			}
		} catch (PDOException $exception) {
			$q = false;
		}

		$error = $q ? $q->errorInfo() : self::$db->errorInfo();

		self::$errno = $error[1];
		self::$error = $error[2];
		self::$affected_rows = $q ? $q->rowCount() : 0;
		self::$insert_id = $q ? self::$db->lastInsertId() : 0;
		self::$exec_time += microtime(true) - $start;
		self::$num_queries++;

		if (self::$queryLogging) {
			self::$queries[self::$num_queries] = array(
				'query' => $query,
				'params' => &$args,
				'time' => microtime(true) - $start,
				'errno' => self::$errno,
				'error' => self::$error,
				'affected_rows' => self::$affected_rows,
				'fetch' => 0,
				'insert_id' => self::$insert_id,
			);

			foreach(debug_backtrace(false) as $trace) {
				if (isset($trace['file']) && $trace['file'] != __FILE__) {
					self::$queries[self::$num_queries]['trace'] = $trace;
					break;
				}
			}
		}

		if ($throwException && isset($exception)) {
			throw $exception;
		}

		return $q;
	}


	/**
	 * Returns a single row, or a single column if $entire_row is false
	 *
	 * @param string $query SQL query
	 * @param mixed ...$args placeholder replacements
	 * @param bool $entire_row set to false to get the first column only
	 * @return mixed|false
	 */
	public static function QuerySingle($query, ...$args /*, $entire_row = true */)
	{
		$entire_row = is_bool(end($args)) ? array_pop($args) : true;

		if ($q = self::Query($query, ...$args)) {
			if ($row = $q->fetch(PDO::FETCH_ASSOC)) {
				if (self::$queryLogging) self::$queries[self::$num_queries]['fetch'] = 1;
				return $entire_row ? $row : reset($row);
			}
		}
		return false;
	}


	/**
	 * This function returns an array of all rows returned by the SQL query
	 *
	 * @param string $query SQL query
	 * @param mixed ...$args placeholder replacements
	 * @param bool $use_first_col_as_key
	 * @return mixed|false
	 */
	public static function QueryAll($query, ...$args)
	{
		$use_first_col_as_key = is_bool(end($args)) ?  array_pop($args) : false;

		$r = array();

		if ($q = self::Query($query, ...$args)) {
			if ($use_first_col_as_key) { //return FETCH_GROUP
				while($row = $q->fetch(PDO::FETCH_ASSOC)) $r[reset($row)] = $row;
			} else {
				$r = $q->fetchAll(PDO::FETCH_ASSOC);
			}
			if (self::$queryLogging) self::$queries[self::$num_queries]['fetch'] = count($r);

			return $r;
		}
		return false;
	}


	/**
	 * Alias for QueryAll
	 */
	public static function GetAll($query, ...$args)
	{
		return self::QueryAll($query, ...$args);
	}


	/**
	 * This function returns one column if only one column is present in the result. Otherwise it returns the whole row.
	 *
	 * @param string $query SQL query
	 * @param mixed ...$args placeholder replacements
	 * @return mixed|false
	 */
	public static function Get($query, ...$args)
	{
		$args[] = true;

		$row = self::QuerySingle($query, ...$args);

		if (is_array($row) && count($row) === 1) {
			return reset($row);
		}

		return $row;
	}


	/**
	 * Execute an SQL statement and returns the number of affected rows
	 *
	 * @param string $query SQL query
	 * @param mixed ...$args placeholder replacements
	 * @param bool $throwExceptionOnError = true
	 * @return int|false
	 */
	public static function Exec($query, ...$args)
	{
		if (($r = self::Query($query, ...$args)) && self::$errno == 0) {
			return self::$affected_rows;
		}
		return false;
	}


	/**
	 * Inserts one or more rows in a table
	 *
	 * @param string $table
	 * @param array $rows
	 * @param boolean $replace
	 * @return int|false
	 */
	public static function Insert($table, array $rows, $replace = false)
	{
		if (empty($rows))
			return false;

		if (!is_array(reset($rows)))
			$rows = array($rows);

		$head = array_keys(current($rows));
		sort($head);

		$fields = array_map('self::escapeField', $head);

		$values = array();

		foreach($rows as $i => $row) {
			ksort($row); // Let's not be too strict, as long as all columns are there
			if (array_keys($row) !== $head) { // We need to make sure all rows contain the same columns
				if (self::$throwException)
					throw new Exception("INSERT ERROR: Unmatching columns on row $i, make sure each row contains the same columns");
				else
					return false;
			}
			$inserts[] = '(' . rtrim(str_repeat('?,', count($row)), ',') . ')';
			$values = array_merge($values, array_values($row));
			// $inserts[] = '(' . implode(',', array_map('self::escapeValue', $row)) . ')';
		}

		$command = $replace ? 'REPLACE INTO ' : 'INSERT INTO ';
		$command .= self::GetTableName($table);
		$command .= ' (' . implode(',', $fields) . ') VALUES ';
		$command .= implode(',', $inserts);

		return self::Exec($command, $values);
	}


	/**
	 * Updates one or more rows in a table
	 *
	 * @param string $table
	 * @param array $fields
	 * @param array $where
	 * @return int|false
	 */
	public static function Update($table, array $fields, $where = ['id' => 0])
	{
		$set = $cond = $values = [];

		foreach($fields as $field => $value) {
			$set[] = self::escapeField($field) . ' = ?';
			$values[] = $value;
		}

		foreach($where as $field => $value) {
			$cond[] = self::escapeField($field) . ' = ?';
			$values[] = $value;
		}

		$query = 'UPDATE {' . $table . '} SET ' . implode(', ', $set) . ' WHERE ' . implode(' AND ', $cond);

		return self::Exec($query, ...$values);
	}


	/**
	 * Delete one or more rows in a table
	 *
	 * @param string $table
	 * @param array $where
	 * @return int|false
	 */
	public static function Delete($table, $where = ['id' => 0])
	{
		$cond = $values = [];

		foreach($where as $field => $value) {
			$cond[] = self::escapeField($field) . ' = ?';
			$values[] = $value;
		}

		$query = 'DELETE FROM {' . $table . '} WHERE ' . implode(' AND ', $cond);

		return self::Exec($query, ...$values);
	}
}
