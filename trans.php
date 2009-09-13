<?php

require ("common.php");
pstart ();

$arg_edit_split = 0 + @$_REQUEST['edit_split'];
$arg_delete = 0 + @$_REQUEST['delete'];
$arg_set_splits = 0 + @$_REQUEST['set_splits'];
$arg_update_trans = 0 + @$_REQUEST['update_trans'];

$arg_tid = 0 + @$_REQUEST['tid'];
$arg_split_id = 0 + @$_REQUEST['split_id'];

if ($arg_update_trans == 1) {
	$arg_name2 = trim (@$_REQUEST['name2']);

	$q = query ("select 0 from trans where tid = ?", $arg_tid);
	if (fetch ($q) == NULL) {
		query ("insert into trans (tid) values (?)", $arg_tid);
	}
	query ("update trans set name2 = ?"
	       ." where tid = ?",
	       array ($arg_name2, $arg_tid));

	flash ("Updated");

	$t = sprintf ("trans.php?tid=%d", $arg_tid);
	redirect ($t);
}

if ($arg_set_splits == 1) {
	$arg_set_splits_checkbox = 0 + @$_REQUEST['set_splits_checkbox'];
	$arg_tag_id = 0 + @$_REQUEST['tag_id'];
	$arg_name2 = trim (@$_REQUEST['name2']);

	if ($arg_set_splits_checkbox != 1) {
		flash ("You must click the checkbox to set these transactions");
		$t = sprintf ("trans.php?tid=%d", $arg_tid);
		redirect ($t);
	}

	$tids = explode (",", @$_REQUEST['tids']);

	foreach ($tids as $tid) {
		$q = query ("select amount"
			    ." from raw_trans"
			    ." where tid = ?",
			    $tid);
		if (($r = fetch ($q)) == NULL)
			continue;
		$amount = 0 + $r->amount;

		query ("delete from splits where tid = ?", $tid);
		query ("insert into splits"
		       ." (split_id, tid, tag_id, split_amount)"
		       ." values (nextval('seq'),?,?,?)",
		       array ($tid, $arg_tag_id, $amount));

		$q = query ("select 0"
			    ." from trans"
			    ." where tid = ?",
			    $tid);
		if (fetch ($q) == NULL) {
			query ("insert into trans (tid) values (?)", $tid);
		}
		query ("update trans set name2 = ? where tid = ?",
		       array ($arg_name2, $tid));
	}

	$t = sprintf ("trans.php?tid=%d", $arg_tid);
	redirect ($t);
}

if ($arg_delete == 1) {
	$body .= "<form action='trans.php'>\n";
	$body .= "<input type='hidden' name='delete' value='2' />\n";
	$body .= sprintf ("<input type='hidden' name='tid' value='%d' />\n",
			  $arg_tid);
	$body .= sprintf ("<input type='hidden'"
			  ." name='split_id' value='%d' />\n",
			  $arg_split_id);
	$body .= "Confirm? <input type='submit' value='delete' />\n";
	$body .= "</form>\n";
	pfinish ();
}

if ($arg_delete == 2) {
	query ("delete from splits where split_id = ?", $arg_split_id);

	flash ("deleted");

	$t = sprintf ("trans.php?tid=%d", $arg_tid);
	redirect ($t);
}

$q = query ("select acct_id, trans_id, ref, name, memo, amount,"
	    ."  to_char (posted, 'YYYY-MM-DD') as posted,"
	    ."  trans_type"
	    ." from raw_trans"
	    ." where tid = ?",
	    $arg_tid);
if (($r = fetch ($q)) == NULL)
	fatal ("invalid tid");
$db_acct_id = trim ($r->acct_id);
$db_trans_id = trim ($r->trans_id);
$db_ref = trim ($r->ref);
$db_name = trim ($r->name);
$db_memo = trim ($r->memo);
$db_total_amount = 0 + $r->amount;
$db_posted = trim ($r->posted);
$db_trans_type = trim ($r->trans_type);

$q = query ("select name2"
	    ." from trans"
	    ." where tid = ?",
	    $arg_tid);
if (($r = fetch ($q)) != NULL) {
	$db_name2 = trim ($r->name2);
} else {
	$db_name2 = "";
}

$splits = array ();
$q = query ("select split_id, tag_id, split_amount"
	    ." from splits"
	    ." where tid = ?"
	    ." order by split_id",
	    $arg_tid);

$acc = 0;
while (($r = fetch ($q)) != NULL) {
	$sp = (object)NULL;
	$sp->split_id = 0 + $r->split_id;
	$sp->tag_id = 0 + $r->tag_id;
	$sp->split_amount = 0 + $r->split_amount;

	$acc += $sp->split_amount;

	$splits[] = $sp;
	$splits_by_split_id[$sp->split_id] = $sp;
}

$unbalanced_amount = $db_total_amount - $acc;
if (abs ($unbalanced_amount) < .001)
	$unbalanced_amount = 0;

if ($arg_edit_split == 1) {
	foreach ($_REQUEST as $var => $val) {
		if (! ereg ("split_tag_id([0-9]*)", $var, $parts))
			continue;
		$split_id = 0 + $parts[1];

		$split_tag_id = 0 + $val;

		if ($split_tag_id == 0)
			continue;

		$k = sprintf ("split_amt%d", $split_id);
		$split_amount = @$_REQUEST[$k];

		if ($split_id == 0) {
			$split_id = get_seq ();
			query ("insert into splits (split_id, tid, "
			       ."     tag_id, split_amount)"
			       ." values (?,?,?,?)",
			       array ($split_id, $arg_tid,
				      $split_tag_id, $split_amount));
		} else if (($sp = @$splits[$split_id]) != NULL) {
			if ($sp->tag_id != $split_tag_id
			    || abs ($sp->amount - $split_amount) > .001) {
				query ("update splits set"
				       ."  tag_id = ?, split_amount = ?"
				       ." where split_id = ?",
				       $split_id);
			}
		}
	}

	flash ("Ok");

	$t = sprintf ("trans.php?tid=%d", $arg_tid);
	redirect ($t);
}

$body .= "<h1>Transaction details</h1>\n";

$body .= "<form action='trans.php'>\n";
$body .= "<input type='hidden' name='update_trans' value='1' />\n";
$body .= sprintf ("<input type='hidden' name='tid' value='%d' />\n", $arg_tid);

$body .= "<table class='twocol'>\n";

$body .= "<tr><th>Account ID</th><td>";
$body .= h($db_acct_id);
$body .= "</td></tr>\n";

$body .= "<tr><th>Trans ID</th><td>";
$body .= h($db_trans_id);
$body .= "</td></tr>\n";

$body .= "<tr><th>Ref</th><td>";
$body .= h($db_ref);
$body .= "</td></tr>\n";

$body .= "<tr><th>Name</th><td>";
$body .= h($db_name);
$body .= "</td></tr>\n";

$body .= "<tr><th>Memo</th><td>";
$body .= h($db_memo);
$body .= "</td></tr>\n";

$c = "";
if ($sp->split_amount < 0)
	$c = "class='red'";
$body .= "<tr><th>Amount</th><td $c>";
$body .= sprintf ("%.2f", $db_total_amount);
$body .= "</td></tr>\n";

$body .= "<tr><th>Posted</th><td>";
$body .= h($db_posted);
$body .= "</td></tr>\n";

$body .= "<tr><th>Trans type</th><td>";
$body .= h($db_trans_type);
$body .= "</td></tr>\n";

$body .= "<tr><th>Name2</th><td>";
$body .= sprintf ("<input type='text' name='name2' value='%s' />\n",
		  h($db_name2));
$body .= "</td></tr>\n";

$body .= "<tr><th></th><td><input type='submit' value='Update' /></td></tr>\n";

$body .= "</table>\n";
$body .= "</form>\n";

get_tags ();

function make_tag_selector ($varname, $curval = 0) {
	global $tags, $tag_name_to_id;

	$ret = "";

	$ret .= sprintf ("<select name='%s'>\n", $varname);
	$ret .= "<option value='0'>--select tag--</option>\n";
	foreach ($tags as $tag_name) {
		$tag_id = 0 + @$tag_name_to_id[$tag_name];

		$s = "";
		if ($curval == $tag_id)
			$s = "seleted='selected'";
		$ret .= sprintf ("<option value='%d' $s>%s</option>\n",
				  $tag_id, h($tag_name));
	}
	$ret .= "</select>\n";

	return ($ret);
}

$rownum = 0;
$rows = "";
foreach ($splits as $sp) {
	$rownum++;
	$o = odd_even ($rownum);
	$rows .= "<tr $o>";

	$tag_name = @$tag_id_to_name[$sp->tag_id];
	$t = sprintf ("tag.php?tag_id=%d", $sp->tag_id);
	$rows .= sprintf ("<td>%s</td>", mklink ($tag_name, $t));

	$c = "";
	if ($sp->split_amount < 0)
		$c = "class='red'";
	$rows .= sprintf ("<td $c>%.2f</td>",  $sp->split_amount);

	$t = sprintf ("trans.php?tid=%d&split_id=%d&delete=1",
		      $arg_tid, $sp->split_id);
	$rows .= sprintf ("<td>%s</td>", mklink ("delete", $t));

	$rows .= "</tr>\n";
}

if ($unbalanced_amount) {
	$body .= "<p class='red'>Unbalanced splits</p>\n";
}

$body .= "<form action='trans.php'>\n";
$body .= "<input type='hidden' name='edit_split' value='1' />\n";
$body .= sprintf ("<input type='hidden' name='tid' value='%d' />\n",
		  $arg_tid);


$rownum++;
$o = odd_even ($rownum);
$rows .= "<tr $o>";
$rows .= "<td>";
$rows .= make_tag_selector ("split_tag_id0", 0);
$rows .= "</td>";
$rows .= sprintf ("<td><input type='text'"
		  ." name='split_amt0' value='%.02f' /></td>",
		  $unbalanced_amount);

$rows .= "<td></td>";
$rows .= "</tr>\n";

$rownum++;
$o = odd_even ($rownum);
$rows .= "<tr $o>";

$rt = sprintf ("trans.php?tid=%d", $arg_tid);
$t = sprintf ("tags.php?return_to=%s", urlencode ($rt));
$rows .= sprintf ("<td>%s</td>", mklink ("add tag", $t));
$rows .= "<td>";
$rows .= "<input type='submit' value='Update' />\n";
$rows .= "</td>";
$rows .= "<td></td>";
$rows .= "</tr>\n";

$cols = array ("Tag", "Amount", "Op");
$body .= boxed_table ($cols, $rows);

$body .= "</form>\n";

$elts = array ();

$q = query ("select rt.tid, rt.name, rt.amount,"
	    ."      to_char (rt.posted, 'YYYY-MM-DD') as posted,"
	    ."      t.name2, s.tag_id"
	    ." from raw_trans rt"
	    ." left join trans t on (t.tid = rt.tid)"
	    ." left join splits s on (s.tid = rt.tid)");
$seen = array ();
while (($r = fetch ($q)) != NULL) {
	$elt = (object)NULL;

	if (isset ($seen[$r->tid]))
		continue;
	$seen[$r->tid] = 1;

	$elt->tid = 0 + $r->tid;
	$elt->name = trim ($r->name);
	$elt->amount = 0 + $r->amount;
	$elt->posted = trim ($r->posted);
	$elt->name2 = trim ($r->name2);
	$elt->tag_id = 0 + $r->tag_id;

	$elt->score = similar_text ($r->name, $db_name);
	$elts[] = $elt;
}

function elt_cmp ($a, $b) {
	if (($rc = $b->score - $a->score) != 0)
		return ($rc);

	if (($rc = strcmp ($a->posted, $b->posted)) != 0)
		return ($rc);

	return (0);
}

usort ($elts, "elt_cmp");

$score_limit = $elts[0]->score - 1;

$similar_tids = array ();

$n = count ($elts);
$rownum = 0;
$rows = "";
for ($i = 0; $i < $n; $i++) {
	$elt = $elts[$i];

	if ($elt->score < $score_limit)
		break;

	if ($elt->tid != $arg_tid)
		$similar_tids[] = $elt->tid;

	$rownum++;
	$o = odd_even ($rownum);
	$rows .= "<tr $o>";

	$rows .= sprintf ("<td>%s</td>", h($elt->posted));

	$t = sprintf ("trans.php?tid=%d", $elt->tid);
	$rows .= sprintf ("<td>%s</td>\n", mklink ($elt->name2, $t));

	$t = sprintf ("trans.php?tid=%d", $elt->tid);
	$rows .= sprintf ("<td>%s</td>\n", mklink ($elt->name, $t));


	$text = sprintf ("%.2f", $elt->amount);
	$rows .= sprintf ("<td class='num'>%s</td>\n", mklink ($text, $t));

	$tag_name = @$tag_id_to_name[$elt->tag_id];
	$t = sprintf ("tag.php?tag_id=%d", $elt->tag_id);
	$rows .= sprintf ("<td>%s</td>", mklink ($tag_name, $t));
	$rows .= "</tr>\n";
}

$body .= "<h2>Similar transactions</h2>\n";

if (count ($splits) == 1 && count ($similar_tids) > 0) {
	$sp = $splits[0];

	$body .= "<form action='trans.php'>\n";
	$body .= "<input type='hidden' name='set_splits' value='1' />\n";
	$body .= sprintf ("<input type='hidden' name='tid' value='%d' />\n",
			  $arg_tid);
	$body .= sprintf ("<input type='hidden' name='name2' value='%s' />\n",
			  h($db_name2));

	$val = join (",", $similar_tids);
	$body .= sprintf ("<input type='hidden' name='tids' value='%s' />\n",
			  h($val));
	$body .= sprintf ("<input type='hidden' name='tag_id' value='%d' />\n",
			  $sp->tag_id);

	$body .= "<input type='checkbox'"
		." name='set_splits_checkbox' value='1' />\n";

	$tag_name = $tag_id_to_name[$sp->tag_id];
	$text = sprintf ("Set %d transactions to %s",
			 count ($similar_tids), $tag_name);
	$body .= sprintf ("<input type='submit' value='%s' />\n",
			  h($text));

	$body .= "</form>\n";
}

$cols = array ("Posted", "Name2", "Name", "Amount", "Tag");
$body .= boxed_table ($cols, $rows);

pfinish ();

?>
