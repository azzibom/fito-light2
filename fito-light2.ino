// пример с настройкой логина-пароля
// если при запуске нажата кнопка на D2 (к GND)
// открывается точка WiFi Config с формой ввода
// храним настройки в EEPROM

#include <GyverPortal.h>
#include <EEManager.h>

struct WifiCfg {
  char ssid[20];
  char pass[20];
};
struct TgCfg {
  char token[50];
  uint64_t chatId;
};
struct Cfg {
  WifiCfg wifi;
  TgCfg tg;
} CFG;
EEManager memory(CFG);
GyverPortal ui;

void build() {
  GP.BUILD_BEGIN();

  GP.PAGE_TITLE("FitoLight2");    
  GP.THEME(GP_DARK);

  GP.TITLE("FitoLight2");                    // заголовок

  
  GP.BLOCK_BEGIN(GP_DIV);
  GP.SWITCH("ledSwitch", ledStatus());   
  GP.BLOCK_END();

  GP.GRID_BEGIN(720);
  GP.BLOCK_BEGIN(GP_TAB, "250", "WiFi");
  GP.FORM_BEGIN("/wifi");
  GP.TEXT("ssid", "SSID", CFG.wifi.ssid);
  GP.BREAK();
  GP.TEXT("pass", "Password", CFG.wifi.pass);
  GP.SUBMIT("Connect");
  GP.FORM_END();
  GP.BLOCK_END();

  GP.BLOCK_BEGIN(GP_TAB, "250", "Tg");
  GP.FORM_BEGIN("/tg");
  GP.TEXT("token", "Token", CFG.tg.token);
  GP.BREAK();
  GP.TEXT("chat", "ChatID", String(CFG.tg.chatId));
  GP.SUBMIT("Save");
  GP.FORM_END();
  GP.BLOCK_END();

  GP.BLOCK_BEGIN(GP_TAB, "250", "Timers");
  GP.FORM_BEGIN("/timer");
  GP.CHECK("on");
  GP.BREAK();
  GP.BOX_BEGIN(GP_CENTER); 
  GP.SPINNER("start.hour", 12, 0, 23);
  GP.SPAN(":");
  GP.SPINNER("start.minute", 0, 0, 59);
  GP.BOX_END(); 
  GP.BOX_BEGIN(GP_CENTER); 
  GP.SPINNER("end.hour", 12, 0, 23);
  GP.SPAN(":");
  GP.SPINNER("end.minute", 0, 0, 59);
  GP.BOX_END(); 
  GP.FORM_END();
  GP.BLOCK_END();
  GP.GRID_END(); 
  GP.GRID_RESPONSIVE(320);

  GP.UPDATE("ledSwitch");
  GP.BUILD_END();
}

void setup() {
  delay(2000);
  Serial.begin(115200);
  Serial.println();
  pinMode(LED_BUILTIN, OUTPUT);

  // читаем логин пароль из памяти
  EEPROM.begin(memory.blockSize());
  memory.begin(0, 252);

  // // если кнопка нажата - открываем портал
  // pinMode(D2, INPUT_PULLUP);
  // if (!digitalRead(D2)) loginPortal();
  if (strlen(CFG.wifi.ssid) == 0) {
    loginPortal();
  }

  // пытаемся подключиться
  Serial.print("Connect to: ");
  Serial.println(CFG.wifi.ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(CFG.wifi.ssid, CFG.wifi.pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! Local IP: ");

  Serial.println(WiFi.localIP());

  ui.attachBuild(build);
  ui.start();
  ui.attach(action);
}

void loginPortal() {
  Serial.println("Portal start");

  // запускаем точку доступа
  WiFi.mode(WIFI_AP);
  WiFi.softAP("WiFi Config");

  // запускаем портал
  GyverPortal uiAp;
  uiAp.attachBuild(build);
  uiAp.start();
  uiAp.attach(action);

  // работа портала
  while (uiAp.tick());
}

void action(GyverPortal& p) {
  if (p.form("/wifi")) {      // кнопка нажата
    p.copyStr("ssid", CFG.wifi.ssid);  // копируем себе
    p.copyStr("pass", CFG.wifi.pass);
    memory.updateNow();
    WiFi.softAPdisconnect();        // отключаем AP
  }

  bool valSwitch;
  if (ui.clickBool("ledSwitch", valSwitch)) {
    Serial.print("Switch: ");
    Serial.println(valSwitch);
    switchLed(valSwitch);
  }

  if (ui.update("ledSwitch")) {
    ui.updateBool("ledSwitch", ledStatus());
  }
}

void loop() {
  ui.tick();
}

bool ledStatus() {
  return !digitalRead(LED_BUILTIN);
}

void switchLed(bool on) {
  digitalWrite(LED_BUILTIN, !on);
}
