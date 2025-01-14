#include <GyverPortal.h>
#include <FileData.h>
#include <LittleFS.h>
#include <GyverNTP.h>
#include <FastBot.h>
#include <GParser.h>

/*
Прошивка для управления фитолентой.
версия 2.1
- управление по web ui и tg
- установка таймеров на вкл/выкл (через web ui)
- настройка wifi/tg/таймеров с сохранением в ФС
- синхронизация времени через интернет

TODO
- вынести настройку часового пояса
- вынести настройку имени хоста
- загрузка и выгрузка файла настроек ui и tg
- настройка таймеров через tg
- обновление прошивки через ui и tg
- вывод информации о состоянии, текущем времени и тд в ui и tg
*/

// = DEFINE ===
#define APP_TITLE "FitoLight2"
#define HOSTNAME "fito-light2"
#define AP_PASS "12345687" // !!
#define TIMER_COUNT 3 // кол-во таймеров
#define MEM_KEY 'A' // ключ сброса памяти
#define GMT 3 // часовой пояс
#define TOUT 10000 // таймаут сохранения данных
#define REL_PIN D1 // пин
String menu = "вкл \t выкл \t статус \n таймеры";

// = STRUCT ===
struct WifiCfg {
  char ssid[20];
  char pass[20];
};
struct TgCfg {
  char token[50];
  uint64_t chatId;
};
struct Clock {
  int8_t hour = 12;
  int8_t minute = 0;
};
struct Timer {
  bool on = false;
  Clock begin;
  Clock end;
};
struct Cfg {
  WifiCfg wifi;
  TgCfg tg;
  Timer timers[TIMER_COUNT];
} CFG;

// = VAR ===
FileData data(&LittleFS, "/settings.dat", MEM_KEY, &CFG, sizeof(CFG), 10000);
GyverPortal ui;
GyverNTP ntp(GMT);
FastBot bot;

// = main ==
void setup() {
  // pre init begin
  delay(2000);

  Serial.begin(115200);
  Serial.println();

  pinMode(REL_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  switchLed(false, false);
  // pre init end
  readSettings(); // читаем из памяти
  runWifi(); // запускаем wifi

  // конфигурируем ui
  ui.attachBuild(buildUI);
  ui.start(HOSTNAME);
  ui.setFS(&LittleFS);
  ui.attach(uiCallback);

  ntp.begin(); // запускаем синхронизацию времени

  // == конфигурируем tg бота ==
  Serial.println("== Tg bot starting... ==");
  bot.setToken(CFG.tg.token);
  bot.setChatID(CFG.tg.chatId);
  bot.skipUpdates();
  bot.attach(tgCallback);
  bot.showMenuText("Я снова в деле", menu);
  Serial.println("== Tg bot started ==");
}

bool begin = false; // флаг необходимости вкл/выключить подсветку по таймеру 
void loop() {
  // = tick begin =
  ui.tick();
  ntp.tick();
  bot.tick();
  data.tick();
  // = tick end =

  lightTimerLoop();
}

void lightTimerLoop() { // обработка таймеров подсветки
  for (int i = 0; i < TIMER_COUNT; i++) { 
    Timer t = CFG.timers[i];
    if (!t.on) {
      continue;
    }
    if (ntp.hour() == t.begin.hour && ntp.minute() == t.begin.minute && !begin) {
      switchLed(true);
      Serial.println("alarm on");
      begin = true;
    } 
    if (ntp.hour() == t.end.hour && ntp.minute() == t.end.minute && begin) {
      switchLed(false);
      Serial.println("alarm off");
      begin = false;
    }
  }
}

// = led ======
bool ledStatus() {
  return digitalRead(REL_PIN);
}

void switchLed(bool on, bool sendAnswerToTg) {
  digitalWrite(REL_PIN, on);
  digitalWrite(LED_BUILTIN, !on);
   if (sendAnswerToTg) {
    sendBotLedState();
  }
}

void switchLed(bool on) {
  switchLed(on, true);
}

// = memory ===
void readSettings() { // читаем настройки
  LittleFS.begin();
  FDstat_t stat = data.read();

  switch (stat) {
    case FD_FS_ERR: Serial.println("FS Error"); break;
    case FD_FILE_ERR: Serial.println("Error"); break;
    case FD_WRITE: Serial.println("Data Write"); break;
    case FD_ADD: Serial.println("Data Add"); break;
    case FD_READ: Serial.println("Data Read"); break;
    default: break;
  }
}

// = wifi =====
void startAP() { // подымаем точку AP
  Serial.println("== AP starting... ==");
  // запускаем точку доступа
  WiFi.mode(WIFI_AP);
  WiFi.softAP(APP_TITLE, AP_PASS);
  Serial.println("== AP started... ==");
}

void runWifi() { // запускаем WiFi
  Serial.println("== Wifi starting... ==");
  WiFi.softAPdisconnect();
  // WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.hostname(HOSTNAME);
  if (strlen(CFG.wifi.ssid) == 0) {
    startAP();
  } else {
    // пытаемся подключиться
    Serial.print("Connect to: ");
    Serial.println(CFG.wifi.ssid);
    WiFi.mode(WIFI_STA);
    if (strlen(CFG.wifi.pass) == 0) {
      WiFi.begin(CFG.wifi.ssid);
    } else {
      WiFi.begin(CFG.wifi.ssid, CFG.wifi.pass);
    }

    long now = millis();
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      if (millis() - now >= 30000) break;  // даем на подключение (сек)
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
      startAP();
    } else {
      Serial.print("Connected! Local IP: ");
      Serial.println(WiFi.localIP());
      
    }
    Serial.println("== Wifi started ==");

    // NBNS.begin(HOSTNAME);
  }
}

// = ui =======
void buildUI() {
  GP.BUILD_BEGIN();

  GP.PAGE_TITLE(APP_TITLE);
  GP.THEME(GP_DARK);
  GP.TITLE(APP_TITLE);
  // LED
  GP.BLOCK_BEGIN(GP_TAB, "");
  GP.SWITCH("ledSwitch", ledStatus());
  GP.BLOCK_END();
  // WiFi
  GP.BLOCK_BEGIN(GP_TAB, "", "WiFi");
  GP.FORM_BEGIN("/wifi");
  GP.TEXT("ssid", "SSID", CFG.wifi.ssid);
  GP.BREAK();
  GP.TEXT("pass", "Password", CFG.wifi.pass);
  GP.SUBMIT("Connect");
  GP.FORM_END();
  GP.BLOCK_END();
  // Tg
  GP.BLOCK_BEGIN(GP_TAB, "", "Tg");
  GP.FORM_BEGIN("/tg");
  GP.TEXT("token", "Token", CFG.tg.token);
  GP.BREAK();
  GP.TEXT("chatId", "ChatID", String(CFG.tg.chatId));
  GP.SUBMIT("Save");
  GP.FORM_END();
  GP.BLOCK_END();

  GP.BLOCK_BEGIN(GP_TAB, "", "Timers");
  for (int i = 0; i < TIMER_COUNT; i++) {
    Timer timer = CFG.timers[i];
    String title = "#"; title += i;
    GP.BLOCK_BEGIN(GP_TAB, "", title);
    GP.FORM_BEGIN("/timer");
    String onPref = "on"; onPref += i;
    GP.SWITCH(onPref, timer.on);
    GP.BREAK();
    String beginPref = "begin"; beginPref += i; 
    String endPref = "end"; endPref += i; 
    buildClockUI(timer.begin, beginPref);
    buildClockUI(timer.end, endPref);
    GP.HIDDEN("timer", String(i));
    GP.SUBMIT_MINI("Save");
    GP.FORM_END();
    GP.BLOCK_END();
  }
  GP.BLOCK_END();

  GP.UPDATE("ledSwitch");
  GP.BUILD_END();
}

void buildClockUI(Clock c, String prefix) {
  GP.BOX_BEGIN(GP_CENTER);
  GP.SPINNER(prefix + ".hour", c.hour, 0, 23);
  GP.SPAN(":");
  GP.SPINNER(prefix + ".minute", c.minute, 0, 59);
  GP.BOX_END();
}

void uiCallback(GyverPortal& p) { // обработка запросов с ui
  wifiFormAction(p);
  tgFormAction(p); 
  timerFormAction(p);

  ledSwitchAction(p);
  updateDynamycElsAction(p);
}

void wifiFormAction(GyverPortal& p) { // обратотка формы wifi
  if (p.form("/wifi")) {               // кнопка нажата
    p.copyStr("ssid", CFG.wifi.ssid);  // копируем себе
    p.copyStr("pass", CFG.wifi.pass);
    FDstat_t stat = data.updateNow();
    Serial.print("uptate status: ");
    switch (stat) {
      case FD_FS_ERR: Serial.println("FS Error"); break;
      case FD_FILE_ERR: Serial.println("Error"); break;
      case FD_WRITE: Serial.println("Data Write"); break;
      case FD_ADD: Serial.println("Data Add"); break;
      case FD_READ: Serial.println("Data Read"); break;
      default: break;
    }
    ESP.restart();
  }
}

void tgFormAction(GyverPortal& p) { // обработка формы телеграм
  if (p.form("/tg")) {
    p.copyStr("token", CFG.tg.token);
    p.copyInt("chatId", CFG.tg.chatId);
    data.update();
    bot.setToken(CFG.tg.token);
    bot.setChatID(CFG.tg.chatId);
    Serial.print("token: "); Serial.println(CFG.tg.token);
    Serial.print("chatId: "); Serial.print(CFG.tg.chatId);
  }
}

void timerFormAction(GyverPortal& p) { // обратотка формы таймера
  if (p.form("/timer")) {
    int timerNum = -1;
    p.copyInt("timer", timerNum);
    Serial.print("timer: ");
    Serial.println(timerNum);
    if (timerNum < 0 || timerNum >= TIMER_COUNT) {
      Serial.println("timer not found");
    } else {
      Timer timer = CFG.timers[timerNum];
      String beginPref = "begin"; beginPref += timerNum; 
      String endPref = "end"; endPref += timerNum;
      String beginHour = beginPref; beginHour += ".hour"; 
      String beginMinute = beginPref; beginMinute += ".minute";
      String endHour = endPref; endHour += ".hour"; 
      String endMinute = endPref; endMinute += ".minute";
      String onPref = "on"; onPref += timerNum; 
      p.copyBool(onPref, timer.on); 
      p.copyInt(beginHour, timer.begin.hour);
      p.copyInt(beginMinute, timer.begin.minute);
      p.copyInt(endHour, timer.end.hour);
      p.copyInt(endMinute, timer.end.minute);
      Serial.println("== timer ==");
      Serial.print("on: ");
      Serial.println(timer.on);
      Serial.print("begin: ");
      Serial.print(timer.begin.hour);
      Serial.print(":");
      Serial.println(timer.begin.minute);
      Serial.print("end: ");
      Serial.print(timer.end.hour);
      Serial.print(":");
      Serial.println(timer.end.minute);
      Serial.println("===========");
      data.update();
    }
  }
}

void ledSwitchAction(GyverPortal& p) { // обратотка переключения  
  bool valSwitch;
  if (ui.clickBool("ledSwitch", valSwitch)) {
    Serial.print("Switch: ");
    Serial.println(valSwitch);
    switchLed(valSwitch);
  }
}

void updateDynamycElsAction(GyverPortal& p) { // обновление динамических эл-ов на ui
  if (ui.update("ledSwitch")) {
    ui.updateBool("ledSwitch", ledStatus());
  }
}

// = tg =======
void tgCallback(FB_msg& msg) { // обработка запросов с tg
  String text = msg.text;
  if (text == "/start") {
    String mess = "Добро пожаловать!";
    mess += " Ваш chatId: ";
    mess += msg.chatID;
    bot.showMenuText(mess, menu, msg.chatID);
  } else {
    if (text == "вкл") {
      switchLed(true, false);
      bot.showMenuText("Включено", menu, msg.chatID);
      return;
    }

    if (text == "выкл") {
      switchLed(false, false);
      bot.showMenuText("Выключено", menu, msg.chatID);
      return;
    }

    if (text == "статус") {
      String answer = "подсветка: ";
      answer += ledStatus() == true ? "ВКЛ" : "ВЫКЛ";
      answer += "\n===========\n";

      answer += "Портал: ";
      answer += "http://";
      answer += WiFi.localIP().toString();
      answer += "\n";
      
      answer += "chatId: ";
      answer += msg.chatID;
      answer += "\n";

      answer += "== TIMERS ==\n";
      for (int i = 0; i < TIMER_COUNT; i++) {
        Timer timer = CFG.timers[i];
        answer += "#"; answer += String(i);
        answer += ": "; answer += timer.on ? "ВКЛ" : "ВЫКЛ";
        answer += " (";
        answer += String(timer.begin.hour); answer += ":"; answer += String(timer.begin.minute);
        answer += " - "; 
        answer += String(timer.end.hour); answer += ":"; answer += String(timer.end.minute);
        answer += ")\n";
      }
      answer += "===========\n";

      bot.showMenuText(answer, menu, msg.chatID);
      return;
    }

    if(text == "timers") {
      String data = msg.data; // t.[1|2|3].[on|off]
      GParser data(data, '.');
      int am = data.amount();
      if (am < 3) {
        bot.showMenuText("ошибочная команда", menu, msg.chatID);
        return;
      }
      String timerNum = data.getInt(1);
      if (timerNum < 0 || timerNum >= TIMER_COUNT) {
        bot.showMenuText("ошибочная команда", menu, msg.chatID);
        return;
      }
      bool on = data[2] == "on" ? true : false;
      CFG.timers[i].on = on;
    }

    if(text == "таймеры" || text == "timers") {
      Serial.println(text);
      String menu1 = "";
      String cback1 = "";
      for (int i = 0; i < TIMER_COUNT; i++) {
        Timer timer = CFG.timers[i];
        menu1 += "#"; menu1 += String(i); menu1 += timer.on ? "(Вкл)" : "(Выкл)";
        if (i + 1 != TIMER_COUNT) {
          menu1 += "\t";
        }
        cback1 += "t."; cback1 += String(i);
        cback1 += timer.on ? ".off" : ".on";
        if (i + 1 != TIMER_COUNT) {
          cback1 += ",";
        }
      }
      Serial.println(menu1);
      bot.inlineMenuCallback("timers", menu1, cback1, msg.chatID);
    }
  }
}


void sendBotLedState() { // отправка состояния в TG
  if (ledStatus() == true) {
    bot.sendMessage("Включено");
  } else {
    bot.sendMessage("Выключено");
  }
}
