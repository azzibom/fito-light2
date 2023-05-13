#include <EEManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define WIFI_AP_NAME "FitoLight"
#define WIFI_AP_PASS "123456-78"
#define SERVER_PORT 80

const char SP_index_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport"
          content="width=device-width, user-scalable=no, initial-scale=1.0, maximum-scale=1.0, minimum-scale=1.0">
    <meta http-equiv="X-UA-Compatible" content="ie=edge">
    <title>FitoLight</title>
    <style>
        html {
            font-family: sans-serif;
            text-align: center;
            width: 25%;
            margin: 0 auto;
            display: flex;
            flex-direction: column;
            align-items: center;
        }

        #switchBtn {
            padding: 20px;
            font-size: 1rem;
            margin-bottom: 10px;
        }
    </style>
</head>
<body>
<div class="cont">
    <h1><a href="/">FitoLight</a></h1>
    <form id="switchForm">
        <button id="switchBtn" type="submit" name="switch">On/Off</button>
    </form>
    <a href="/settings">Settings</a>
</div>

<script>
    let STATE = false;
    const switchBtn = document.getElementById('switchBtn');
    const switchForm = document.getElementById('switchForm')
    switchBtn.disabled = true
    fetch('/state')
        .then(resp => resp.text())
        .then(text => {
            setState(text === 'on')
            switchBtn.disabled = false
        })
    switchForm.addEventListener("submit", e => {
        e.preventDefault()
        switchBtn.disabled = true
        fetch(`/switch?state=${STATE ? 'off' : 'on'}`)
            .then(resp => resp.text())
            .then(text => {
                setState(text === 'on')
                switchBtn.disabled = false
            })
    })

    function setState(state) {
        STATE = state
        switchBtn.textContent = state ? 'Off' : 'On'
    }
</script>
</body>
</html>
)rawliteral";

const char SP_settings_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport"
          content="width=device-width, user-scalable=no, initial-scale=1.0, maximum-scale=1.0, minimum-scale=1.0">
    <meta http-equiv="X-UA-Compatible" content="ie=edge">
    <title>Settings</title>
    <style>
        body {
            font-family: sans-serif;
            text-align: center;
            width: 25%;
            margin: 0 auto;
            display: flex;
            flex-direction: column;
            align-items: center;
        }

        form, fieldset {
            display: flex;
            flex-direction: column;
            align-items: center;
        }

        form * {
            margin-bottom: 5px;
        }
    </style>
</head>
<body>
<div class="cont">
    <h1><a href="/">FitoLight</a></h1>
    <h2>Настройки</h2>
    <fieldset>
        <legend>WiFi</legend>
        <form action="/wifi" method="post">
            <input name="ssid" placeholder="ssid" required>
            <input type="password" name="pass" placeholder="password" required>
            <button type="submit">Подключится</button>
        </form>
    </fieldset>
</div>
</body>
</html>

)rawliteral";

struct WifiSettings {
  bool on = false;
  char ssid[32];
  char password[32];
};

WifiSettings wifiSettings;
ESP8266WebServer server(SERVER_PORT);
EEManager memory(wifiSettings);

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  readSettings();
  runWifi();
  runServer();
}

void loop() {
  server.handleClient();
}

// fun

void readSettings() {
  EEPROM.begin(memory.blockSize());
  memory.begin(0, 254);
}

void runWifi() {
  WiFi.softAPdisconnect();
  bool upped = wifiSettings.on;
  if (upped) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSettings.ssid, wifiSettings.password);
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(500);
      Serial.println(WiFi.status());
    }
  }
  if (!upped) {
    WiFi.softAP(WIFI_AP_NAME, WIFI_AP_PASS);
  }
}

void runServer() {
  server.onNotFound(handleNotFound);
  server.on("/", handleRoot);
  server.on("/switch", handleSwitch);
  server.on("/state", handleState);
  server.on("/settings", handleSettings);
  server.on("/wifi", HTTP_POST, handleWifi);

  server.begin();
  Serial.println("HTTP server started");
}

void switchLED(bool on) {
  bool ledStatus = digitalRead(LED_BUILTIN);
  if (ledStatus != on) {
    digitalWrite(LED_BUILTIN, on);
  }
}

void handleSwitch() {
  if(server.hasArg("state")) {
    String state = server.arg("state");
    switchLED(state == "on"); 
  }
  handleState();
}

void handleState() {
  bool ledStatus = digitalRead(LED_BUILTIN);
  server.send(200, "text/plane", ledStatus ? "on" : "off");
}

void handleSettings() {
 server.send(200, "text/html", SP_settings_page);
}

void handleWifi() {
  strcpy(wifiSettings.ssid, server.arg("ssid").c_str());
  strcpy(wifiSettings.password, server.arg("pass").c_str());
  wifiSettings.on = true;
  runWifi();
  memory.updateNow();
}

void handleRoot() {
  server.send(200, "text/html", SP_index_page);
}

void handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plane", "");
}

