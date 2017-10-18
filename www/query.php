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

    // Information query, basic
    if ($info == 'info'){
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
    
    // Information query, with last measurements.
    elseif ($info == 'fullinfo'){
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
                $rowarr = [];
                $rowarr["id"] = $row["id"];
                $rowarr["name"] = $row["name"];
                $rowarr["position"] = $row["position"];
                $rowarr["date_added"] = $row["date_added"];
                
                $id = $row["id"];
                $sql = "SELECT * FROM data WHERE id = $id ORDER BY timestamp DESC LIMIT 1";
                $dataresult = $conn->query($sql);
                
                if ($result->num_rows > 0){
                    while($row = $dataresult->fetch_assoc()) {
                        $rowarr["timestamp"] = $row["timestamp"];
                        $rowarr["rssi"] = $row["rssi"];
                        $rowarr["batterymV"] = $row["batterymV"];
                        $rowarr["airTemp"] = $row["airTemp"];
                        $rowarr["airHumidity"] = $row["airHumidity"];
                        $rowarr["airPressureHpa"] = $row["airPressureHpa"];
                        $rowarr["waterArrayCount"] = $row["waterArrayCount"];
                        $rowarr["waterTemp1"] = $row["waterTemp1"];
                        $rowarr["waterTemp2"] = $row["waterTemp2"];
                        $rowarr["waterTemp3"] = $row["waterTemp3"];
                        $rowarr["waterTemp4"] = $row["waterTemp4"];
                        $rowarr["waterTemp5"] = $row["waterTemp5"];
                        $rowarr["waterTemp6"] = $row["waterTemp6"];
                    }
                }
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