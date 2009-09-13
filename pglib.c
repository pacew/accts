/* Copyright (c) 2006 Pace Willisson, all rights reserved */

#include <opus/opus.h>

#include "pglib.h"

#include <libpq-fe.h>

#define MAX_VALS 100
#define STMT_PREFIX_SIZE 2000

double long_query_thresh_secs;

enum dbs {
	dbs_idle,
	dbs_in_transaction,
	dbs_in_query,
	dbs_done,
	dbs_copy_in,
};

struct db_state {
	enum dbs state;
	char const *start_file;
	int start_line;
	PGconn *conn;
	PGresult *res;
	char *stmt;
	int stmt_size;
	char stmt_prefix[STMT_PREFIX_SIZE];
	char dbmsg[200];
	int err;
	char errbuf[200];
	char *vals[MAX_VALS];
	int vals_sizes[MAX_VALS];
	int vals_idx;
	int trans_step_count;
};

static struct db_state *dbs;

void *
db_init3 (char const *dbinfo_str)
{
	DB_NULL_FIELD = xstrdup ("");
	
	/* If the database type is given, then make sure it is correct,
	 *  otherwise assume it is correct.
	 */
	if (strncmp ("pgsql:", dbinfo_str, 6) == 0)
		dbinfo_str += 6;

	dbs = xcalloc (1, sizeof *dbs);

	if ((dbs->conn = PQconnectdb (dbinfo_str)) == NULL)
		fatal ("can't connect to postgres\n");

	if (PQstatus (dbs->conn) != CONNECTION_OK)
		fatal ("error connecting to database: %s\n",
		       PQerrorMessage (dbs->conn));

	return (dbs);
}

void *
db_init4 (char const *dbinfo_str)
{
	PGconn *conn;

	if (DB_NULL_FIELD == NULL) DB_NULL_FIELD = xstrdup ("");
	
	if ((conn = PQconnectdb (dbinfo_str)) == NULL)
		return (NULL);

	if (PQstatus (conn) != CONNECTION_OK) {
		PQfinish (conn);
		return (NULL);
	}

	dbs = xcalloc (1, sizeof *dbs);
	dbs->conn = conn;

	return (dbs);
}

void
db_setdb (void *db)
{
	dbs = db;
}

void *
db_get_conn (void)
{
	return (dbs->conn);
}

void
db_close ()
{
	int i;
	 if (dbs->conn == NULL) {
		  dbg ("trying to close a closed connection");
		  return;
	 }

	if (dbs->res)
		PQclear (dbs->res);
	
	 PQfinish (dbs->conn);

	 for (i = 0; i < MAX_VALS; i++) {
		 if (dbs->vals[i])
			 free (dbs->vals[i]);
	 }

	 if (dbs->stmt)
		 free (dbs->stmt);


	 free (dbs);
	 dbs = NULL;
}

void
db_cleanup (void)
{
	if (dbs)
		db_close ();
	free ((void *)DB_NULL_FIELD);
	DB_NULL_FIELD = NULL;
}

void
db_cancel (void)
{
	PGcancel *can;
	char err[1000];

	if (dbs->conn == NULL) {
		dbg ("trying to do a cancel on a closed connection");
		return;
	}
	
	if ((can = PQgetCancel (dbs->conn)) == NULL) {
		dbg ("can't get pg cancel structure\n");
		return;
	}
	
	if (PQcancel (can, err, sizeof err) == 0) {
		dbg ("problem doing pq cancel: %s\n", err);
	}
	
	PQfreeCancel (can);
}

int
db_check_connect()
{
 
	if ( dbs->conn != NULL ) {
		
		//this is unreliable at best
		//PQstatus returns OK even when I KNOW it is not
		//it takes a while for libpq to catch up..
		if (PQstatus(dbs->conn) != CONNECTION_OK) {
			dbs->err = 1;
			return 1;
		}

	} else {
		return 1;
	}

	return 0;

}


void
db_reconnect(void)
{

	if ( dbs->conn != NULL ) {
		PQreset(dbs->conn);
	}

	if ( db_check_connect() ) {
		fatal("can't reconnect/reset.");
	}

}

void
dberr (char const *fmt, ...)
{
	va_list args;

	va_start (args, fmt);
	vsnprintf (dbs->errbuf, sizeof dbs->errbuf, fmt, args);
	va_end (args);
		   
	dbs->err = 1;
}

void
db_prepare1 (char const *file, int line, char const *stmt)
{
	int need;
	int len;

	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}


	if (dbs->err)
		return;

	switch (dbs->state) {
	case dbs_idle:
		dbg ("%s:%d: wrong state: need db_start_transaction\n",
		     file, line);
		break;
	case dbs_in_transaction:
		dbg ("%s:%d: wrong state: need db_exec\n", file, line);
		break;
	case dbs_in_query:
		dbg ("%s:%d: wrong state: need db_exec\n", file, line);
		break;
	case dbs_done:
		break;
	case dbs_copy_in:
		dbg ("%s:%d: wrong state: need db_put_copy_end\n", file, line);
		break;
	}
	
	dbs->state = dbs_in_transaction;

	dbs->vals_idx = 0;

	need = strlen (stmt) + 1;

	if (need > dbs->stmt_size) {
		dbs->stmt_size = need;
		dbs->stmt = xrealloc (dbs->stmt, dbs->stmt_size);
	}

	strcpy (dbs->stmt, stmt);

	/* for printing error messages */
	len = strlen (dbs->stmt);
	if (len < STMT_PREFIX_SIZE) {
		strcpy (dbs->stmt_prefix, dbs->stmt);
	} else {
		strncpy (dbs->stmt_prefix, dbs->stmt, STMT_PREFIX_SIZE);
		strcpy (dbs->stmt_prefix + STMT_PREFIX_SIZE - 4, "...");
	}
}

void
dbset_str1 (char const *file, int line, int idx, char const *str)
{
	int need;


	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}

	if (dbs->err)
		return;

	if (idx >= MAX_VALS)
		fatal ("%s:%d: dbset_str: out of bounds\n", file, line);

	/* idx arg is 1-based, but vals_idx is 0 based */
	if (idx != dbs->vals_idx + 1) {
		dberr ("%s:%d: db parameter %d out of sequence",
		       file, line, idx);
		return;
	}

	if (str == DB_NULL_FIELD || str == 0) {
		dbs->vals[dbs->vals_idx] = NULL;
	} else {
		need = strlen (str) + 1;
		if (need > dbs->vals_sizes[dbs->vals_idx]) {
			xfree (dbs->vals[dbs->vals_idx]);
			dbs->vals[dbs->vals_idx] = xmalloc (need);
		}
		strcpy (dbs->vals[dbs->vals_idx], str);
	}
	dbs->vals_idx++;
}

void
dbset_int1 (char const *file, int line, int idx, int ival)
{
	char numbuf[100];
	
	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}


	snprintf (numbuf, sizeof numbuf, "%d", ival);
	dbset_str1 (file, line, idx, numbuf);
}

void
dbset_dbl1 (char const *file, int line, int idx, char const *fmt, double val)
{
	char numbuf[100];
	
	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}

	snprintf (numbuf, sizeof numbuf, fmt, val);
	dbset_str1 (file, line, idx, numbuf);
}

/*
 * the pointer returned is in the res structure, so it will remain
 * valid until that is cleared at the start of the next query
 */
char const *
dbget_str1 (char const *file, int line, int row, int col)
{
	char const *val;
		

	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}

	if (dbs->err)
		return ("");

	if (dbs->res == NULL || dbs->state != dbs_done) {
		dberr ("%s:%d: wrong state need db_exec\n", file, line);
		return ("");
	}

	if (row == 0 || col == 0) {
		dberr ("%s:%d: dbget is 1 based\n", file, line);
		return ("");
	}

	if (PQgetisnull (dbs->res, row-1, col-1))
		return (DB_NULL_FIELD);

	if ((val = PQgetvalue (dbs->res, row-1, col-1)) == NULL) {
		dberr ("%s:%d: dbget_str: error in PQgetvalue", file, line);
		return ("");
	}

	return (val);
}

void
dbget_xstr1 (char const *file, int line, char *ret, int len, int row, int col)
{
	char const *val;

	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}

	if (len <= 0)
		return;

	val = dbget_str1 (file, line, row, col);
	xstrcpy (ret, val, len);
}	

int
dbget_int1 (char const *file, int line, int row, int col)
{
	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}
	
	return (atoi (dbget_str1 (file, line, row, col)));
}

time_t
dbget_time1 (char const *file, int line, int row, int col)
{
	char const *p;
	struct tm tm;
	int n;

	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}

	p = dbget_str1 (file, line, row, col);

	memset (&tm, 0, sizeof tm);
	n = sscanf (p, "%d-%d-%d %d:%d:%d",
		    &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
		    &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
	if (n != 6)
		return (0);

	tm.tm_year -= 1900;
	tm.tm_mon--;
	tm.tm_isdst = -1;

	return (mktime (&tm));
}

double 
dbget_dbl1 (char const *file, int line, int row, int col)
{

	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}

	return (atof (dbget_str1 (file, line, row, col)));
}

int
db_exec1 (char const *file, int line)
{
	int rc;
	unsigned long start;
	double secs;

	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}

	if (dbs->err)
		return (-1);

	switch (dbs->state) {
	case dbs_idle:
		dbg ("%s:%d: wrong state: need db_start_transaction\n",
		     file, line);
		break;
	case dbs_in_transaction:
		break;
	case dbs_in_query:
		dberr ("%s:%d: wrong state: need db_prepare\n", file, line);
		break;
	case dbs_done:
		dbg ("%s:%d: wrong state: need db_start_transaction\n",
		     file, line);
		break;
	case dbs_copy_in:
		dbg ("%s:%d: wrong state: need db_put_copy_end\n", file, line);
		break;
	}
	
	if (dbs->res)
		PQclear (dbs->res);

	start = millitime ();
	dbs->res = PQexecParams (dbs->conn, dbs->stmt, dbs->vals_idx, NULL,
				(char const **)dbs->vals, NULL, NULL, 0);
	secs = (millitime () - start) / 1000.0;
	if (long_query_thresh_secs && secs > long_query_thresh_secs)
		dbg ("%s:%d: long query %.3f\n", file, line, secs);
	
	if (dbs->res == NULL) {
		dberr ("%s:%d: db_exec: PQexecParams returned NULL",
		       file, line);
		return (-1);
	}

	dbs->state = dbs_done;

	rc = PQresultStatus (dbs->res);

	if (rc == PGRES_COMMAND_OK)
		return (0);

	if (rc == PGRES_TUPLES_OK)
		return (PQntuples (dbs->res));

	if (rc == PGRES_COPY_IN) {
		dbs->state = dbs_copy_in;
		return (0);
	}

	dbg ("%s:%d: dberr: %s    %s\n", file, line,
	     PQerrorMessage (dbs->conn), dbs->stmt_prefix);
	return (-1);
}

void
db_release_result (void)
{
	if (dbs->res) {
		PQclear (dbs->res);
		dbs->res = NULL;
	}
}


void
db_print (void)
{
	int i;
	printf ("================\n");
	printf ("%s\n", dbs->stmt);
	for (i = 0; i < dbs->vals_idx; i++) {
		printf ("\t$%d = \"%s\"\n", i+1, dbs->vals[i]);
	}
	printf ("================\n");
}

void
db_start_transaction1 (char const *file, int line)
{
	int need_rollback;

	//this is the only place it would be safe to reconnect
	if ( db_check_connect() ) {
		dbg("%s:%d: db connection lost. attempting to recover.",
		      file, line);
		db_reconnect();
	}


	need_rollback = 1;

	switch (dbs->state) {
	case dbs_idle:
		need_rollback = 0;
		break;
	case dbs_in_transaction:
		dbg ("%s:%d: wrong state: need db_prepare\n", file, line);
		break;
	case dbs_in_query:
		dbg ("%s:%d: wrong state: need db_exec\n", file, line);
		break;
	case dbs_done:
		dbg ("%s:%d: wrong state: need db_finish_transaction\n",
		     file, line);
		dbg ("      last start was %s:%d\n",
		     dbs->start_file, dbs->start_line);
		break;
	case dbs_copy_in:
		dbg ("%s:%d: wrong state: need db_put_copy_end\n", file, line);
		break;
	}
	
	if (need_rollback) {
		dbs->err = 0;
		dbs->errbuf[0] = 0;
		db_prepare ("ROLLBACK");
		db_exec ();
	}

	dbs->vals_idx = 0;
	dbs->err = 0;
	dbs->errbuf[0] = 0;

	dbs->start_file = file;
	dbs->start_line = line;

	dbs->state = dbs_done;
	db_prepare ("START TRANSACTION");
	db_exec ();
}

int
db_finish_transaction1 (char const *file, int line)
{
	char stmt_prefix[STMT_PREFIX_SIZE];
	int need_rollback;

	dbs->trans_step_count = 0;

	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}

	need_rollback = 1;
	
	if (dbs->err == 0) {
		switch (dbs->state) {
		case dbs_idle:
			dbg ("%s:%d: wrong state: need db_start_transaction",
			    file, line);
			break;
		case dbs_in_transaction:
			dbg ("%s:%d: wrong state: need db_prepare",
			     file, line);
			break;
		case dbs_in_query:
			dbg ("%s:%d: db_start: wrong state: need db_exec",
			     file, line);
			break;
		case dbs_done:
			need_rollback = 0;
			break;
		case dbs_copy_in:
			dbg ("%s:%d: wrong state: need db_put_copy_end\n",
			     file, line);
			break;
		}
	}

	strcpy (stmt_prefix, dbs->stmt_prefix);
	dbs->stmt_prefix[0] = 0;

	dbs->vals_idx = 0;

	if (dbs->err) {
		need_rollback = 1;
		dbg ("%s\n", dbs->errbuf); /* errbuf already include linenum */
		dbg ("   %s\n", stmt_prefix);
	}

	if (need_rollback) {
		dbs->err = 0;
		db_prepare ("ROLLBACK");
		db_exec ();
		dbs->state = dbs_idle;
		return (-1);
	}

	db_prepare ("COMMIT");
	db_exec ();
	dbs->state = dbs_idle;

	if (dbs->err) {
		dbg ("%s:%d: dberr: commit error: %s\n",
		     file, line, stmt_prefix);
		return (-1);
	}

	return (0);
}

int
db_set_savepoint1 (char const *file, int line, char const *name)
{
	char sql[1000];

	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}


	snprintf (sql, sizeof sql, "savepoint %s", name);
	db_prepare (sql);
	 
	if (db_exec () != 0) 
		return (-1);
	return (0);
}



int
db_rollback_savepoint1 (char const *file, int line, char const *name)
{
	char sql[100];
	int need_rollback;
	
	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}

	need_rollback = 1;

	switch (dbs->state) {
	case dbs_idle:
		dbg("%s:%d: wrong state: no transaction\n", file, line);
		need_rollback = 0;
		break;
	case dbs_done:
	case dbs_in_transaction:
	case dbs_in_query:
		break;
	case dbs_copy_in:
		dbg ("%s:%d: wrong state: need db_put_copy_end\n", file, line);
		break;
	}

	if (need_rollback == 0)
		return (-1);

	dbs->err = 0;
	snprintf (sql, sizeof sql, "rollback to savepoint %s", name);
	db_prepare(sql);
	db_exec ();
	
	if (dbs->err) {
		dbg ("%s:%d: dberr: savepoint rollback: %s\n",
		     file, line, name);
		return (-1);
	}

	return (0);
}

int
db_release_savepoint1 (char const *file, int line, char const *name)
{
	char sql[100];
	int need_release;

	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}

	need_release = 1;
	
	switch (dbs->state) {
	case dbs_idle:
		dbg("%s:%d: wrong state: no transaction\n", file, line);
		need_release = 0;
		break;
	case dbs_done:
	case dbs_in_transaction:
	case dbs_in_query:
		break;
	case dbs_copy_in:
		dbg ("%s:%d: wrong state: need db_put_copy_end\n", file, line);
		break;
	}

	if (need_release == 0)
		return (-1);

	dbs->err = 0;
	snprintf (sql, sizeof sql, "release savepoint %s", name);
	db_prepare (sql);
	db_exec ();

	if (dbs->err) {
		dbg ("%s:%d: dberr: savepoint release: %s\n",
		     file, line, name);
		return (-1);
	}

	return (0);
}


char const *
dbget_colname1 (char const *file, int line, int col)
{
	char const *val;
	
	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}
	
	if (dbs->err)
		return ("");

	if (dbs->res == NULL || dbs->state != dbs_done) {
		dberr ("%s:%d: wrong state need db_exec\n", file, line);
		return ("");
	}

	if (col == 0) {
		dberr ("%s:%d: dbget is 1 based\n", file, line);
		return ("");
	}

	
	if ((val = PQfname (dbs->res, col-1)) == NULL) {
		dberr ("%s:%d: dbget_colname: error in PQfname", file, line);
		return ("");
	}

	return (val);
}


int 
dbget_nfields1 (char const *file, int line)
{
	
	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}	
	
	if (dbs->err)
		return 0;
	
	if (dbs->res == NULL || dbs->state != dbs_done) {
		dberr ("%s:%d: wrong state need db_exec\n", file, line);
		return 0;
	}
	
	return PQnfields (dbs->res);

}

void
db_trans_step (int force)
{
	if (dbs->trans_step_count > 100
	    || (force && dbs->trans_step_count > 0))
		db_finish_transaction ();

	if (force == 2)
		return;

	if (dbs->trans_step_count == 0)
		db_start_transaction ();
	dbs->trans_step_count++;
}

int
db_put_copy_data1 (char const *file, int line, char *buf, int len)
{
	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}
	
	if (dbs->err)
		return (-1);
	
	if (dbs->res == NULL || dbs->state != dbs_copy_in) {
		dberr ("%s:%d: not in copy_in state\n", file, line);
		return 0;
	}

	if (PQputCopyData (dbs->conn, buf, len) < 0) {
		dberr ("error in PQputCopyData\n");
		return (-1);
	}

	return (0);
}

int
db_put_copy_end1 (char const *file, int line, char *err)
{
	int rc;

	if ( db_check_connect() ) {
		fatal("%s:%d: db connection lost. can't safely recover.",
		      file, line);
	}
	
	if (dbs->err)
		return (-1);
	
	if (dbs->res == NULL || dbs->state != dbs_copy_in) {
		dberr ("%s:%d: not in copy_in state\n", file, line);
		return (-1);
	}

	if (PQputCopyEnd (dbs->conn, err) < 0)
		dberr ("%s:%d: error in PQputCopyEnd\n", file, line);

	dbs->res = PQgetResult (dbs->conn);

	if (dbs->res == NULL) {
		dberr ("%s:%d: db_put_copy_end: PQgetResult returned NULL",
		       file, line);
		return (-1);
	}

	dbs->state = dbs_done;

	rc = PQresultStatus (dbs->res);

	if (rc == PGRES_COMMAND_OK)
		return (0);

	if (rc == PGRES_TUPLES_OK)
		return (PQntuples (dbs->res));

	dbg ("%s:%d: dberr in copy: %s    %s\n", file, line,
	     PQerrorMessage (dbs->conn), dbs->stmt_prefix);

	return (-1);
}

void
db_report_long_queries (double secs)
{
	long_query_thresh_secs = secs;
}
