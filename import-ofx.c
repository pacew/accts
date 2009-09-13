#include <opus/opus.h>
#include <libofx/libofx.h>

#include "pglib.h"

void read_ofx (char *filename);

void
usage (void)
{
	fprintf (stderr, "usage: money\n");
	exit (1);
}

int
main (int argc, char **argv)
{
	int c;

	while ((c = getopt (argc, argv, "")) != EOF) {
		switch (c) {
		default:
			usage ();
		}
	}

	if (optind != argc)
		usage ();

	db_init3 ("dbname=wtrans");

	read_ofx ("checking.ofx");

	db_trans_step (1);

	return (0);
}

int
cb_status (const struct OfxStatusData data, void *status_data)
{
	printf ("status %s %s %s %s\n",
		data.ofx_element_name,
		data.name,
		data.description,
		data.server_message);
	return (true);
}

int
cb_account (const struct OfxAccountData data, void * account_data)
{
	printf ("account_id %s\n", data.account_id);

	return (true);
}


int
cb_statement (const struct OfxStatementData data, void * statement_data)
{
	printf ("balance %.2f\n", data.ledger_balance);
	return (true);
}

int
cb_transaction (const struct OfxTransactionData data, void * transaction_data)
{
	char trans_type_buf[1000], posted_buf[1000];
	char const *new_ref, *db_ref;
	char const *new_name, *db_name;
	char const *new_memo, *db_memo;
	char const *new_posted, *db_posted;
	char const *new_trans_type, *db_trans_type;
	double new_amount, db_amount;
	struct tm tm;
	int row, col;
	int nrows;
	int id;
	int diff;

	if (data.account_id_valid == 0)
		fatal ("no acct_id");
	if (data.fi_id_valid == 0)
		fatal ("no fi_id\n");

	new_ref = "";
	new_name = "";
	new_memo = "";
	new_amount = 0;
	new_posted = "";
	new_trans_type = "";

	if (data.check_number_valid) {
		new_ref = data.check_number;
	} else if (data.reference_number_valid) {
		new_ref = data.reference_number;
	}

	if (data.name_valid)
		new_name = data.name;

	if (data.memo_valid)
		new_memo = data.memo;

	if (data.amount_valid)
		new_amount = data.amount;

	if (data.date_posted_valid) {
		tm = *localtime (&data.date_posted);
		sprintf (posted_buf, "%d-%02d-%02d %02d:%02d:%02d",
			 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			 tm.tm_hour, tm.tm_min, tm.tm_sec);
		new_posted = posted_buf;
	}

	if (data.transactiontype_valid) {
		switch (data.transactiontype) {
		case OFX_CREDIT: new_trans_type = "credit"; break;
		case OFX_DEBIT: new_trans_type = "debit"; break;
		case OFX_DEP: new_trans_type = "deposit"; break;
		case OFX_CHECK: new_trans_type = "check"; break;
		default:
			sprintf (trans_type_buf, "unknown-%d",
				 data.transactiontype);
			new_trans_type = trans_type_buf;
			break;
		}
	}

	db_trans_step (0);
	db_prepare ("select id, ref, name, memo,"
		    "       amount, posted, trans_type"
		    " from raw_trans"
		    " where acct_id = $1 and trans_id = $2");
	col = 1;
	dbset_str (col++, data.account_id);
	dbset_str (col++, data.fi_id);
	if ((nrows = db_exec ()) < 0)
		fatal ("db error");

	db_ref = "";
	db_name = "";
	db_memo = "";
	db_amount = 0;
	db_posted = "";
	db_trans_type = "";
	if (nrows > 0) {
		row = 1;
		col = 1;
		id = dbget_int (row, col++);
		db_ref = dbget_str (row, col++);
		db_name = dbget_str (row, col++);
		db_memo = dbget_str (row, col++);
		db_amount = dbget_dbl (row, col++);
		db_posted = dbget_str (row, col++);
		db_trans_type = dbget_str (row, col++);
	} else {
		db_prepare ("select nextval('seq')");
		db_exec ();
		id = dbget_int (1, 1);

		db_prepare ("insert into raw_trans (id, acct_id, trans_id)"
			    "  values ($1, $2, $3)");
		col = 1;
		dbset_int (col++, id);
		dbset_str (col++, data.account_id);
		dbset_str (col++, data.fi_id);
		db_exec ();
	}
		
	diff = 0;

	if (strcmp (new_ref, db_ref) != 0)
		diff = 1;
	if (strcmp (new_name, db_name) != 0)
		diff = 1;
	if (strcmp (new_memo, db_memo) != 0)
		diff = 1;
	if (new_amount != db_amount)
		diff = 1;
	if (strcmp (new_posted, db_posted) != 0)
		diff = 1;
	if (strcmp (new_trans_type, db_trans_type) != 0)
		diff = 1;
	    
	if (diff) {
		db_prepare ("update raw_trans set"
			    "   ref = $1, name = $2, memo = $3,"
			    "   amount = $4, posted = $5,"
			    "   trans_type = $6"
			    " where id = $7");
		col = 1;
		dbset_str (col++, new_ref);
		dbset_str (col++, new_name);
		dbset_str (col++, new_memo);
		dbset_dbl (col++, "%.2f", new_amount);
		dbset_str (col++, new_posted);
		dbset_str (col++, new_trans_type);
		dbset_int (col++, id);
		db_exec ();
	}

	return (true);
}


void
read_ofx (char *filename)
{
	LibofxContextPtr ctx;

	ctx = libofx_get_new_context ();
	ofx_set_transaction_cb (ctx, cb_transaction, NULL);

	libofx_proc_file (ctx, filename, AUTODETECT);

}
