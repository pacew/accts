#include <opus/opus.h>
#include <openssl/sha.h>

#include "pglib.h"

void read_qif (char *filename);

void
usage (void)
{
	fprintf (stderr, "usage: import-qif\n");
	exit (1);
}

int
main (int argc, char **argv)
{
	int c;
	char *filename;

	filename = "ccard.qif";

	while ((c = getopt (argc, argv, "")) != EOF) {
		switch (c) {
		default:
			usage ();
		}
	}

	if (optind < argc)
		filename = argv[optind++];

	if (optind != argc)
		usage ();

	db_init3 ("dbname=accts");

	read_qif (filename);

	db_trans_step (1);

	return (0);
}

char *
trim (char *buf)
{
	int len;

	len = strlen (buf);
	while (len > 0 && isspace (buf[len-1]))
		buf[--len] = 0;

	return (buf);
}

double
parse_number (char *str)
{
	char buf[1000];
	char *inp, *outp;
	double val;
	char *endp;

	if (strlen (str) > 100)
		fatal ("invalid number");

	outp = buf;
	for (inp = str; *inp; inp++) {
		if (*inp != ',')
			*outp++ = *inp;
	}
	*outp = 0;

	val = strtod (buf, &endp);
	if (*endp != 0)
		fatal ("invalid number %s / %s\n", buf, endp);

	return (val);
}
	
int linenum;

char acct_id[1000];

struct trans {
	struct trans *next;

	int input_seq;

	char *ref;
	char *posted;
	char *name;
	char *memo;
	char *trans_type;
	double amount;

	unsigned char hash[SHA_DIGEST_LENGTH];
	int dayseq;

	char *trans_id;
};

struct trans *trans_list, **trans_list_tailp;
int ntrans;

struct trans *
make_trans (void)
{
	struct trans *tp;

	tp = xcalloc (1, sizeof *tp);
	tp->ref = "";
	tp->posted = "";
	tp->name = "";
	tp->memo = "";
	tp->trans_type = "";
	
	return (tp);
}

void
update_db (struct trans *tp)
{
	int row, col, nrows;
	char const *db_ref;
	char const *db_name;
	char const *db_memo;
	char const *db_posted;
	char const *db_trans_type;
	double db_amount;
	int id;
	int diff;

	db_trans_step (0);
	db_prepare ("select id, ref, name, memo,"
		    "       amount, posted, trans_type"
		    " from raw_trans"
		    " where acct_id = $1 and trans_id = $2");
	col = 1;
	dbset_str (col++, acct_id);
	dbset_str (col++, tp->trans_id);
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
		dbset_str (col++, acct_id);
		dbset_str (col++, tp->trans_id);
		db_exec ();
	}
		
	diff = 0;

	if (strcmp (tp->ref, db_ref) != 0)
		diff = 1;
	if (strcmp (tp->name, db_name) != 0)
		diff = 1;
	if (strcmp (tp->memo, db_memo) != 0)
		diff = 1;
	if (tp->amount != db_amount)
		diff = 1;
	if (strcmp (tp->posted, db_posted) != 0)
		diff = 1;
	if (strcmp (tp->trans_type, db_trans_type) != 0)
		diff = 1;
	    
	if (diff) {
		db_prepare ("update raw_trans set"
			    "   ref = $1, name = $2, memo = $3,"
			    "   amount = $4, posted = $5,"
			    "   trans_type = $6"
			    " where id = $7");
		col = 1;
		dbset_str (col++, tp->ref);
		dbset_str (col++, tp->name);
		dbset_str (col++, tp->memo);
		dbset_dbl (col++, "%.2f", tp->amount);
		dbset_str (col++, tp->posted);
		dbset_str (col++, tp->trans_type);
		dbset_int (col++, id);
		db_exec ();
	}
}

void
print_trans (FILE *f, struct trans *tp)
{
	fprintf (f, "trans %s %s %s %s %s %.14f\n",
		 tp->trans_id, tp->posted, tp->name, tp->memo,
		 tp->ref, tp->amount);
}


int
hash_cmp (void const *raw1, void const *raw2)
{
	struct trans const *arg1 = *(struct trans * const *)raw1;
	struct trans const *arg2 = *(struct trans * const *)raw2;
	int rc;

	if ((rc = memcmp (arg1->hash, arg2->hash, SHA_DIGEST_LENGTH)) != 0)
		return (rc);

	return (arg1->input_seq - arg2->input_seq);
}

struct trans **trans_arr;

void
process_trans (void)
{
	struct trans *tp;
	int idx;
	char buf[1000];
	SHA_CTX ctx;
	char *outp;

	trans_arr = xcalloc (ntrans, sizeof *trans_arr);
	for (tp = trans_list, idx = 0; tp; tp = tp->next, idx++)
		trans_arr[idx] = tp;

	for (tp = trans_list; tp; tp = tp->next) {
		SHA1_Init (&ctx);
		SHA1_Update (&ctx, tp->ref, strlen (tp->ref));
		SHA1_Update (&ctx, tp->posted, strlen (tp->posted));
		SHA1_Update (&ctx, tp->name, strlen (tp->name));
		SHA1_Update (&ctx, tp->memo, strlen (tp->memo));
		sprintf (buf, "%.2f", tp->amount);
		SHA1_Update (&ctx, buf, strlen (buf));
		SHA1_Final (tp->hash, &ctx);
	}

	qsort (trans_arr, ntrans, sizeof *trans_arr, hash_cmp);
	
	for (idx = 0; idx < ntrans - 1; idx++) {
		if (memcmp (trans_arr[idx]->hash,
			    trans_arr[idx+1]->hash,
			    SHA_DIGEST_LENGTH) == 0) {
			trans_arr[idx+1]->dayseq = trans_arr[idx]->dayseq + 1;
		}
	}

	for (tp = trans_list; tp; tp = tp->next) {
		outp = buf;
		for (idx = 0; idx < 4; idx++) {
			sprintf (outp, "%02x", tp->hash[idx]);
			outp += 2;
		}
		sprintf (outp, "-%02d", tp->dayseq);
		tp->trans_id = xstrdup (buf);
	}

	for (tp = trans_list; tp; tp = tp->next)
		print_trans (stdout, tp);

	for (tp = trans_list; tp; tp = tp->next)
		update_db (tp);
}




void
read_qif (char *filename)
{
	FILE *f;
	char buf[1000];
	char *p;
	int month, mday, year;
	struct trans *trans;
	char obuf[1000];

	if ((f = fopen (filename, "r")) == NULL)
		fatal ("can't open %s\n", filename);

	if (fgets (buf, sizeof buf, f) == NULL)
		fatal ("unexpected EOF\n");
	trim (buf);

	if (sscanf (buf, "!Type:%s", acct_id) != 1)
		fatal ("%d: invalid QIF: %s\n", linenum, buf);

	linenum = 0;
	trans = NULL;

	trans_list = NULL;
	trans_list_tailp = &trans_list;

	while (1) {
		if (fgets (buf, sizeof buf, f) == NULL)
			break;
		linenum++;

		if (trans == NULL) {
			trans = make_trans ();
			trans->input_seq = ntrans;
		}

		p = trim (buf);
		switch (*p) {
		case 0:
			break;

		case '^':
			*trans_list_tailp = trans;
			trans_list_tailp = &trans->next;
			ntrans++;
			trans = NULL;
			break;
		case 'D':
			if (sscanf (buf, "D%d/%d/%d",
				    &month, &mday, &year) != 3) {
				fatal ("%d: invalid date: %s\n", linenum, buf);
			}

			if (year < 1900)
				fatal ("%d: invalid year %s\n", linenum, buf);
			
			sprintf (obuf, "%04d-%02d-%02d 12:00:00",
				 year, month, mday);
			trans->posted = xstrdup (obuf);
			break;
		case 'T':
			trans->amount = parse_number (buf + 1);
			break;
		case 'N': // check number
			trans->ref = xstrdup (buf + 1);
			break;
		case 'P': // payee
			trans->name = xstrdup (buf + 1);
			break;
		case 'M': // memo
			trans->memo = xstrdup (buf + 1);
			break;
		default:
			fatal ("%d: unknown opcode: %s\n", linenum, buf);
			break;
		}
	}

	process_trans ();
}
