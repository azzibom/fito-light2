#include <FastBot.h>
#include <EEManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define SERVER_PORT 80
#define AP_SSID "FitoLight"
#define AP_PASS "12345678"
#define HOSTNAME "fitolight.local"

const char SP_index_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>FitoLight</title>

    <style>
        .container {
            max-width: 400px;
            margin: 0 auto;
            text-align: center;
            font-family: sans-serif;
        }
        #switchBtn {
            padding: 20px;
            font-size: large;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>FitoLight</h1>
        <button id="switchBtn" disabled>Вкл/Выкл</button>
        <hr>
        <a href="/settings">Настройки</a>
    </div>

    <script>
        let ledState = undefined

        function setBtnState(state) {
            ledState = state === 'on' ? "ON" : "OFF"
            if (ledState === "ON") {
                switchBtn.innerText = "Вкл"
            } else {
                switchBtn.innerText = "Выкл"
            }
        }

        const switchBtn = document.getElementById('switchBtn')
        switchBtn.addEventListener('click', e => {
            switchBtn.disabled = true
            const param = ledState === "ON" ? "off" : "on"
            fetch(`/led/switch?state=${param}`)
                .then(resp => resp.text())
                .then(text => {
                    setBtnState(text)
                    switchBtn.disabled = false
                })
        })

        function fetchLedState() {
            fetch('/led/state')
                .then(resp => resp.text())
                .then(text => {
                    setBtnState(text)
                    switchBtn.disabled = false
                })
            setTimeout(fetchLedState, 3000)
        }
        fetchLedState()


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
    <fieldset>
        <legend>Tg</legend>
        <form action="/tg" method="post">
            <input name="token" placeholder="token" required>
            <input name="chat_id" placeholder="chat id">
            <button type="submit">Подключится</button>
        </form>
    </fieldset>
</div>
</body>
</html>
)rawliteral";

struct WifiSet {
  char ssid[32];
  char pass[32];
};

struct TgSet {
  bool on = false;
  char token[50];
  int64_t chat = 0;
};
struct Settings {
  WifiSet wifi;
  TgSet tg;
} settings;
EEManager memory(settings);
ESP8266WebServer server(SERVER_PORT);
FastBot bot;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  readSettings();
  runWifi();
  runServer();
  runTg();
}

void loop() {
  server.handleClient();
  if (settings.tg.on) {
    bot.tick();
  }
}

// fun

void readSettings() {
  Serial.println("settings reading...");
  EEPROM.begin(memory.blockSize());
  memory.begin(0, 253);
  Serial.println("settings readed");
}

void runWifi() {
  Serial.println("WiFi starting...");
  WiFi.softAPdisconnect();
  if (strlen(settings.wifi.ssid) != 0) {
    WiFi.mode(WIFI_STA);
    WiFi.hostname(HOSTNAME);
    if (strlen(settings.wifi.pass) == 0) {
      WiFi.begin(settings.wifi.ssid);
    } else {
      WiFi.begin(settings.wifi.ssid, settings.wifi.pass);
    }
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if (millis() > 30000) break;  // даем на подключение 30 сек
    }
  }
  if (strlen(settings.wifi.ssid) == 0 || WiFi.status() != WL_CONNECTED) {
    WiFi.hostname(HOSTNAME);
    WiFi.softAP(AP_SSID, AP_PASS);
  } else {
    Serial.println(WiFi.localIP());
  }
  Serial.println("WiFi started");
}

void runServer() {  // запускаю сервер
  Serial.println("WebServer starting...");
  server.onNotFound(handleNotFound);
  // странички
  server.on("/", handleRoot);
  // server.on("/settings", handleSettings);

  // подсветка
  server.on("/led/switch", handleLedSwitch);  // /led/switch?state={on/off}
  server.on("/led/state", handleLedState);    // /led/state text/plane {on/off}

  // настройки
  server.on("/wifi", handleWifi);      // /wifi?ssid={ssid}&pass={pass}
  server.on("/tg", handleTg);          // /tg?token={token}&chat={chat}&state={on/off}
  server.on("/reset", handleReset);    // /reset?accept=true
  server.on("/status", handleStatus);  //

  server.enableCORS(true);
  server.begin();

  Serial.println("WebServer started");
}

void runTg() {
  if (settings.tg.on) {
    Serial.println("Tg bot starting...");
    bot.setToken(settings.tg.token);
    bot.setChatID(settings.tg.chat);
    bot.skipUpdates();
    bot.attach(handleTgMessage);
    bot.showMenuText("Я снова в деле", "/вкл \t /выкл \t /статус");
    Serial.println("Tg bot started");

  }
}

// ==

void switchLED(bool on, bool sendAnswerToTg) {
  digitalWrite(LED_BUILTIN, !on);
  if (sendAnswerToTg) {
    sendBotLedState();
  }
}

bool ledStatus() {
  return !digitalRead(LED_BUILTIN);
}

// ==

void handleLedSwitch() {  // вкл/выкл подсветку
  if (server.hasArg("state")) {
    String state = server.arg("state");
    switchLED(state == "on", true);
  }
  handleLedState();
}

void handleLedState() {  // состояние подвсетки
  bool ledStatus = !digitalRead(LED_BUILTIN);
  server.send(200, "text/plane", ledStatus ? "on" : "off");
}

// wifi endpoints
void handleWifi() {  // подключаемся к wifi, передаем ssid и пароль
  if (!(server.hasArg("ssid") && server.hasArg("pass"))) {
    server.send(400, "text/plan", "укажите параметры ssid и pass для подключения к wifi или mode для смены режима работы");
  }

  if (server.hasArg("ssid") && server.hasArg("pass")) {
    strcpy(settings.wifi.ssid, server.arg("ssid").c_str());
    strcpy(settings.wifi.pass, server.arg("pass").c_str());
  }
  memory.updateNow();
  server.send(200, "text/plan", "устройство будет перезагружено");
  ESP.restart();
}

// tg
void handleTg() {  // настройка телеги
  if (!(server.hasArg("token") || server.hasArg("chat"))) {
    server.send(400, "text/plan", "укажите хотябы один параметр: token={token} chat={chat_id}");
    return;
  }

  if (server.hasArg("token")) {
    strcpy(settings.tg.token, server.arg("token").c_str());
    settings.tg.on = true;
    bot.setToken(settings.tg.token);
  }
  if (server.hasArg("chat")) {
    settings.tg.chat = server.arg("chat").toInt();
    bot.setChatID(settings.tg.chat);
  }
  memory.updateNow();
}

void handleReset() {  // сбросить настройки
  if (server.hasArg("accept") && server.arg("accept") == "true") {
    memory.reset();
    server.send(200, "text/plan", "reseting...");
    ESP.restart();
  }
}

// pages

void handleRoot() {
  server.send(200, "text/html", SP_index_page);
}

// void handleSettings() {
//  server.send(200, "text/html", SP_settings_page);
// }

void handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plane", "");
  // server.send(200, "application/json", "{}");
}

// status

void handleStatus() {
  String message = "{\n";
  message += "  \"mode\": \"";
  message += WiFi.getMode() == 1 ? "STA" : "AP";
  message += "\",\n";
  message += "  \"ssid\": \"";
  message += settings.wifi.ssid;
  message += "\",\n";
  message += "  \"tg\": \"";
  message += strlen(settings.tg.token) != 0 ? "on" : "off";
  message += "\",\n";
  message += "  \"chat\": \"";
  message += settings.tg.chat;
  message += "\"\n";
  message += "}";
  server.send(200, "application/json", message);
}


// tg bot handler

void handleTgMessage(FB_msg& msg) {
  String text = msg.text;
  if (text == "/start") {
    String mess = "Добро пожаловать!";
    mess += " Ваш chatId: ";
    mess += msg.chatID;
    bot.showMenuText(mess, "/вкл \t /выкл \t /статус", msg.chatID);
  } else {
    if (text == "/вкл") {
      switchLED(true, false);
      bot.sendMessage("Включено", msg.chatID);
    }

    if (text == "/выкл") {
      switchLED(false, false);
      bot.sendMessage("Выключено", msg.chatID);
    }

    if (text == "/статус") {
      if (ledStatus() == true) {
        bot.sendMessage("Включено", msg.chatID);
      } else {
        bot.sendMessage("Выключено", msg.chatID);
      }
    }
  }
}

void sendBotLedState() {
  if (ledStatus() == true) {
    bot.sendMessage("Включено");
  } else {
    bot.sendMessage("Выключено");
  }
}
