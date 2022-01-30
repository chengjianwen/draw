<?php
$conn = new SQLite3('/tmp/draw/data/stroke.db');
$result = $conn->query("SELECT json_group_array (json_object('nick', nick, 'time', datetime(time, 'localtime'))) FROM ONLINE;");
echo $result->fetchArray()[0];
