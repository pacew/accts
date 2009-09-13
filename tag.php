<?php

require ("common.php");
pstart ();

$arg_tag_id = 0 + @$_REQUEST['tag_id'];

$q = query ("select s.split_id, s.tid, s.split_amount,"
	    ."      rt.acct_id, rt.name"
	    ." from splits s"
	    ." left join raw_trans rt on (rt.tid = s.tid)"
	    ." where s.tag_id = ?"
	    ." order by s.tid, s.split_id",
	    $arg_tag_id);

$rownum = 0;
$rows = "";
while (($r = fetch ($q)) != NULL) {
	$split_id = 0 + $r->split_id;
	$tid = 0 + $r->tid;
	$split_amount = 0 + $r->split_amount;
	$acct_id = trim ($r->acct_id);
	$name = trim ($r->name);

	$rownum++;
	$o = odd_even ($rownum);
	$rows .= "<tr $o>";

	$t = sprintf ("trans.php?tid=%d", $tid);

	$rows .= sprintf ("<td>%s</td>\n", h($acct_id));
	$rows .= sprintf ("<td>%s</td>\n", mklink ($name, $t));
	$rows .= sprintf ("<td>%.2f</td>\n", $split_amount);

	$rows .= "</tr>\n";
}	
	
$cols = array ("Account", "Name", "Amount");
$body .= boxed_table ($cols, $rows);

pfinish ();
?>