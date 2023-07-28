#include <GyverPortal.h>
#include <FileData.h>
#include <LittleFS.h>
#include <GyverNTP.h>
#include <FastBot.h>

/*
Прошивка для управления фитолентой.
- управление по web ui и tg
- установка таймеров на вкл/выкл
- настройка wifi/tg/таймеров с сохранением
- синхронизация времени

TODO
- вынести настройку часового пояса
- загрузка и выгрузка файла настроек ui и tg
- настройка таймеров через tg
- обновление прошивки через ui и tg
- вывод информации о состоянии, текущем времени и тд в ui и tg
*/

// = DEFINE ===
#define APP_TITLE "FitoLight2"
#define AP_PASS "12345687"
#define TIMER_COUNT 3 // кол-во таймеров
#define MEM_KEY 'A' // ключ сброса памяти
#define GMT 3 // часовой пояс
#define TOUT 10000 // таймаут сохранения данных
#define REL_PIN D1 // тин

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
// EEManager memory(CFG);
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

  pinMode(LED_BUILTIN, OUTPUT);
  // pre init end
  readSettings(); // читаем из памяти
  runWifi(); // запускаем wifi
  
  ui.attachBuild(buildUI);
  ui.start(APP_TITLE);
  ui.attach(uiCallback);

  ntp.begin();

  // == bot ==
  Serial.println("== Tg bot starting... ==");
  bot.setToken(CFG.tg.token);
  bot.setChatID(CFG.tg.chatId);
  bot.skipUpdates();
  bot.attach(tgCallback);
  bot.showMenuText("Я снова в деле", "/вкл \t /выкл \t /статус");
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
  return !digitalRead(LED_BUILTIN);
}

void switchLed(bool on, bool sendAnswerToTg) {
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
  WiFi.setAutoConnect(true);
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

    if (WiFi.status() != WL_CONNECTED) {
      startAP();
    } else {
      Serial.println();
      Serial.print("Connected! Local IP: ");
      Serial.println(WiFi.localIP());
      
    }
    Serial.println("== Wifi started ==");
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
    data.update();
    WiFi.softAPdisconnect();  // отключаем AP
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
      Timer t = CFG.timers[timerNum];
      String beginPref = "begin"; beginPref += timerNum; 
      String endPref = "end"; endPref += timerNum;
      String beginHour = beginPref; beginHour += ".hour"; 
      String beginMinute = beginPref; beginMinute += ".minute";
      String endHour = endPref; endHour += ".hour"; 
      String endMinute = endPref; endMinute += ".minute";
      String onPref = "on"; onPref += timerNum; 
      p.copyBool(onPref, CFG.timers[timerNum].on); 
      p.copyInt(beginHour, CFG.timers[timerNum].begin.hour);
      p.copyInt(beginMinute, CFG.timers[timerNum].begin.minute);
      p.copyInt(endHour, CFG.timers[timerNum].end.hour);
      p.copyInt(endMinute, CFG.timers[timerNum].end.minute);
      Serial.println("== timer ==");
      Serial.print("on: ");
      Serial.println(CFG.timers[timerNum].on);
      Serial.print("begin: ");
      Serial.print(CFG.timers[timerNum].begin.hour);
      Serial.print(":");
      Serial.println(CFG.timers[timerNum].begin.minute);
      Serial.print("end: ");
      Serial.print(CFG.timers[timerNum].end.hour);
      Serial.print(":");
      Serial.println(CFG.timers[timerNum].end.minute);
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
    bot.showMenuText(mess, "/вкл \t /выкл \t /статус", msg.chatID);
  } else {
    if (text == "/вкл") {
      switchLed(true, false);
      bot.sendMessage("Включено", msg.chatID);
    }

    if (text == "/выкл") {
      switchLed(false, false);
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

void sendBotLedState() { // отправка состояния в TG
  if (ledStatus() == true) {
    bot.sendMessage("Включено");
  } else {
    bot.sendMessage("Выключено");
  }
}
