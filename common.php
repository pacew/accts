<?php

$dbname = "wtrans";


require ("DB.php");

session_start ();

function ckerr ($str, $obj, $aux = "")
{
	$ret = "";
	if (DB::isError ($obj)) {
		$ret = sprintf ("<p>DBERR %s: %s<br />\n",
				h($str),
				h($obj->getMessage ()),
				"");
		
		/* these fields might have db connect passwords */
		$ret .= h($obj->userinfo);
		if ($aux != "")
			$ret .= sprintf ("<p>aux info: %s</p>\n",
					 h($aux));
		$ret .= "</p>";
		echo ($ret);
		echo ("domain_name "
		      . htmlentities ($_SERVER['HTTP_HOST']) . "<br/>");
		echo ("request "
		      . htmlentities ($_SERVER['REQUEST_URI']) . "<br/>");
		var_dump ($_SERVER);
		error ();
		exit ();
	}
}

function query ($stmt, $arr = NULL) {
	global $login_id, $_SERVER;
	global $db;

	if (is_string ($stmt) == 0) {
		echo ("wrong type first arg to query");
		error ();
		exit ();
	}

	$q = $db->query ($stmt, $arr);
	ckerr ($stmt, $q);
	
	return ($q);
}

function fetch ($q) {
	return ($q->fetchRow (DB_FETCHMODE_OBJECT));
}

$db1 = new DB;
$db = $db1->connect ("pgsql://apache@/$dbname");
ckerr ("connect/local can't connect to database", $db);

query ("begin transaction");


function h($val) {
	return (htmlentities ($val, ENT_QUOTES, 'UTF-8'));
}

function do_commit () {
	query ("end transaction");
}

function redirect ($t) {
	ob_clean ();
	do_commit ();
	header ("Location: $t");
	exit ();
}

function pstart ($title = "") {
	global $head, $before_container, $body, $after_container, $dbname;

	$head = "";
	$before_container = "";
	$body = "";
	$after_container = "";

	ob_start ();

	$head .= "<meta http-equiv='Content-Type' content='text/html;"
		." charset=utf-8' />\n";

	if ($title == "")
		$title = $dbname;
	$head .= sprintf ("<title>%s</title>", h($title));

	$head .= "<link rel='stylesheet' type='text/css'"
		." href='style.css' />\n";

	$before_container .= make_nav ();

	if (@$_SESSION['flash'] != "") {
		$body .= "<div class='the_flash'>\n";
		$body .= $_SESSION['flash'];
		$body .= "</div>\n";
		$_SESSION['flash'] = "";
	}

}

function flash ($str) {
	$_SESSION['flash'] = @$_SESSION['flash'] . $str;
}

function pfinish () {
	global $head, $before_container, $body, $after_container;

	echo ("<!DOCTYPE html PUBLIC '-//W3C//DTD XHTML 1.0"
	      ." Transitional//EN'"
	      ." 'http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd'>\n");
	
	echo ("<html xmlns='http://www.w3.org/1999/xhtml'>\n");
	echo ("<head>\n");
	echo   ($head);
	echo ("</head>\n");

	echo ("<body>\n");
	echo   ($before_container);
	echo   ("<div class='container'>\n");
	echo     ($body);
	echo   ("</div>\n");
	echo   ($after_container);
	echo ("</body>\n");
	echo ("</html>\n");
	do_commit ();
	exit ();
}

function mklink ($text, $target) {
	if (trim ($text) == "")
		return ("&nbsp;");
	if (trim ($target) == "")
		return (h($text));
	return (sprintf ("<a href='%s'>%s</a>",
			 h($target), h($text)));
}

function odd_even ($x) {
	if ($x & 1)
		return ("class='odd'");
	return ("class='even'");
}

function boxed_table ($cols, $rows) {
	$ret = "";

	$ncols = count ($cols);

	$ret .= "<table class='boxed'>\n";

	$ret .= "<tr class='heading'>";
	for ($i = 0; $i < $ncols; $i++) {
		$c = "mth";
		if ($i == 0)
			$c = "lth";
		if ($i == $ncols - 1)
			$c = "rth";
		$ret .= "<th class='$c'>";
		$ret .= h($cols[$i]);
		$ret .= "</th>\n";
	}

	$ret .= "</tr>\n";

	$ret .= $rows;

	$ret .= "</table>\n";

	return ($ret);
}

function make_nav () {
	$ret = "";

	$ret .= "<div class='nav'>\n";
	$ret .= "<ul class='nav_main'>\n";
	$ret .= sprintf ("<li>%s</li>\n", mklink ("raw", "wtrans.php"));
	$ret .= sprintf ("<li>%s</li>\n", mklink ("splits", "splits.php"));
	$ret .= sprintf ("<li>%s</li>\n", mklink ("tags", "tags.php"));
	$ret .= "</ul>\n";
	$ret .= "</div>\n";
	$ret .= "<div style='clear:both'></div>\n";

	return ($ret);
}

function get_seq () {
	$q = query ("select nextval('seq') as seq");
	$r = fetch ($q);
	return (0 + $r->seq);
}

function get_tags () {
	global $tags, $tag_name_to_id, $tag_id_to_name;

	if (isset ($tags))
		return;

	$tags = array ();
	$tag_name_to_id = array ();
	$tag_id_to_name = array ();

	$q = query ("select tag_id, name"
		    ." from tags"
		    ." order by name");
	while (($r = fetch ($q)) != NULL) {
		$tag_id = 0 + $r->tag_id;
		$name = trim ($r->name);

		$tags[] = $name;
		$tag_name_to_id[$name] = $tag_id;
		$tag_id_to_name[$tag_id] = $name;
	}
}

function fatal ($str) {
	echo ("fatal error: ");
	echo ($str);
	echo (@$_SESSION['flash']);
	$_SESSION['flash'] = "";
	exit ();
}

?>
