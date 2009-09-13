<?php

require ("common.php");
pstart ();

$trans = array ();

$q = query ("select rt.tid, rt.name, rt.amount,"
	    ."      to_char (rt.posted, 'YYYY-MM-DD') as posted,"
	    ."      t.name2,"
	    ."      s.split_id, s.tag_id, s.split_amount"
	    ." from raw_trans rt"
	    ." left join trans t on (t.tid = rt.tid)"
	    ." left join splits s on (s.tid = rt.tid)"
	    ." order by rt.posted, rt.tid");
while (($r = fetch ($q)) != NULL) {
	$tid = 0 + $r->tid;
	$name = $r->name;
	$total_amount = 0 + $r->amount;
	$posted = trim ($r->posted);
	$name2 = trim ($r->name2);
	$split_id = 0 + $r->split_id;
	$tag_id = 0 + $r->tag_id;
	$split_amount = 0 + $r->split_amount;

	if (($tp = @$trans[$tid]) == NULL) {
		$tp = (object)NULL;
		$tp->tid = $tid;
		$tp->name = $name;
		$tp->total_amount = $total_amount;
		$tp->posted = $posted;
		$tp->name2 = $name2;
		$tp->splits = array ();

		$trans[$tid] = $tp;
	}

	if ($split_id) {
		$sp = (object)NULL;
		$sp->split_id = $split_id;
		$sp->tag_id = $tag_id;
		$sp->split_amount = $split_amount;

		$tp->splits[$split_id] = $sp;
	}
}
	    
foreach ($trans as $tp) {
	$acc = 0;
	foreach ($tp->splits as $sp) {
		$acc += $sp->split_amount;
	}
	if (abs ($acc - $tp->total_amount) > .001)
		$tp->unbalanced = 1;
}


$rownum = 0;
$rows = "";
foreach ($trans as $tp) {
	if ($tp->unbalanced) {
		$rownum++;
		$o = odd_even ($rownum);
		$rows .= "<tr $o>";

		$t = sprintf ("trans.php?tid=%d", $tp->tid);
		$rows .= sprintf ("<td>%s</td>\n", mklink ($tp->posted, $t));
		$rows .= sprintf ("<td>%s</td>\n", mklink ($tp->name2, $t));
		$rows .= sprintf ("<td>%s</td>\n", mklink ($tp->name, $t));
		$rows .= sprintf ("<td class='num'>%s</td>\n",
				  mklink ($tp->total_amount, $t));
		$rows .= "</tr>\n";
	}
}

if ($rows) {
	$body .= "<h1>Unbalanced transactions</h1>\n";

	$cols = array ("Posted", "Name2", "Name", "Amount");
	$body .= boxed_table ($cols, $rows);
}

pfinish ();

?>