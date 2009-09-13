/* Copyright (c) 2006 Pace Willisson, all rights reserved */

/*
 * Rules:
 *
 * Strings returned by dbget_str are valid only until the next db_prepare
 * or db_finish_transaction.  Be sure to copy them to a safe place before
 * doing anything else.
 */

char const *DB_NULL_FIELD;

void *db_init3 (char const *dbinfo_string);
void *db_init4 (char const *dbinfo_string);
void db_setdb (void *db);
void dberr (char const *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void dberr (char const *fmt, ...);
void db_close (void);
void db_cancel (void);
void db_cleanup (void);

void *db_get_conn (void);

void db_release_result (void);


void db_prepare1 (char const *file, int line, char const *stmt);
#define db_prepare(stmt) (db_prepare1 (__FILE__,__LINE__,(stmt)))

void dbset_str1 (char const *file, int line, int idx, char const *str);
#define dbset_str(idx, str) (dbset_str1 (__FILE__,__LINE__,(idx),(str)))

void dbset_int1 (char const *file, int line, int idx, int ival);
#define dbset_int(idx, ival) (dbset_int1 (__FILE__,__LINE__,(idx),(ival)))

void dbset_dbl1 (char const *file, int line, int idx, char const *fmt, double val);
#define dbset_dbl(idx,fmt,val) \
	(dbset_dbl1(__FILE__,__LINE__,(idx),(fmt),(val)))

char const *dbget_str1 (char const *file, int line, int row, int col);
#define dbget_str(row,col) (dbget_str1(__FILE__,__LINE__,(row),(col)))

void dbget_xstr1 (char const *file, int line, char *ret, int len,
		  int row, int col);
#define dbget_xstr(ret,len,row,col) (dbget_xstr1(__FILE__,__LINE__,\
					(ret),(len),(row),(col)))

int dbget_int1 (char const *file, int line, int row, int col);
#define dbget_int(row, col) (dbget_int1(__FILE__,__LINE__,(row),(col)))

time_t dbget_time1 (char const *file, int line, int row, int col);
#define dbget_time(row, col) (dbget_time1(__FILE__,__LINE__,(row),(col)))

double dbget_dbl1 (char const *file, int line, int row, int col);
#define dbget_dbl(row, col) (dbget_dbl1(__FILE__,__LINE__,(row),(col)))

int db_exec1 (char const *file, int line);
#define db_exec() (db_exec1 (__FILE__,__LINE__))

void db_start_transaction1 (char const *file, int line);
#define db_start_transaction() (db_start_transaction1 (__FILE__,__LINE__))

int db_finish_transaction1 (char const *file, int line);
#define db_finish_transaction() (db_finish_transaction1(__FILE__,__LINE__))

int db_set_savepoint1 (char const *file, int line, char const *name);
#define db_set_savepoint(name) (db_set_savepoint1(__FILE__,__LINE__,name))

int db_rollback_savepoint1 (char const *file, int line, char const *name);
#define db_rollback_savepoint(nam) (db_rollback_savepoint1(__FILE__,__LINE__,nam))

int db_release_savepoint1 (char const *file, int line, char const *name);
#define db_release_savepoint(nam) (db_release_savepoint1(__FILE__,__LINE__,nam))

char const *dbget_colname1 (char const *file, int line, int col);
#define dbget_colname(col) (dbget_colname1(__FILE__,__LINE__,(col)))

int dbget_nfields1 (char const *file, int line);
#define dbget_nfields() (dbget_nfields1(__FILE__,__LINE__))

void db_print (void);

void db_trans_step (int force);

int db_put_copy_data1 (char const *file, int line, char *buf, int len);
#define db_put_copy_data(buf, len) (db_put_copy_data1(__FILE__,__LINE__, \
						      (buf),(len)))

int db_put_copy_end1 (char const *file, int line, char *err);
#define db_put_copy_end(err) (db_put_copy_end1(__FILE__,__LINE__,(err)))

void db_report_long_queries (double secs);

void db_reconnect (void);
