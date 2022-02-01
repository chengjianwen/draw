<?php
$conn = new SQLite3('/tmp/draw/data/stroke.db');
$results = $conn->query("SELECT uuid, name, filename FROM MEDIA ORDER BY time DESC");
$data = array();
while ($result = $results->fetchArray(SQLITE3_ASSOC))
{
	$duration = intdiv(filesize($result['filename']), 16000) / 2;
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

echo json_encode ($data);
echo "\n";
