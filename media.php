<?php
$conn = new SQLite3('/tmp/draw/data/stroke.db');
//$result = $conn->query("SELECT json_group_array (datetime(time, 'localtime')) FROM (SELECT time FROM MEDIA ORDER BY time DESC)");
$results = $conn->query("SELECT datetime(time, 'localtime') AS time, filename FROM MEDIA ORDER BY time DESC");
$data = array();
while ($result = $results->fetchArray(SQLITE3_ASSOC))
{
	$duration = intdiv(filesize($result['filename']), 16000);
	unset($result['filename']);
	if ($duration < 60)
	  $result['duration'] = "less 1 minute";
	else if ($duration < 3600)
		$result['duration'] = sprintf ("%2d:%02d",
			intdiv($duration, 60),
			$duration % 60);
	else if ($duration < 86400)
		$result['duration'] = sprintf ("%d:%2d:%02d",
			intdiv($duration, 3600),
			intdiv($duration % 3600, 60),
			$duration % 3600 % 60);
	else
		$result['duration'] = sprintf ("%dd %2d:%2d:%02d",
			intdiv($duration, 86400),
			intdiv($duration % 86400, 3600),
			intdiv($duration % 86400 % 3600, 60),
			$duration % 86400 % 3600 % 60);
	$data[] = $result;
}

//var_dump ($data);
echo json_encode ($data);
echo "\n";
