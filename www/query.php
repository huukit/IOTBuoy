<?php
$servername = "localhost";
$username = "poiju";
$password = "poiju";
$dbname = "Poijut";

$id = $_REQUEST['id'];

// Create connection
$conn = new mysqli($servername, $username, $password, $dbname);
// Check connection
if ($conn->connect_error) {
    die("Connection failed: " . $conn->connect_error);
}

$sql = "SELECT name FROM poijut WHERE id = $id";
$result = $conn->query($sql);

if ($result->num_rows > 0) {
   $row = $result->fetch_assoc();
   $name =  $row["name"];
}

$limit = $_REQUEST['limit'];

if ($limit > 0)
   $sql = "SELECT * FROM data WHERE id = $id ORDER BY timestamp DESC LIMIT $limit";
else
    $sql = "SELECT * FROM data WHERE id = $id";

$result = $conn->query($sql);

if ($result->num_rows > 0) {
    // output data of each row
    while($row = $result->fetch_assoc()) {
        echo $name . " " . $row["id"] 
                   . " " . $row["timestamp"]
                   . " " . $row["rssi"]
                   .  " " . $row["batterymV"]
                   . " " . $row["airTemp"]
                   . " " . $row["airHumidity"]
                   . " " . $row["airPressureHpa"];

                   for($i = 0; $i < $row["waterArrayCount"]; $i++){
                     $sensor = "waterTemp" . ($i + 1);
                     echo " ". ($i + 1) . ":" . $row[$sensor];
                   }
                   echo "<br>";
    }
} else {
    echo "0 results";
}
$conn->close();

?>
