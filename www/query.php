<!DOCTYPE html>
<html lang = "en-US">
    <head>
        <meta charset = "UTF-8">
        <title>Query</title>
    </head>
    <body>
        <?php
        $servername = "localhost";
        $username = "poiju";
        $password = "poiju";
        $dbname = "Poijut";

        // Data or information
        $info = filter_input(INPUT_GET, 'info');

        // ID and datatype.
        $id = filter_input(INPUT_GET, 'id');
        $type = filter_input(INPUT_GET, 'datatype');

        // Limits, etc.
        $limit = filter_input(INPUT_GET, 'limit');
        $sdate = filter_input(INPUT_GET, 'sdate');
        $edate = filter_input(INPUT_GET, 'edate');

        // Create connection
        $conn = new mysqli($servername, $username, $password, $dbname);
        // Check connection
        if ($conn->connect_error) {
            die("Connection failed: " . $conn->connect_error);
        }

        // Information query
        if ($info == 'true'){
            if(empty($id)){
                $sql = "SELECT * FROM poijut";
            }
            else{
                $sql = "SELECT * FROM poijut WHERE id = $id";
            }
            $result = $conn->query($sql);
            $data = array();

            if ($result->num_rows > 0) {
                 while($row = $result->fetch_assoc()) {
                     $rowarr["id"] = $row["id"];
                     $rowarr["name"] = $row["name"];
                     $rowarr["position"] = $row["position"];
                     $rowarr["date_added"] = $row["date_added"];
                     array_push($data, $rowarr);
                 }
            }
            echo json_encode($data);
        }
        
        // Data query
        else{
            $sql = "SELECT timestamp, $type FROM data WHERE id = $id";
            
            if(!empty($sdate)){
                $sql = $sql . " AND timestamp >= '$sdate'";
            }
            
            if(!empty($edate)){
                $sql = $sql . " AND timestamp <= '$edate'";
            }
            
            // Limit active.
            if(!empty($limit))  {
                $sql = $sql . " ORDER BY timestamp DESC LIMIT $limit";
            }

            echo $sql;
            $result = $conn->query($sql);
            $data = array();
            if ($result->num_rows > 0) {
                $arr = array();
                while($row = $result->fetch_assoc()) {
                    array_push($data, array($row["timestamp"] => $row["$type"]));
                 }
                 echo json_encode($data);
            } else {
                echo "[]";
            }
        }
        $conn->close();
        ?>
</body>
</html>