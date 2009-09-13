<?php

require ("common.php");
pstart ();

$arg_update = 0 + @$_REQUEST['update'];
$arg_delete = 0 + @$_REQUEST['delete'];
$arg_tag_id = 0 + @$_REQUEST['tag_id'];
$arg_return_to = trim (@$_REQUEST['return_to']);

if ($arg_delete == 1) {
	$body .= sprintf ("<form action='tags.php'>\n");
	$body .= "<input type='hidden' name='delete' value='2' />\n";
	$body .= sprintf ("<input type='hidden' name='tag_id' value='%d' />\n",
			  $arg_tag_id);
	$body .= "Confirm? <input type='submit' value='delete' />\n";
	$body .= "</form>\n";
	pfinish ();
}

if ($arg_delete == 2) {
	query ("delete from tags where tag_id = ?", $arg_tag_id);
	query ("update splits set tag_id = null where tag_id = ?", $arg_tag_id);
	flash ("Deleted");
	redirect ("tags.php");
}

if ($arg_update == 1) {
	get_tags ();

	foreach ($_REQUEST as $var => $val) {
		if (! ereg ("^tag([0-9]*)", $var, $parts))
			continue;

		$tag_id = 0 + $parts[1];
		$val = trim ($val);

		if ($tag_id == 0) {
			if ($val) {
				$tag_id = get_seq ();
				query ("insert into tags (tag_id, name)"
				       ." values (?,?)",
				       array ($tag_id, $val));
				continue;
			}
		}

		if ($val) {
			$old_name = @$tag_id_to_name[$tag_id];
			if (strcmp ($old_name, $val) != 0) {
				query ("update tags set name = ?"
				       ." where tag_id = ?",
				       array ($val, $tag_id));
			}
		}
	}

	flash ("Ok");

	if ($arg_return_to)
		redirect ($arg_return_to);

	redirect ("tags.php");
}

$body .= "<form action='tags.php'>\n";
$body .= "<input type='hidden' name='update' value='1' />\n";
$body .= sprintf ("<input type='hidden' name='return_to' value='%s' />\n",
		  h($arg_return_to));

$q = query ("select tag_id, name"
	    ." from tags"
	    ." order by name");

$rownum = 0;
$rows = "";
while (($r = fetch ($q)) != NULL) {
	$tag_id = 0 + $r->tag_id;
	$name = trim ($r->name);

	$rownum++;
	$o = odd_even ($rownum);
	$rows .= "<tr $o>\n";
	$rows .= "<td>";
	$rows .= sprintf ("<input type='text' name='tag%d' value='%s' />\n",
			  $tag_id, h($name));

	$t = sprintf ("tags.php?tag_id=%d&delete=1", $tag_id);
	$rows .= mklink ("delete", $t);
	$rows .= "</td>";
	$rows .= "</tr>\n";
}

$rownum++;
$o = odd_even ($rownum);
$rows .= "<tr $o>\n";
$rows .= "<td>";
$rows .= "<input type='text' name='tag0' value='' />\n";
$rows .= "</td>";
$rows .= "</tr>\n";

$rownum++;
$o = odd_even ($rownum);
$rows .= "<tr $o>\n";
$rows .= "<td>";
$rows .= "<input type='submit' value='Update' />\n";
$rows .= "</td>";
$rows .= "</tr>\n";

$cols = array ("Tag");
$body .= boxed_table ($cols, $rows);

$body .= "</form>\n";

pfinish ();

?>