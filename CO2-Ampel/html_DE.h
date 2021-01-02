/***************************************************************************
  Copyright (c) 2020-2021 Lars Wessels

  This file a part of the "CO2-Ampel" source code.
  https://github.com/lrswss/co2ampel

  Published under Apache License 2.0

***************************************************************************/

const char HEADER_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="de">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta name="author" content="(c) 2020-2021 Lars Wessels, Karlsruhe, GERMANY">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>CO2-Ampel</title>
<style>
body{text-align:center;font-family:verdana;background:white;}
div,fieldset,input,select{padding:5px;font-size:1em;}
h2{margin-top:2px;margin-bottom:8px}
h3{font-size:0.8em;margin-top:-2px;margin-bottom:2px;font-weight:lighter;}
p{margin:0.5em 0;}
a{text-decoration:none;color:inherit;}
input{width:100%;box-sizing:border-box;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;}
input[type=checkbox],input[type=radio]{width:1em;margin-right:6px;vertical-align:-1px;}
p,input[type=text]{font-size:0.96em;}
select{width:100%;}
textarea{resize:none;width:98%;height:318px;padding:5px;overflow:auto;}
button{border:0;border-radius:0.3rem;background:#009374;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;-webkit-transition-duration:0.4s;transition-duration:0.4s;cursor:pointer;}
button:hover{background:#007364;}
button:focus {outline:0;}
td{text-align:right;}
.bred{background:#d43535;}
.bred:hover{background:#931f1f;}
.bgrn{background:#47c266;}
.bgrn:hover{background:#5aaf6f;}
.footer{font-size:0.6em;color:#aaa;}
</style>
)=====";


const char ROOT_html[] PROGMEM = R"=====(
<script>
var suspendReadings = false;
var calibrationInProgress = false;
var co2status = 0; // no data
var prevCO2status = 0;

function hideMessages() {
  document.getElementById("timeoutInfo").style.display = "none";
  document.getElementById("warmupInfo").style.display = "none";
  document.getElementById("calibrateInfo").style.display = "none"; 
  document.getElementById("calibrateDone").style.display = "none";
  document.getElementById("calibrateFailed").style.display = "none";
  document.getElementById("noopMode").style.display = "none";
}

function resetSystem() {
  var xhttp = new XMLHttpRequest();
  suspendReadings = true;
  if (confirm("CO2-Ampel wirklich neustarten?")) {
    hideMessages();
    document.getElementById("sysReset").style.display = "block";
    document.getElementById("message").style.height = "16px";
    document.getElementById("heading").scrollIntoView();
    xhttp.open("GET", "/restart", true);
    xhttp.send();
    setTimeout(function(){location.href='/';}, 15000);
  }
}

function calibrateCO2Sensor() {
  var xhttp = new XMLHttpRequest();
  if (co2status == 1) {
    alert("Aufwärmphase noch aktiv!");
  } else if (calibrationInProgress) {
    alert("CO2-Sensorkalibrierung noch aktiv!");
  } else if (confirm("CO2-Sensor wirklich neu kalibrieren? Der Sensor muss für ca. 5 Minuten der Außenluft ausgesetzt werden.")) {
    document.getElementById("timeoutInfo").style.display = "none";
    document.getElementById("heading").scrollIntoView();
    calibrationInProgress = true;
    xhttp.open("GET", "/calibrate", true);
    xhttp.send();
    setTimeout(function() { getReadings(); }, 500);
  }
}

function webserverOffline() {
  document.getElementById("timeoutInfo").style.display = "none";
  document.getElementById("webserverOffline").style.display = "block";
  suspendReadings = true;
}

function getSetup() {
  var xhttp = new XMLHttpRequest();
  var arr;
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      arr = this.responseText.split(',');
      if (arr[0] == "1") { // LoRaWAN enabled?    
        document.getElementById("lorawan_modus").style.display = "table-row";
      }
      if (arr[1] == "1") { // Logging enabled?    
        document.getElementById("logfile_download").style.display = "block";
      }
    }    
  };
  xhttp.open("GET", "/setup", true);
  xhttp.send();
}

function getReadings() {
  var xhttp = new XMLHttpRequest();
  var res;
  var timeout;
  var height = 0;
  var lorawanMode = -1;

  if (suspendReadings)
    return;
 
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      res = JSON.parse(xhttp.responseText);
      document.getElementById("Date").innerHTML = res.date;
      document.getElementById("Time").innerHTML = res.time;
      document.getElementById("Temp").innerHTML = res.temperature;
      document.getElementById("CO2").innerHTML = res.co2median;
      co2status = parseInt(res.co2status); // see config.h
      document.getElementById("Hum").innerHTML = res.humidity;
      document.getElementById("Pres").innerHTML = res.pressure;
      batteryVoltage = parseFloat(res.vbat);
      if (batteryVoltage > 0) {
        document.getElementById("VBat").innerHTML = res.vbat;
      }
      webserverTimeout = parseInt(res.webserverTimeout);
      document.getElementById("WebserverTimeout").innerHTML = webserverTimeout > 60 ? Math.round(webserverTimeout/60) + " Min." : webserverTimeout + " Sek.";
      calibrationTimeout = parseInt(res.calibrationTimeout);
      document.getElementById("CalibrationTimeout").innerHTML = calibrationTimeout;
      warmupTimeout = parseInt(res.warmupTimeout);
      document.getElementById("WarmupTimeout").innerHTML = warmupTimeout;
      mqttCounter = parseInt(res.mqttMessages)
      document.getElementById("MqttCounter").innerHTML = mqttCounter;
      document.getElementById("LoRaDevAddr").innerHTML = res.loraDevAddr;
      document.getElementById("LoRaSeqnoUp").innerHTML = res.loraSeqnoUp;
      lorawanMode = parseInt(res.otaa);

      if (calibrationInProgress || calibrationTimeout > 0) {
        if (co2status != 6) {  // calibration ended
          calibrationInProgress = true;
          document.getElementById("calibrateInfo").style.display = "none";
          if (co2status != 7) {
            document.getElementById("calibrateDone").style.display = "block";
          } else {
            document.getElementById("calibrateFailed").style.display = "block";
          }
          // preserve closing calibration status message for a few seconds
          setTimeout(function(){ calibrationInProgress = false; }, 3000);
        } else {  // calibration ongoing
          document.getElementById("calibrateInfo").style.display = "block";
          document.getElementById("CO2").innerHTML = "----";
        }
        height += 12;
      } else {
        document.getElementById("calibrateInfo").style.display = "none";
        document.getElementById("calibrateDone").style.display = "none";
        document.getElementById("calibrateFailed").style.display = "none";
      }

      if (co2status == 1) {
        document.getElementById("warmupInfo").style.display = "block";
        document.getElementById("CO2").innerHTML = "----";
        height += 12;
      } else {
        document.getElementById("warmupInfo").style.display = "none";
      }

      if (webserverTimeout > 0 && !calibrationInProgress && prevCO2status != 1 && co2status != 1) {
        if (webserverTimeout <= 5)
          setTimeout(webserverOffline, 5000);
        document.getElementById("timeoutInfo").style.display = "block";
        height += 12;
      } else {
        document.getElementById("timeoutInfo").style.display = "none";
      }    

      if (!co2status && !prevCO2status) {
        document.getElementById("invalidData").style.display = "block";
        height += 12;
      } else {
        document.getElementById("invalidData").style.display = "none";
      }
      
      if (co2status == 8) {
        document.getElementById("noopMode").style.display = "block";
        height += 12;
      } else {
        document.getElementById("noopMode").style.display = "none";
      }
      prevCO2status = co2status; // used to add small (empty) delay in status messages

      if (height > 0) {
        height += 4;
        document.getElementById("message").style.height = height + "px";
      } else {
        document.getElementById("message").style.display = "none";
      }

      if (batteryVoltage < 3.70 && batteryVoltage > 0) {
        document.getElementById("VBatDisplay").style = "font-weight:bold;color:red";
      } else {
        document.getElementById("VBatDisplay").style = "font-weight:normal;color:black";
      }

      if (mqttCounter > -1) {
        document.getElementById("mqtt_msgs").style.display = "table-row";
      } else {
        document.getElementById("mqtt_msgs").style.display = "none";
      }

      if (lorawanMode > -1) {
        document.getElementById("lorawan_addr").style.display = "table-row";
        document.getElementById("lorawan_seqnoup").style.display = "table-row";
      }
      if (lorawanMode > 0) {
        document.getElementById("LoRaMode").innerHTML = "OTAA"
      } else if (!lorawanMode) {
        document.getElementById("LoRaMode").innerHTML = "ABP";
      } else {
        document.getElementById("LoRaMode").innerHTML = "Aus";
      }
    }
  };
  xhttp.open("GET", "/ui", true);
  xhttp.send();
}

function initPage() {
  getSetup();
  setTimeout(function() { getReadings(); }, 250);
  setInterval(function() { getReadings(); }, 3000);
}
</script>
</head>
<body onload="initPage();">
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2 id="heading">CO2-Ampel __SYSTEMID__</h2>
<div id="message" style="margin-top:10px;color:red;text-align:center;font-weight:bold;max-width:335px">
<span id="sysReset" style="display:none">System wird neu gestartet...</span>
<span id="timeoutInfo" style="display:none">Webserver-Abschaltung in <span id="WebserverTimeout">--</span></span>
<span id="webserverOffline" style="display:none">Webserver deaktiviert!</span>
<span id="calibrateInfo" style="display:none">CO2-Kalibrierung noch <span id="CalibrationTimeout">--</span> Sek.</span>
<span id="calibrateDone" style="display:none">CO2-Kalibrierung beendet!</span>
<span id="calibrateFailed" style="display:none">CO2-Kalibrierung fehlgeschlagen!</span>
<span id="warmupInfo" style="display:none">Aufw&auml;rmphase noch <span id="WarmupTimeout">--</span> Sek.</span>
<span id="invalidData" style="display:none">Keine g&uuml;ltigen CO2-Daten!<br></span>
<span id="noopMode" style="display:none">Messungen derzeit ausgesetzt!<br></span>
</div>
<noscript>Bitte JavaScript aktivieren!<br /></noscript>
</div>
<div id="readings" style="margin-top:8px">
<table style="min-width:340px">
  <tr><th>Systemzeit:</th><td><span id="Date">--.--.--</span> <span id="Time">--:--</span></td></tr>
  <tr><th>CO2-Konzentration:</th><td><span id="CO2">----</span> ppm</td></tr>
  <tr><th>Temperatur:</th><td><span id="Temp">--</span> &deg;C</td></tr>
  <tr><th>Luftfeuchte:</th><td><span id="Hum">--</span> %</td></tr>
  <tr><th>Luftdruck:</th><td><span id="Pres">----</span> hPa</td></tr>
  <tr id="lorawan_modus" style="display:none;"><th>LoRaWAN-Modus:</th><td><span id="LoRaMode">---</span></td></tr>
  <tr id="lorawan_addr" style="display:none;"><th>LoRaWAN-Adresse:</th><td><span id="LoRaDevAddr">----------<span></td></tr>
  <tr id="lorawan_seqnoup" style="display:none;"><th>LoRaWAN-Pakete:</th><td><span id="LoRaSeqnoUp">--<span></td></tr>
  <tr id="mqtt_msgs" style="display:none;"><th>MQTT-Nachrichten:</th><td><span id="MqttCounter">--<span></td></tr>
  <tr><th>Batteriespannung:</th><td id="VBatDisplay"><span id="VBat">-.--</span> V</td></tr>
</table>
</div>
<div id="buttons" style="margin-top:10px">
<p><button onclick="location.href='/config';">Einstellungen</button></p>
<p id="logfile_download" style="display:none;"><button onclick="location.href='/logs';">Logdateien herunterladen</button></p>
<p><button onclick="calibrateCO2Sensor();">CO2-Sensor kalibrieren</button></p>
<p><button onclick="location.href='/update';">Firmware aktualisieren</button></p>
<p><button class="button bred" onclick="resetSystem();">System neustarten</button></p>
</div>
)=====";


const char FOOTER_html[] PROGMEM = R"=====(
<div class="footer"><hr/>
<p style="float:left;margin-top:-2px"><a href="https://github.com/lrswss/co2ampel" title="build on __BUILD__">Firmware __FIRMWARE__</a></p>
<p style="float:right;margin-top:-2px"><a href="mailto:software@bytebox.org">&copy; 2020-2021 Lars Wessels</a></p>
<div style="clear:both;"></div>
</div>
</div>
</body>
</html>
)=====";


const char UPDATE_html[] PROGMEM = R"=====(
</head>
<body>
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2>Firmware-Update</h2>
</div>
<div style="margin-left:50px;margin-bottom:10px">
1. Firmware-Datei ausw&auml;hlen<br>
2. Aktualisierung starten<br>
3. Upload dauert ca. 15 Sek.<br>
4. CO2-Ampel neustarten
</div>
<div>
<form method="POST" action="/update" enctype="multipart/form-data" id="upload_form">
  <input type="file" name="update">
  <p><button class="button bred" type="submit">Aktualisierung durchf&uuml;hren</button></p>
</form>
<p><button onclick="location.href='/';">Startseite</button></p>
</div>
)=====";


const char UPDATE_ERR_html[] PROGMEM = R"=====(
</head>
<body>
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2>Firmware-Update<br />fehlgeschlagen</h2>
</div>
<div>
<p><button onclick="location.href='/update';">Update wiederholen</button></p>
<p><button onclick="location.href='/';">Startseite</button></p>
</div>
)=====";


const char UPDATE_OK_html[] PROGMEM = R"=====(
<script>

function resetSystem() {
  var xhttp = new XMLHttpRequest();
  if (confirm("CO2-Ampel wirklich neustarten?")) {
    document.getElementById("sysReset").style.display = "block";
    document.getElementById("message").style.display = "block";
    xhttp.open("GET", "/restart", true);
    xhttp.send();
    setTimeout(function(){location.href='/';}, 15000);
  }
}

function clearSettings() {
  var xhttp = new XMLHttpRequest();
  if (confirm("Alle Einstellungen zurücksetzen?")) {
    xhttp.open("GET", "/reset/full", true);
    xhttp.send();
  }
}
</script>
</head>
<body>
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2 id="heading">Firmware-Update<br />erfolgreich</h2>
<div id="message" style="display:none;margin-top:8px;color:red;font-weight:bold;height:16px;text-align:center;max-width:335px">
<span id="sysReset" style="display:none">CO2-Ampel wird neugestartet...<br></span>
</div>
</div>
<div>
<p><button class="button bred" onclick="clearSettings(); resetSystem();">System neustarten</button></p>
<p><button onclick="location.href='/';">Startseite</button></p>
</div>
)=====";


const char LOGS_HEADER_html[] PROGMEM = R"=====(
<script>
function deleteLogs() {
  var xhttp = new XMLHttpRequest();
  if (confirm("Wirklich alle Dateien löschen?")) {
    xhttp.open("GET", "/rmlogs", true);
    xhttp.send();
    document.getElementById("filelist").style.display = "none";
  }
}
</script>
</head>
<body>
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2>Logdateien</h2>
<h3>__BYTES_FREE__ kb frei</h3>
</div>
<div id="filelist" style="margin-left:50px;margin-top:20px;margin-bottom:10px">
)=====";


const char LOGS_FOOTER_html[] PROGMEM = R"=====(
</div>
<div>
<p><button class="button bred" onclick="deleteLogs();">Logdateien l&ouml;schen</button></p>
<p><button onclick="location.href='/sendlogs';">Logdateien herunterladen</button></p>
<p><button onclick="location.href='/';">Startseite</button></p>
</div>
)=====";


const char SETTINGS_html[] PROGMEM = R"=====(
<script>
function hideMessages() {
  document.getElementById("configSaved").style.display = "none";
  document.getElementById("configSaveFailed").style.display = "none";
  document.getElementById("configReset").style.display = "none";
  document.getElementById("userError").style.display = "none";
  document.getElementById("passError").style.display = "none";
  document.getElementById("message").style.display = "none";
}

function configSaved() {
  const url = new URLSearchParams(window.location.search);
  if (url.has("saved") || url.has("failed") ) {
    if (url.has("saved"))
      document.getElementById("configSaved").style.display = "block";
    else
      document.getElementById("configSaveFailed").style.display = "block";
    document.getElementById("message").style.display = "block";
    document.getElementById("heading").scrollIntoView();
    setTimeout(hideMessages, 4000);
  }
}

function checkInput() {
  var err = 0;
  var xhttp = new XMLHttpRequest();

  if (document.getElementById("checkbox_auth").checked == true &&
      document.getElementById("input_username").value.length < 4 &&
      document.getElementById("input_username").value.length > 15) {
    document.getElementById("userError").style.display = "block";
    err++;
  }
  if (document.getElementById("checkbox_auth").checked == true &&
      document.getElementById("input_password").value.length < 4 &&
      document.getElementById("input_password").value.length > 15) {
    document.getElementById("passError").style.display = "block";
    err++;
  }
  if (err > 0) {
    height = (err * 12) + 4;
    document.getElementById("message").style.height = height + "px";
    document.getElementById("message").style.display = "block";
    document.getElementById("heading").scrollIntoView();
    setTimeout(hideMessages, 4000);
    xhttp.open("GET", "/tickle", true); // reset webserver timeout
    xhttp.send();
    return false;
  } else {
    document.getElementById("message").style.display = "none";
    return true;
  }
}

function digitsOnly(input) {
  var regex = /[^0-9]/g;
  input.value = input.value.replace(regex, "");
}

function pad(num, size) {
  var s = num + "";
  while (s.length < size) s = "0" + s;
  return s;
}

function noopSelectors() {
  var noopstart = document.createElement("select");
  var noopend = document.createElement("select");

  noopstart.setAttribute("id", "noop_start");
  noopstart.setAttribute("name", "noopstart");
  noop_start_selector.appendChild(noopstart);

  noopend.setAttribute("id", "noop_end");
  noopend.setAttribute("name", "noopend");
  noop_end_selector.appendChild(noopend);

  for (var i = 0; i < 24; i++) {
    var opt_start = document.createElement("option");
    var opt_end = document.createElement("option");
    opt_start.setAttribute("value", i);
    opt_end.setAttribute("value", i);
    opt_start.text = pad(i, 2);
    opt_end.text = pad(i, 2);
    noopstart.appendChild(opt_start);
    noopend.appendChild(opt_end);
  }
}

function toggleMedianFilter() {
  if (document.getElementById("checkbox_medianfilter").checked == true) {
    document.getElementById("medianfilter").style.display = "block";
  } else {
    document.getElementById("medianfilter").style.display = "none";
  }
}

function toggleLogging() {
  if (document.getElementById("checkbox_logging").checked == true) {
    document.getElementById("loginterval").style.display = "block";
  } else {
    document.getElementById("loginterval").style.display = "none";
  }
}

function toggleAuth() {
  if (document.getElementById("checkbox_auth").checked == true) {
    document.getElementById("auth").style.display = "block";
  } else {
    document.getElementById("auth").style.display = "none";
  }
}

function selectNoopTime() {
    document.getElementById("noop_start").value = __NOOPSTART__;
    document.getElementById("noop_end").value = __NOOPEND__;
}

function configResetted() {
  const url = new URLSearchParams(window.location.search);
  if (url.has("reset")) {
    document.getElementById("configReset").style.display = "block";
    document.getElementById("message").style.display = "block";
    document.getElementById("heading").scrollIntoView();
    setTimeout(hideMessages, 4000);
  }
}

function clearSettings() {
  var xhttp = new XMLHttpRequest();
  suspendReadings = true;
  if (confirm("Alle Einstellungen zurücksetzen?")) {
    xhttp.open("GET", "/reset/config", true);
    xhttp.send();
    setTimeout(function(){location.href='/config?reset';}, 500);
  }
}

function getSetup() {
  var xhttp = new XMLHttpRequest();
  var arr;
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      arr = this.responseText.split(',');
      if (arr[0] == "1") { // LoRaWAN enabled?    
        document.getElementById("lorawan_settings").style.display = "block";
      } 
    }    
  };
  xhttp.open("GET", "/setup", true);
  xhttp.send();
}
</script>
</head>
<body onload="getSetup(); toggleMedianFilter(); toggleAuth(); toggleLogging(); noopSelectors(); selectNoopTime(); configSaved(); configResetted();">
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2 id="heading">Einstellungen</h2>
<div id="message" style="display:none;margin-top:10px;margin-bottom:8px;color:red;font-weight:bold;height:16px;text-align:center;max-width:335px">
<span id="configSaved" style="display:none;color:green">Einstellungen gespeichert!</span>
<span id="configSaveFailed" style="display:none;color:red">Fehler beim Speichern!</span>
<span id="configReset" style="display:none;color:red">Einstellungen zur&uuml;ckgesetzt!</span>
<span id="userError" style="display:none;color:red">Benutzer ung&uuml;ltig!</span>
<span id="passError" style="display:none;color:red">Passwort ung&uuml;ltig!!</span>
</div>
</div>
<div style="max-width:335px">
<form method="POST" action="/config" onsubmit="checkInput();">
  <fieldset><legend><b>&nbsp;CO2-Schwellwerte (ppm)&nbsp;</b></legend>
  <p><b>Gute Luftqualit&auml;t bis (gr&uuml;n)</b><br />
  <input name="co2medium" value="__CO2_MEDIUM__" onkeyup="digitsOnly(this)"></p>
  <p><b>Akzeptable Luftqualit&auml;t bis (gelb)</b><br />
  <input name="co2high" value="__CO2_HIGH__" onkeyup="digitsOnly(this)"></p>
  <p><b>Alarmierung ab (rot blinkend)</b><br />
  <input name="co2alarm" value="__CO2_ALARM__" onkeyup="digitsOnly(this)"></p>
  <p><b>Hysterese f&uuml;r Farbwechsel (ppm)</b><br />
  <input name="hysteresis" value="__HYSTERESIS__" onkeyup="digitsOnly(this)"></p></fieldset>
  <br />
  <fieldset><legend><b>&nbsp;Messparameter&nbsp;</b></legend>
  <p><b>Messinterval (min. __INTERVALMIN__ Sek.)</b><br />
  <input name="interval" value="__INTERVAL__" onkeyup="digitsOnly(this)"></p>
  <p id="medianfilter"><b>Anzahl Messungen f&uuml;r Medianwert:</b><br />
  <input name="samples" value="__SAMPLES__" onkeyup="digitsOnly(this)"></p>
  <p><input id="checkbox_medianfilter" name="medianfilter" type="checkbox" onclick="toggleMedianFilter();" __MEDIANFILTER__><b>Medianfilter aktivieren</b></p></fieldset>
  <br />
  <fieldset><legend><b>&nbsp;Messungen aussetzen&nbsp;</b></legend>
  <p><b>Startzeit</b><br /><span id="noop_start_selector"></span></p>
  <p id="noop_end_option"><b>Endzeit</b><br /><span id="noop_end_selector"></span></p>
  <p><input id="checkbox_noop" name="noop" type="checkbox" __NOOP__><b>Zeitplan aktivieren</b></p></fieldset>
  <br />
  <fieldset><legend><b>&nbsp;Grundeinstellungen&nbsp;</b></legend>
  <p><input id="checkbox_auth" name="auth" type="checkbox" __AUTH__ onclick="toggleAuth();"><b>Passwortschutz aktivieren</b></p>
  <span id="auth">
    <p><b>Benutzername</b><br />
    <input id="input_username" name="username" size="16" maxlength="15" value="__USERNAME__"></p>
    <p><b>Passwort</b><br />
    <input id="input_password" type="password" name="password" size="16" maxlength="15" value="__PASSWORD__"></p>
  </span> 
  <p><b>H&ouml;he &uuml;ber NN</b><br />
  <input name="altitude" value="__ALTITUDE__" onkeyup="digitsOnly(this);"></p>
  <p><input id="checkbox_logging" name="logging" type="checkbox" __LOGGING__ onclick="toggleLogging();"><b>Protokollierung aktivieren</b></p>
  <p id="loginterval"><b>Aufzeichungsinterval (min. 60 Sek.)</b><br />
  <input name="loginterval" value="__LOGINTERVAL__" onkeyup="digitsOnly(this)"></p>
  </fieldset>
  <p style="margin-top:25px"><button class="button bred" onclick="clearSettings(); return false;">Einstellungen zur&uuml;cksetzen</button></p>
  <p><button type="submit">Einstellungen speichern</button></p>
</form>
<p><button style="margin-top:15px" onclick="location.href='/network';">Netzwerkeinstellungen</button></p>
<p id="lorawan_settings" style="display:none;"><button onclick="location.href='/lorawan';">LoRaWAN-Parameter</button></p>
<p><button onclick="location.href='/';">Startseite</button></p>
</div>
)=====";


const char NETWORK_html[] PROGMEM = R"=====(
<script>
function hideMessages() {
  document.getElementById("configSaved").style.display = "none";
  document.getElementById("configSaveFailed").style.display = "none";
  document.getElementById("configReset").style.display = "none";
  document.getElementById("appassError").style.display = "none";
  document.getElementById("stassidError").style.display = "none";
  document.getElementById("stapassError").style.display = "none";
  document.getElementById("mqttpassError").style.display = "none";
  document.getElementById("mqttuserError").style.display = "none";
  document.getElementById("message").style.display = "none";
}

function checkInput() {
  var err = 0;
  var xhttp = new XMLHttpRequest();

  if (document.getElementById("input_appassword").value.length > 0 && 
      document.getElementById("input_appassword").value.length < 8) {
    document.getElementById("appassError").style.display = "block";
    err++;
  }
  if (document.getElementById("checkbox_wlan").checked == true &&
      document.getElementById("input_stassid").value.length <= 2) {
    document.getElementById("stassidError").style.display = "block";
    err++;
  }
  if (document.getElementById("checkbox_wlan").checked == true &&
      document.getElementById("input_stapassword").value.length > 0 &&
      document.getElementById("input_stapassword").value.length < 8) {
    document.getElementById("stapassError").style.display = "block";
    err++;
  }
  if (document.getElementById("checkbox_mqttauth").checked == true &&
      document.getElementById("input_mqttpassword").value.length < 4 &&
      document.getElementById("input_mqttpassword").value.length > 31) {
    document.getElementById("mqttpassError").style.display = "block";
    err++;
  }
  if (document.getElementById("checkbox_mqttauth").checked == true &&
      document.getElementById("input_mqttuser").value.length < 4 &&
      document.getElementById("input_mqttuser").value.length > 31) {
    document.getElementById("mqttuserError").style.display = "block";
    err++;
  }
  if (err > 0) {
    height = (err * 12) + 4;
    document.getElementById("message").style.height = height + "px";
    document.getElementById("message").style.display = "block";
    document.getElementById("heading").scrollIntoView();
    setTimeout(hideMessages, 4000);
    xhttp.open("GET", "/tickle", true); // reset webserver timeout
    xhttp.send();
    return false;
  } else {
    document.getElementById("message").style.display = "none";
    return true;
  }
}

function digitsOnly(input) {
  var regex = /[^0-9]/g;
  input.value = input.value.replace(regex, "");
}

function configSaved() {
  const url = new URLSearchParams(window.location.search);
  if (url.has("saved") || url.has("failed") ) {
    if (url.has("saved"))
      document.getElementById("configSaved").style.display = "block";
    else
      document.getElementById("configSaveFailed").style.display = "block";
    document.getElementById("message").style.display = "block";
    document.getElementById("heading").scrollIntoView();
    setTimeout(hideMessages, 4000);
  }
}

function configResetted() {
  const url = new URLSearchParams(window.location.search);
  if (url.has("reset")) {
    document.getElementById("configReset").style.display = "block";
    document.getElementById("message").style.display = "block";
    document.getElementById("heading").scrollIntoView();
    setTimeout(hideMessages, 4000);
  }
}

function toggleWebAutoOff() {
  if (document.getElementById("checkbox_webtimeout").checked == true) {
    document.getElementById("webtimeout").style.display = "block";
    document.getElementById("checkbox_restapi").checked = false;
  } else {
    document.getElementById("webtimeout").style.display = "none";
  }
}

function toggleREST() {
  if (document.getElementById("checkbox_restapi").checked == true) {
    document.getElementById("checkbox_webtimeout").checked = false;
    document.getElementById("webtimeout").style.display = "none";
    if (document.getElementById("checkbox_wlan").checked == false) {
      document.getElementById("checkbox_wlan").checked = true;
      toggleWLAN();
    }
  }
}

function toggleWLAN() {
  if (document.getElementById("checkbox_wlan").checked == true) {
    document.getElementById("wlan").style.display = "block";
    document.getElementById("mqtt").style.display = "block";
    toggleMQTT();
  } else {
    document.getElementById("wlan").style.display = "none";
    document.getElementById("mqtt").style.display = "none";
    document.getElementById("checkbox_mqtt").checked = false;
    document.getElementById("checkbox_restapi").checked = false;
    toggleREST();
  }
}

function toggleMQTT() {
  if (document.getElementById("checkbox_mqtt").checked == true) {
    document.getElementById("mqttsettings").style.display = "block";
  } else {
    document.getElementById("mqttsettings").style.display = "none";
  }
}

function toggleMQTTAuth() {
  if (document.getElementById("checkbox_mqttauth").checked == true) {
    document.getElementById("mqttauth").style.display = "block";
  } else {
    document.getElementById("mqttauth").style.display = "none";
  }
}

function clearSettings() {
  var xhttp = new XMLHttpRequest();
  suspendReadings = true;
  if (confirm("Alle Einstellungen zurücksetzen?")) {
    xhttp.open("GET", "/reset/network", true);
    xhttp.send();
    setTimeout(function() { location.href = "/network?reset"; }, 500);
  }
}
</script>
</head>
<body onload="toggleWLAN(); toggleMQTT(); toggleMQTTAuth(); toggleWebAutoOff(); configResetted(); configSaved();">
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2 id="heading">Netzwerkeinstellungen</h2>
<div id="message" style="display:none;margin-top:10px;margin-bottom:8px;color:red;font-weight:bold;height:16px;text-align:center;max-width:335px">
<span id="configSaved" style="display:none;color:green">Einstellungen gespeichert!</span>
<span id="configSaveFailed" style="display:none;color:red">Fehler beim Speichern!</span>
<span id="configReset" style="display:none;color:red">Einstellungen zur&uuml;ckgesetzt!</span>
<span id="appassError" style="display:none">Passwort f&uuml;r Access-Point zu kurz!</span>
<span id="stassidError" style="display:none">WLAN-SSID fehlt!</span>
<span id="stapassError" style="display:none">WLAN-Passwort f&uuml;r zu kurz!</span>
<span id="mqttuserError" style="display:none">MQTT-Benutzername ung&uuml;tig!</span>
<span id="mqttpassError" style="display:none">MQTT-Passwort ung&uuml;tig!</span>
</div>
</div>
<div style="max-width:335px">
<form method="POST" action="/network" onsubmit="return checkInput();">
  <fieldset><legend><b>&nbsp;Lokaler Webserver&nbsp;</b></legend>
  <p><input id="checkbox_webtimeout" name="webserverautooff" type="checkbox" onclick="toggleWebAutoOff();" __WEBAUTOOFF__><b>Autoabschaltung aktivieren</b></p>
  <p id="webtimeout"><b>Laufzeit (min. __WEBTIMEOUTMIN__ Sek.)</b><br />
  <input name="webtimeout" value="__WEBTIMEOUT__" onkeyup="digitsOnly(this)"></p>
  <p><input id="checkbox_restapi" name="restapi" type="checkbox" onclick="toggleREST();" __RESTAPI__><b>RESTful API aktivieren</b></p></fieldset>
  <br />
  <fieldset><legend><b>&nbsp;WLAN&nbsp;</b></legend>
  <p><b>Passwort f&uuml;r lokalen Access-Point</b><br />
  <input id="input_appassword" type="password" name="appassword" size="16" maxlength="31" value="__APPASSWORD__"></p>
  <p style="margin-top:10px"><input id="checkbox_wlan" name="wlan" type="checkbox" onclick="toggleWLAN();" __WLAN__><b>Mit lokalem WLAN verbinden</b></p>
  <span id="wlan">
    <p><b>Netzwerkkennung (SSID)</b><br />
    <input id="input_stassid" name="stassid" size="16" maxlength="31" value="__STASSID__"></p>
    <p><b>Passwort (optional)</b><br />
    <input id="input_stapassword" type="password" name="stapassword" size="16" maxlength="31" value="__STAPASSWORD__"></p>
  </span></fieldset>
  <span style="display:none" id="mqtt">
    <br />
    <fieldset><legend><b>&nbsp;Push-Nachrichten&nbsp;</b></legend>
    <p><input id="checkbox_mqtt" name="mqtt" type="checkbox" onclick="toggleMQTT();" __MQTT__><b>MQTT aktivieren</b></p>
    <span style="display:none" id="mqttsettings">
      <p><b>Adresse des Brokers</b><br />
      <input name="mqttbroker" size="16" maxlength="63" value="__MQTTBROKER__"></p>
      <p><b>Topic f&uuml;r Nachrichten</b><br />
      <input name="mqtttopic" size="16" maxlength="63" value="__MQTTTOPIC__"></p>
      <p><b>Interval (max. 900 Sek.)</b><br />
      <input name="mqttinterval" value="__MQTTINTERVAL__" onkeyup="digitsOnly(this)"></p>
      <p><input id="checkbox_mqttauth" name="mqttauth" onclick="toggleMQTTAuth();" type="checkbox" __MQTTAUTH__><b>Authentifizierung aktivieren</b></p>
      <span style="display:none" id="mqttauth">
        <p><b>Benutzername</b><br />
        <input id="input_mqttuser" name="mqttuser" size="16" maxlength="31" value="__MQTTUSERNAME__"></p>
        <p><b>Passwort</b><br />
        <input id="input_mqttpassword" type="password" name="mqttpassword" size="16" maxlength="31" value="__MQTTPASSWORD__"></p>
      </span>
      <p><input name="mqttjson" type="checkbox" __MQTTJSON__><b>Pro Messwert ein Topic</b></p>
    </span></fieldset>
  </span>
  <p style="margin-top:25px"><button class="button bred" onclick="clearSettings(); return false;">Einstellungen zur&uuml;cksetzen</button></p>
  <p><button type="submit">Einstellungen speichern</button></p>
</form>
<p><button onclick="location.href='/';">Startseite</button></p>
</div>
)=====";


const char LORAWAN_html[] PROGMEM = R"=====(
<script>
function hideMessages() {
  document.getElementById("configSaved").style.display = "none";
  document.getElementById("configSaveFailed").style.display = "none";
  document.getElementById("configReset").style.display = "none";
  document.getElementById("appeuiError").style.display = "none";
  document.getElementById("appkeyError").style.display = "none";
  document.getElementById("devaddrError").style.display = "none";
  document.getElementById("nwkskeyError").style.display = "none";
  document.getElementById("appskeyError").style.display = "none";
  document.getElementById("authError").style.display = "none";
  document.getElementById("message").style.display = "none";
}

function configSaved() {
  const url = new URLSearchParams(window.location.search);
  if (url.has("saved") || url.has("failed") ) {
    if (url.has("saved"))
      document.getElementById("configSaved").style.display = "block";
    else
      document.getElementById("configSaveFailed").style.display = "block";
    document.getElementById("configSaved").style.display = "block";
    document.getElementById("message").style.display = "block";
    document.getElementById("heading").scrollIntoView();
    setTimeout(hideMessages, 4000);
  }
}

function configResetted() {
  const url = new URLSearchParams(window.location.search);
  if (url.has("reset")) {
    document.getElementById("configReset").style.display = "block";
    document.getElementById("message").style.display = "block";
    document.getElementById("heading").scrollIntoView();
    setTimeout(hideMessages, 4000);
  }
}

function clearSettings() {
  var xhttp = new XMLHttpRequest();
  suspendReadings = true;
  if (confirm("Alle Einstellungen zurücksetzen?")) {
    xhttp.open("GET", "/reset/lorawan", true);
    xhttp.send();
    setTimeout(function() { location.href = "/lorawan?reset"; }, 500);
  }
}

function hexOnly(input) {
  input.value = input.value.toUpperCase();
  input.value = input.value.replace(/[^0-9A-F]/g, "");
}

function digitsOnly(input) {
  var regex = /[^0-9]/g;
  input.value = input.value.replace(regex, "");
}

function checkInput() {
  var err = 0;
  var xhttp = new XMLHttpRequest();

  if (document.getElementById("enabled_checkbox").checked == false) {
    return true;
  }
  
  if (document.getElementById("abp_checkbox").checked == false &&
      document.getElementById("otaa_checkbox").checked == false) {
    document.getElementById("authError").style.display = "block";
    err++;
  }
  if (document.getElementById("ttn_checkbox").checked == true &&
      (document.getElementById("otaa_checkbox").checked == true ||
      document.getElementById("appeui_input").value.replace(/0/g, "").length > 0) &&
      (document.getElementById("appeui_input").value.length != 16 ||
      document.getElementById("appeui_input").value.replace(/0/g, "").length <= 4)) {
    document.getElementById("appeuiError").style.display = "block";
    document.getElementById("appeui_input").style.border = "2px solid red";
    err++;
  } else {
    document.getElementById("appeui_input").style.border = "1px solid";
  }
  if ((document.getElementById("otaa_checkbox").checked == true ||
      document.getElementById("appkey_input").value.replace(/0/g, "").length > 0) && 
      (document.getElementById("appkey_input").value.length != 32 ||
      document.getElementById("appkey_input").value.replace(/0/g, "").length <= 8)) {  
    document.getElementById("appkeyError").style.display = "block";
    document.getElementById("appkey_input").style.border = "2px solid red";
    err++;
  } else {
    document.getElementById("appkey_input").style.border = "1px solid";
  }
  if ((document.getElementById("abp_checkbox").checked == true ||
      document.getElementById("devaddr_input").value.replace(/0/g, "").length > 0) &&
      (document.getElementById("devaddr_input").value.length != 8 ||
      document.getElementById("devaddr_input").value.replace(/0/g, "").length <= 2)) {
    document.getElementById("devaddrError").style.display = "block";
    document.getElementById("devaddr_input").style.border = "2px solid red";
    err++;
  } else {
    document.getElementById("devaddr_input").style.border = "1px solid";
  }
  if ((document.getElementById("abp_checkbox").checked == true ||
      document.getElementById("nwkskey_input").value.replace(/0/g, "").length > 0) &&
      (document.getElementById("nwkskey_input").value.length != 32 ||
      document.getElementById("nwkskey_input").value.replace(/0/g, "").length <= 8)) {
    document.getElementById("nwkskeyError").style.display = "block";
    document.getElementById("nwkskey_input").style.border = "2px solid red";
    err++;
  } else {
    document.getElementById("nwkskey_input").style.border = "1px solid";
  }
  if ((document.getElementById("abp_checkbox").checked == true ||
      document.getElementById("appskey_input").value.replace(/0/g, "").length > 0) &&
      (document.getElementById("appskey_input").value.length != 32 ||
      document.getElementById("appskey_input").value.replace(/0/g, "").length <= 8)) {
    document.getElementById("appskeyError").style.display = "block";
    document.getElementById("appskey_input").style.border = "2px solid red";
    err++;
  } else {
    document.getElementById("appskey_input").style.border = "1px solid";
  }

  if (err > 0) {
    height = (err * 12) + 4;
    document.getElementById("message").style.height = height + "px";
    document.getElementById("message").style.display = "block";
    document.getElementById("heading").scrollIntoView();
    setTimeout(hideMessages, 4000);
    xhttp.open("GET", "/tickle", true); // reset webserver timeout
    xhttp.send();
    return false;
  } else {
    document.getElementById("message").style.display = "none";
    return true;
  }
}

function toggleOTAA() {
  if (document.getElementById("otaa_checkbox").checked == true) {
    document.getElementById("abp_checkbox").checked = false;
    document.getElementById("drjoin_selector").disabled = false;
    document.getElementById("sf11").disabled = false;
    document.getElementById("sf12").disabled = false;
  } else {
    document.getElementById("abp_checkbox").checked = true;
    document.getElementById("drjoin_selector").disabled = true;
    if (document.getElementById("ttn_checkbox").checked == true) {
      document.getElementById("sf11").disabled = true;
      document.getElementById("sf12").disabled = true;
    }
  }
}

function toggleABP() {
  if (document.getElementById("abp_checkbox").checked == true) {
    document.getElementById("otaa_checkbox").checked = false;
    document.getElementById("drjoin_selector").disabled = true;
    if (document.getElementById("ttn_checkbox").checked == true) {
      document.getElementById("sf11").disabled = true;
      document.getElementById("sf12").disabled = true;
      if (document.getElementById("drsend_selector").value < 2)
        document.getElementById("drsend_selector").value = 2;

    }
  } else {
    document.getElementById("otaa_checkbox").checked = true;
    document.getElementById("abp_checkbox").checked = false;
    document.getElementById("drjoin_selector").disabled = false;
    document.getElementById("sf11").disabled = false;
    document.getElementById("sf12").disabled = false;
  }
}

function toggleTTN() {
  if (document.getElementById("ttn_checkbox").checked == true) {
    if (document.getElementById("abp_checkbox").checked == true) {
      document.getElementById("sf11").disabled = true;
      document.getElementById("sf12").disabled = true;
      if (document.getElementById("drsend_selector").value < 2)
        document.getElementById("drsend_selector").value = 2;
    }
    document.getElementById("appeui").style.display = "block";
  } else {
    document.getElementById("appeui").style.display = "none";
    document.getElementById("sf11").disabled = false;
    document.getElementById("sf12").disabled = false;
  }
}

function toggleEnable() {
  if (document.getElementById("enabled_checkbox").checked == true) {
    document.getElementById("lorawan1").style.display = "block";
    document.getElementById("lorawan2").style.display = "block";
  } else {
    document.getElementById("lorawan1").style.display = "none";
    document.getElementById("lorawan2").style.display = "none";
  }
}

function selectSF() {
  document.getElementById("drsend_selector").value = __DRSEND__;
  if (document.getElementById("drjoin_selector"))
    document.getElementById("drjoin_selector").value = __DRJOIN__;
}
</script>
</head>
<body onload="selectSF(); toggleEnable(); toggleTTN(); toggleOTAA(); toggleABP(); configResetted(); configSaved();">
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2 id="heading">LoRaWAN-Parameter</h2>
<h3>DevEUI: __DEVEUI__</h3>
<div id="message" style="display:none;margin-top:10px;margin-bottom:8px;color:red;height:16px;font-weight:bold;text-align:center;max-width:335px">
<span id="configSaved" style="display:none;color:green">Einstellungen gespeichert!</span>
<span id="configSaveFailed" style="display:none;color:red">Fehler beim Speichern!</span>
<span id="configReset" style="display:none;color:red">Einstellungen zur&uuml;ckgesetzt!</span>
<span id="appeuiError" style="display:none">AppEUI f&uuml;r OTAA ung&uuml;ltig!</span>
<span id="appkeyError" style="display:none">AppKey f&uuml;r OTAA ung&uuml;ltig!</span>
<span id="devaddrError" style="display:none">DevAddr f&uuml;r ABP ung&uuml;ltig!</span>
<span id="nwkskeyError" style="display:none">NwksKey f&uuml;r ABP ung&uuml;ltig!</span>
<span id="appskeyError" style="display:none">AppsKey f&uuml;r ABP ung&uuml;ltig!</span>
<span id="authError" style="display:none">OTAA oder ABP ausw&auml;hlen!</span>
</div>
</div>
<div style="max-width:335px">
<form method="POST" action="/lorawan" onsubmit="return checkInput();">
  <fieldset><legend><b>&nbsp;Grundeinstellungen&nbsp;</b></legend>
  <p><input id="enabled_checkbox" name="enabled" type="checkbox" onclick="toggleEnable();" __ENABLED__><b>LoRaWAN aktivieren</b></p>
  <span id="lorawan1">
  <p><b>Sendeinterval (min. 60 Sek.)</b><br />
  <input name="txinterval" size="16" maxlength="4" value="__TXINTERVAL__" onkeyup="digitsOnly(this);"></p>
  <p><b>Spreading Factor Senden</b><select id="drsend_selector" name="drsend">
  <option value="5">SF7</option><option value="4">SF8</option><option value="3">SF9</option>
  <option value="2">SF10</option><option id="sf11" value="1">SF11</option><option id="sf12" value="0">SF12</option></select></p>
  <p><b>Spreading Factor OTAA Join</b><select id="drjoin_selector" name="drjoin">
  <option value="5">SF7</option><option value="4">SF8</option><option value="3">SF9</option>
  <option value="2">SF10</option><option value="1">SF11</option><option value="0">SF12</option></select></p>
  <p><input id="ttn_checkbox" name="ttn" type="checkbox" __TTN__ onclick="toggleTTN();"><b>Daten &uuml;ber TTN senden</b></p>
  </span>
  </fieldset>
  <span id="lorawan2">
  <br />
  <fieldset><legend><b>&nbsp;OTAA-Schl&uuml;ssel&nbsp;</b></legend>
  <p id="appeui" style="display:none"><b>AppEUI (16, MSB)</b><br />
  <input id="appeui_input" name="appeui" size="16" maxlength="16" value="__APPEUI__" onkeyup="hexOnly(this)"></p>
  <p><b>AppKey (32, MSB)</b><br />
  <input id="appkey_input" name="appkey" size="32" maxlength="48" value="__APPKEY__" onkeyup="hexOnly(this)"></p>
  <p><input id="otaa_checkbox" name="otaa" type="checkbox" onclick="toggleOTAA();" __OTAA__><b>OTAA verwenden</b></p></fieldset>
  <br />
  <fieldset><legend><b>&nbsp;ABP-Schl&uuml;ssel&nbsp;</b></legend>
  <p><b>Device Address (8, MSB)</b><br />
  <input id="devaddr_input" name="devaddr" size="8" maxlength="12" value="__DEVADDR__" onkeyup="hexOnly(this);"></p>
  <p><b>Network Session Key (32, MSB)</b><br />
  <input id="nwkskey_input" name="nwkskey" size="32" maxlength="48" value="__NWKSKEY__" onkeyup="hexOnly(this);"></p>
  <p><b>Application Session Key (32, MSB)</b><br />
  <input id="appskey_input" name="appskey" size="32" maxlength="48" value="__APPSKEY__" onkeyup="hexOnly(this);"></p>
  <p><input id="abp_checkbox" name="adp" type="checkbox" onclick="toggleABP();" __ABP__ ><b>ABP verwenden</b></p></fieldset>
  </span>
  <p style="margin-top:25px"><button class="button bred" onclick="clearSettings(); return false;">Einstellungen zur&uuml;cksetzen</button></p>
  <p><button type="submit">Einstellungen speichern</button></p>
</form>
<p><button onclick="location.href='/';">Startseite</button></p>
</div>
)=====";
