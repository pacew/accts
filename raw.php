<?php

require ("common.php");
pstart ();

$q = query ("select tid, acct_id, trans_id, ref, "
	    ."  name, memo, amount,"
	    ."  to_char (posted, 'YYYY-MM-DD') as posted, trans_type"
	    ." from raw_trans"
	    ." order by posted, tid");
$rownum = 0;
$rows = "";
while (($r = fetch ($q)) != NULL) {
	$rownum++;
	$o = odd_even ($rownum);
	$rows .= "<tr $o>";

	$t = sprintf ("trans.php?tid=%d", $r->tid);
	
	$rows .= sprintf ("<td>%s</td>\n", mklink ($r->acct_id, $t));
	$rows .= sprintf ("<td>%s</td>\n", mklink ($r->trans_id, $t));
	$rows .= sprintf ("<td>%s</td>\n", mklink ($r->ref, $t));
	$rows .= sprintf ("<td>%s</td>\n", mklink ($r->name, $t));
	$rows .= sprintf ("<td>%s</td>\n", mklink ($r->amount, $t));
	$rows .= sprintf ("<td>%s</td>\n", mklink ($r->posted, $t));
	$rows .= sprintf ("<td>%s</td>\n", mklink ($r->trans_type, $t));
	$rows .= "</tr>\n";
}

$cols = array ("Acct", "Trans", "Ref", "Name", "Amount", "Posted", "Type");
$body .= boxed_table ($cols, $rows);

pfinish ();

?>
