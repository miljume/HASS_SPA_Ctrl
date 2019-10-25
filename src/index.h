const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="stylesheet" href="http://code.jquery.com/mobile/1.4.5/jquery.mobile-1.4.5.min.css"/>
<script src="http://code.jquery.com/jquery-1.11.3.min.js"></script>
<script src="http://code.jquery.com/mobile/1.4.5/jquery.mobile-1.4.5.min.js"></script>
</head>
<body>
<div data-role="header">
<h1>MSPA CAMARO</h1>
</div>
<form>
<div class = "ui-field-contain">
<select id = "power" onchange="setPower(this.value)">
<option option value = "0">SPA Power OFF</option>
<option option value = "1">SPA Power ON</option>
</select>
<select id = "heater" onchange="setHeater(this.value)">
<option option value = "0">SPA Heater OFF</option>
<option option value = "1">SPA Heater ON</option>
</select>
</div>
</form>
<label for="slider">Set Temp: </label>
<input type="range" name="temp" id="temp" value="38" min="20" max="42" data-highlight = "true">
<input type="button" name="set_temp" id="set_temp" value="Set Temp" onclick="setTemp(temp.value)">
<div>
	Actual Temp : <span id="act_temp">NA</span>
</div>
</select>
<script>

function setTemp(temp_state) {
  var xhttp = new XMLHttpRequest();  
  xhttp.open("GET", "setTemp?Temperature="+temp_state, true);
  xhttp.send();
}

function setPower(power_state) {
  var xhttp = new XMLHttpRequest();  
  xhttp.open("GET", "setPower?PowerState="+power_state, true);
  xhttp.send();
}

function setHeater(heater_state) {
  var xhttp = new XMLHttpRequest();  
  xhttp.open("GET", "setHeater?HeaterState="+heater_state, true);
  xhttp.send();
}

setInterval(function() {
  updateStatus();
}, 2000); //2000mSeconds update rate

function updateStatus(){
  $.getJSON('/status.json', function(data){
    $("#power").val(data.p == "off" ? "0" : "1").selectmenu("refresh");
	$("#heater").val(data.h == "off" ? "0" : "1").selectmenu("refresh");
	$("#act_temp").html(data.at);
	$("#temp").val(data.st).slider("refresh");
  }).fail(function(err){
    console.log("err getJSON mesures.json "+JSON.stringify(err));
  });
};
</script>
</body>
</html>
)=====";
