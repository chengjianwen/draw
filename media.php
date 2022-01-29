<?php
$conn = new SQLite3('/tmp/draw/data/stroke.db');
$result = $conn->query('SELECT json_group_array (time) FROM MEDIA;');
echo $result->fetchArray()[0];
