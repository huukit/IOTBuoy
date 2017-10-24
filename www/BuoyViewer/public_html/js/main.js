/**
 * <copyright>
 * Copyright (c) 2017: Tuomas Huuki
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2chrome-extension://mpognobbkildjkofajifpdfhcoklimli/components/startpage/startpage.html?section=Speed-dials&activeSpeedDialIndex=0
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Lesser GNU General Public License for more details.
 *
 * You should have received a copy of the (Lesser) GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * </copyright>
 */

var myChart = null;
var markers = [];
var map = null;
var rTimer = null;
var nodes = null;

const queryUrl = "http://ohtakari.dyndns.org:8080/query.php";

function loadRealtime(){
    $('.loader').show();
    data = "info=fullinfo";
    $.getJSON(queryUrl, data, function (result, status) {
        var nodelist = "<table><tr>" 
                        + "<th>ID</th>"
                        + "<th>Name</th>"
                        + "<th>Lat °</th>"
                        + "<th>Lon °</th>"
                        + "<th>Date Added</th>"
                        + "<th>Last Seen</th></tr>";
                        
        var nodedata = "<table><tr>" 
                        + "<th>ID</th>"
                        + "<th>RSSI (dBm)</th>"
                        + "<th>Battery (mV)</th>"
                        + "<th>Air Temp (°C)</th>"
                        + "<th>Air Humidity (%RH)</th>"
                        + "<th>Air Pressure (hPa)</th>"
                        + "<th>Water probes (n)</th>"
                        + "<th>Water (1) (°C)</th>"
                        + "<th>Water (2) (°C)</th>"
                        + "<th>Water (3) (°C)</th>"
                        + "<th>Water (4) (°C)</th>"
                        + "<th>Water (5) (°C)</th>"
                        + "<th>Water (6) (°C)</th></tr>";
                
        for (i = 0; i < result.length; i++) {
            nodes = [];
            nodes.push(result[i].id);
            
            var node = "<tr>";
            node += "<td>" + result[i].id + "</td>";
            node += "<td>" + result[i].name + "</td>";
            var lati, long;
            lati = parseFloat(result[i].position.split(",")[0]);
            long = parseFloat(result[i].position.split(",")[1].trim());
            node += "<td>" + lati + "</td>";
            node += "<td>" + long + "</td>";
            node += "<td>" + result[i].date_added + "</td>";
            node += "<td>" + result[i].timestamp + "</td>";
            node += "</tr>";
            nodelist += node;
            
            node = "<tr>";
            node += "<td>" + result[i].id + "</td>";
            node += "<td>" + result[i].rssi + "</td>";
            node += "<td>" + result[i].batterymV + "</td>";
            node += "<td>" + result[i].airTemp + "</td>";
            node += "<td>" + result[i].airHumidity + "</td>";
            node += "<td>" + result[i].airPressureHpa + "</td>";
            node += "<td>" + result[i].waterArrayCount + "</td>";
            node += "<td>" + result[i].waterTemp1 + "</td>";
            node += "<td>" + result[i].waterTemp2 + "</td>";
            node += "<td>" + result[i].waterTemp3 + "</td>";
            node += "<td>" + result[i].waterTemp4 + "</td>";
            node += "<td>" + result[i].waterTemp5 + "</td>";
            node += "<td>" + result[i].waterTemp6 + "</td>";
            node += "</tr>";
            nodedata += node;
        }

        nodelist += "</table>";
        nodedata += "</table>";
        console.log(nodelist);

        document.getElementById('realTimeNodes').innerHTML = nodelist;
        document.getElementById('realTimeData').innerHTML = nodedata;
        
        if(rTimer === null)setRefresh();
        $('.loader').hide();
    })
            .error(function (xhr, textStatus) {
                alert(textStatus + " " + xhr.readyState);
            });
}

function setRefresh(){
    console.log(document.getElementById("interval").value);
    rtime = document.getElementById("interval").value;
    var refreshtime;
    
    switch (rtime) {
        case "stopped":
            refreshtime = 0;
            break;
        case "1s":
            refreshtime = 1000;
            break;

        case "10s":
            refreshtime = 10000;
            break;

        case "1m":
            refreshtime = 60000;
            break;
        default:    
            break;
    }
    
    if(refreshtime === 0)
        clearInterval(rTimer);
    else{
        rTimer = setInterval(loadRealtime, refreshtime);
    }
}

function loadNodes() {
    document.getElementById('nodes').innerHTML = "Kebab";

    var mapOptions = {
        center: new google.maps.LatLng(64.088427, 23.407632),
        zoom: 10,
        mapTypeId: google.maps.MapTypeId.MAP
    };

    map = new google.maps.Map(document.getElementById("mymap"), mapOptions);

    // Get list and update all data.
    data = "info=info";
    $.getJSON(queryUrl, data, function (result, status) {
        var nodelist = "Select node:<select id =\"nodeId\" onchange = \"updateGraph()\">";
        console.log("Updating node list.");
        
        for (i = 0; i < result.length; i++) {
            var node = "<option value = \"" + result[i].id + "\">" + result[i].name + "</option>";
            var lati, long;
            lati = parseFloat(result[i].position.split(",")[0]);
            long = parseFloat(result[i].position.split(",")[1].trim());
            var marker = new google.maps.Marker({
                position: {lat: lati, lng: long},
                map: map,
                label: result[i].name
            });
            
            markers[result[i].id] = marker;
            console.log(node);
            console.log("Position: " + result[i].position);
            console.log("lat: " + lati + " lon: " + long);
            nodelist += node;
        }
        nodelist += "</select>";
        console.log(nodelist);

        document.getElementById('nodes').innerHTML = nodelist;
        $('.loader').hide();
    })
            .error(function (xhr, textStatus) {
                alert(textStatus + " " + xhr.readyState);
            });
}

function onNodeClicked(id, start, end) {
    var datatype = document.getElementById('dataType').value;
    console.log("Node:" + id + " clicked. Data type: " + datatype);

    // Relocate map.
    var latLng = markers[id].getPosition(); // returns LatLng object
    map.setCenter(latLng);

    // Get list and update graph.
    data = "id=" + id + "&datatype=" + datatype + "&sdate=" + start + "&edate=" + end;
    $.getJSON(queryUrl, data, function (result, status) {
        console.log("Data received for id: " + id);

        var datas = [], dates = [];
        const maxGraphPoints = 500;
        if (result.length >= maxGraphPoints) {
            var step = Math.ceil(result.length / maxGraphPoints);
            for (i = 0; i < result.length; i += step) {
                var date = Object.keys(result[i])[0];
                dates.push(date);
                datas.push(result[i][date]);
            }
        } else {
            for (i = 0; i < result.length; i++) {
                var date = Object.keys(result[i])[0];
                dates.push(date);
                datas.push(result[i][date]);
            }
        }

        var ctx = document.getElementById("myChart");
        var label = document.getElementById('dataType').value;
        
        myChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: dates,
                datasets: [{
                        label: label,
                        data: datas,
                        backgroundColor: [
                            'rgba(255, 99, 132, 0.2)',
                            'rgba(54, 162, 235, 0.2)',
                            'rgba(255, 206, 86, 0.2)',
                            'rgba(75, 192, 192, 0.2)',
                            'rgba(153, 102, 255, 0.2)',
                            'rgba(255, 159, 64, 0.2)'
                        ],
                        borderColor: [
                            'rgba(255,99,132,1)',
                            'rgba(54, 162, 235, 1)',
                            'rgba(255, 206, 86, 1)',
                            'rgba(75, 192, 192, 1)',
                            'rgba(153, 102, 255, 1)',
                            'rgba(255, 159, 64, 1)'
                        ],
                        borderWidth: 1
                    }]
            }/*,
            
            options: {
                scales: {
                    yAxes: [{
                            ticks: {
                                beginAtZero: true
                            }
                        }]
                }
            }
            */
        });
        $('.loader').hide();
    })
            .error(function (xhr, textStatus) {
                alert(textStatus + " " + xhr.readyState);
            });
}

function updateGraph() {
    $('.loader').show();
            
    if(myChart !== null)
        myChart.destroy();
    
    var start = document.getElementById('fromdate').value;
    var end = document.getElementById('todate').value;

    if (start === "" || end === "") {
        start = new Date().toISOString().substring(0, 10);
        end = new Date().toISOString().substring(0, 10);
    }

    document.getElementById('fromdate').value = start;
    document.getElementById('todate').value = end;

    start += " 00:00:00";
    end += " 23:59:59";

    var id = document.getElementById('nodeId').value;

    console.log("Date is:" + start + " and end is: " + end);

    onNodeClicked(id, start, end);
}

function loadDay(){
    var sdate = new Date();
    var edate = new Date();
    document.getElementById('fromdate').value = sdate.toISOString().substring(0, 10);
    document.getElementById('todate').value = edate.toISOString().substring(0, 10);
    updateGraph();
} 

function loadMonth(){
    var sdate = new Date();
    if(sdate.getMonth() === 0){
        sdate.setMonth(11);
    }
    else{
        sdate.setMonth(sdate.getMonth() - 1);
    }
     
    var edate = new Date();
    
    document.getElementById('fromdate').value = sdate.toISOString().substring(0, 10);
    document.getElementById('todate').value = edate.toISOString().substring(0, 10);
    updateGraph();
}

function loadYear(){
    var sdate = new Date();
    var edate = new Date();
    sdate.setFullYear(sdate.getFullYear() - 1);
    document.getElementById('fromdate').value = sdate.toISOString().substring(0, 10);
    document.getElementById('todate').value = edate.toISOString().substring(0, 10);
    updateGraph();
}