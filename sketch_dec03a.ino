/*
  *  1) EEPROM загрузка и выгрузка
  *  Статус датчиков (пока их 2)
  *  Статус реле (света???)
  *  Включение | выключие реле
  *  ???? Кнопка перезапуска ардуино на странице и в ???? реале
  *  ???? Сообщение об перезапуске в ресетпейдж после нажатия субмит
  *  ???? Автономный режим работы
  *  ???? Тестирование оборудования (интернет контроллер, датчики и т.д) через бипер или через лсд жк
  *  Убрать автоматическое обновление контролПейджа, добавить обновление страницы в момент изменения ее (статус датчика изменился -> обновить)
  *  Добавить Netmask
*/

#include <Base64.h>
#include "EtherCard.h"
#include "EEPROM.h"

/*
* Пины для датчиков движения
*/

#define SENSOR_1_PIN 4
#define SENSOR_2_PIN 5
#define SENSOR_3_PIN 6
#define SENSOR_4_PIN 7

/*
* Пин кнопки сброса
*/

#define BUTTON_RESET_PIN 2

/*
* Пин реле питания (свет)
*/

//TODO Выбрать номер пина
#define RELE_PIN 3

/*
* Максимальная длина логина и пароля
*/

#define MAX_PASSWORD_LENGTH 6
#define MAX_LOGIN_LENGTH 6

/*
* Сетевые реквизиты
*/

static byte mymac[] = {0x74,0x69,0x69,0x2D,0x30,0x32};
//static byte myip[] = {EEPROM.read(1),EEPROM.read(2),EEPROM.read(3),EEPROM.read(4)};
/*
* Настройки по умолчанию сетевых настроек
*/

//static byte defgtw[] = {};
//static byte defdns[] = {};
//static byte defnetmask = {};

/*
* Логин и пароль по умолчанию
*/

char deflogin[] = "310";
char defpassword[] = "304305";

/*
* Логин и пароль.
* Загружаются и выгружаются из EEPROM
* Задаются в SETTIGNs
*/

char main_login[6];
char main_password[6];

/*
* Перечисления страниц
*/

enum Page {
  CONTROL,
  UNKNOWN,
  SETTING,
  AUTHENTICATION,
  TEST,
  FOUND
};

/*
* Буфер. хер знает зачем нужен, пока оставим
*/

byte Ethernet::buffer[1000];

/*
* Последняя отправка сработки датчика движения
*/

long previousMillis;

/*
* Время включения питания реле (как долго будет светиться свет)
* Указывается в мсек
*/

int interval = 10000;

/*
* Данные получаемые с буфера
*/

static char* data;

/*
* Заполнитель буфера
*/

BufferFiller bfill;

bool isActivatedSession;

/*
*************************** HTML страницы ******************************
*/

static word httpTest();

/*
* Функция static word loginPage()
* Возращает страницу авторизации
*/

static word loginPage();

/*
* Функция static word resetPage()
* Возвращает страницу с настройками сетевых параметров
*/

static word resetPage();

/*
* Функция static word controlPage()
* Возвращает страницу с управлением системы
*/

static word controlPage();

/*
* Функция static word httpNotFound()
* Возращает страницу с ответом HTTP 404 Not Found (страница не найдена)
* Используется в ошибках системы
*/

static word httpNotFound();

/*
* Функция static word httpUnauthorized()
* Возвращает страницу авторизации
*/

static word httpUnauthorized();

/*
*************************** Функции ******************************
*/

/*
* Функция resetNetwork()
* Устанавливает сетевые реквизиты по умолчанию
*/

void resetNetwork();

/*
* Функция setupNetwork()
* Загружает из EEPROM сетевые реквизиты и загружает их в ENC28J60
*/

void setupNetwork();

/*
* Функция encodeBase64(char *text)
* char *text - текст, который нужно закодировать
* Возвращает закодированную в Base64 строку String без учитывания символа \0
*/

String encodeBase64(String text);

/*
* Функция void cleanEEPROM(int from, int to)
* int from - номер начального адреса ячейки EEPROM
* int to - номер последнего адреса ячейки EEPROM
* Очищает память EEPROM в заданном диапозоне
*/

void cleanEEPROM(int from, int to);

/* 
* Функция loadFromEEPROM(int pos)
* int pos - номер адреса в памяти EEPROM
* Загружает с памяти EEPROM значение в заданном адресе
*/

byte loadFromEEPROM(int pos);

/*
* Функция void loadToEEPROM(int pos, byte whatToSend)
* int post - позиция ячейки памяти, в которую производится загрузка
* whatToSend - собственно, что отправить
* Нужен для упрощения написания вызова функции EEPROM.write
*/

void loadToEEPROM(int pos, byte whatToSend);

/*
* Функция void authHandler(String log, String pass)
* Сравнивает login и password с дефолтными login и password
*   *Если login и password соотвуствуют (авторизация произошла успешно
*                                         запускает страницу controlPage
*/

void authHandler(char *request);

/*
* Функция void setPage(Page p)
* Принимает значение перечисления Page:
*   * CONTROL - переход на страницу управления
*   * SETTING - переход на страницу настроек
*   * UNKNOWN - ошибка 404 (не найдено)
*   * AUTHENTICATION - переход на страницу ввода логина и пароля
* Запускает HTML страницу
*/

void setPage(Page p);

/*
* Функция void requestHandler(char* request)
* Определяет какой запрос пришел (GET или POST);
* Передает значение request соотвествующей функции (в зависимости от типа запроса);
*/

void requestHandler(char* request);

/*
* Функция void getHandler(char* request)
* Обрабатывает GET запросы:
*   * Выход из controlPage и переход в httpUnauthorized
*   * Включение\Выключение реле
*/

void getHandler(char* request);

/*
* Функция void postHandler
* Обрабатывает POST запросы:
*   * Получение сетевых параметров
*/

void postHandler(char * request);

void setup() {

  pinMode(SENSOR_1_PIN, INPUT); // Подключение датчка D1 на вход.
  pinMode(SENSOR_2_PIN, INPUT); // Подключение датчка D2 на вход.
  pinMode(RELE_PIN, OUTPUT); //
  pinMode(BUTTON_RESET_PIN, INPUT);
  Serial.begin(9600);
  if(ether.begin(sizeof Ethernet::buffer,mymac,10) == 0) {
    Serial.println(F("Failed to access Ethernet controller"));
  }

  setupNetwork();
  // if(!ether.dhcpSetup()) Serial.println("DHCP Failed"); //Установка сетевых реквизитов по DHCP

  for (int i = 0 ; i < MAX_LOGIN_LENGTH; i++) {
    if (loadFromEEPROM(17 + i) != NULL) {
      main_login[i] = (char) loadFromEEPROM(17 + i);

    } else break;
  }

  for (int i = 0; i < MAX_PASSWORD_LENGTH; i++) {
    if (loadFromEEPROM(23 + i) != NULL) {
      main_password[i] = (char) loadFromEEPROM(23 + i);
    } else break;
  }

  Serial.println(F(""));
  Serial.println(F("----------LOAD_FROM_EEPROM------------"));
  Serial.print(F("ip = "));
  Serial.print(((byte) loadFromEEPROM(1)));
  Serial.print(F("."));
  Serial.print((byte) loadFromEEPROM(2));
  Serial.print(F("."));
  Serial.print((byte) loadFromEEPROM(3));
  Serial.print(F("."));
  Serial.print((byte) loadFromEEPROM(4));
  Serial.println(F(""));
  Serial.print(F("gtw = "));
  Serial.print((byte) loadFromEEPROM(5));
  Serial.print(".");
  Serial.print((byte) loadFromEEPROM(6));
  Serial.print(".");
  Serial.print((byte) loadFromEEPROM(7));
  Serial.print(".");
  Serial.print((byte) loadFromEEPROM(8));
  Serial.println("");
  Serial.print(F("dns = "));
  Serial.print((byte) loadFromEEPROM(9));
  Serial.print(F("."));
  Serial.print((byte) loadFromEEPROM(10));
  Serial.print(F("."));
  Serial.print((byte) loadFromEEPROM(11));
  Serial.print(F("."));
  Serial.print((byte) loadFromEEPROM(12));
  Serial.println(F(""));
  Serial.print(F("net = "));
  Serial.print((byte) loadFromEEPROM(13));
  Serial.print(F("."));
  Serial.print((byte) loadFromEEPROM(14));
  Serial.print(F("."));
  Serial.print((byte) loadFromEEPROM(15));
  Serial.print(F("."));
  Serial.print((byte) loadFromEEPROM(16));
  Serial.println(F(""));
  Serial.print(F("login = "));
  
  for(int i = 0; i < 6; i++) {
    if (main_login[i] != NULL) {
      Serial.print(main_login[i]); 
    }
  }
  
  Serial.println(F(""));
  Serial.print(F("password = "));
  
  for(int i = 0; i < 6; i++) {
    if (main_password[i] != NULL) {
      Serial.print(main_password[i]);
    }
  }

  // Serial.println(F(""));
  // Serial.println(F("--------------------------------------"));
  // Serial.println("");
  // Serial.println(F("----------------CONFIG----------------"));
  // ether.printIp("Ip: ", ether.myip);
  // ether.printIp("Netmask: ", ether.netmask);
  // ether.printIp("GW Ip:" ,ether.gwip);
  // ether.printIp("DNS Ip:", ether.dnsip);
  // Serial.println(F("--------------------------------------"));

}

void loop() {

  word pos = ether.packetLoop(ether.packetReceive());

  if (digitalRead(BUTTON_RESET_PIN) == HIGH) {
    // EEPROM.write(0,1); // записываем в 0 ячейку сигнал о том, что произошел сброс
    delay(1000);
    resetNetwork();
  }

  // Если хотя бы один датчик сработал -> присваеваем значение последней отправки значение время работы ардуино.
  if (checkSensor()) {
    previousMillis = millis();
  }

  // Секундомер
  if (millis() - previousMillis <= interval) {
    digitalWrite(RELE_PIN, 1);
  } else { digitalWrite(RELE_PIN, 0); }



  // Если пришел запрос - начинаем обрабатывать его
  if(pos) {
    bfill = ether.tcpOffset();
    data = (char *) Ethernet::buffer + pos;
    requestHandler(data);
    authHandler(data);
  }
}

void setupNetwork() {
  //TODO сделать загрузку из EEPROM
  static byte ip[4] = {loadFromEEPROM(1), loadFromEEPROM(2), loadFromEEPROM(3), loadFromEEPROM(4)};
  static byte netmask[4] = {loadFromEEPROM(5), loadFromEEPROM(6), loadFromEEPROM(7), loadFromEEPROM(8)};
  static byte gtw[4] = {loadFromEEPROM(13), loadFromEEPROM(14), loadFromEEPROM(15), loadFromEEPROM(16)};
  static byte dns[4] = {loadFromEEPROM(9), loadFromEEPROM(10), loadFromEEPROM(11), loadFromEEPROM(12)};
  ether.staticSetup(ip, gtw, dns, netmask);

  Serial.println(F("----------------CONFIG----------------"));
  ether.printIp("Ip: ", ether.myip);
  ether.printIp("Netmask: ", ether.netmask);
  ether.printIp("GW Ip:" ,ether.gwip);
  ether.printIp("DNS Ip:", ether.dnsip);
  Serial.println(F("--------------------------------------"));
}

void resetNetwork() {

  static byte ip[4] = {192, 168, 1, 113};
  static byte netmask[4] = {255, 255, 255, 0};
  static byte dns[4] = {192, 168, 1, 1};
  static byte gtw[4] = {192, 168, 1, 1};
  ether. staticSetup(ip,gtw,dns,netmask);
  Serial.println(F("----------------CONFIG----------------"));
  ether.printIp("Ip: ", ether.myip);
  ether.printIp("Netmask: ", ether.netmask);
  ether.printIp("GW Ip:" ,ether.gwip);
  ether.printIp("DNS Ip:", ether.dnsip);
  Serial.println(F("--------------------------------------"));
}

String encodeBase64(String text) {
  char temp[text.length()];
  strcpy(temp, text.c_str());
  int encodedLength = Base64.encodedLength(sizeof(temp));
  char encodedString[encodedLength];
  Base64.encode(encodedString, temp, sizeof(temp));
  return String(encodedString);
}

void cleanEEPROM(int from, int to) {
  for (int i = from; i < to; i++) {
    EEPROM.write(i, 0);
  }
}

byte loadFromEEPROM(int pos) {
  return EEPROM.read(pos);
}

void loadToEEPROM(int pos, byte whatToSend) {
  EEPROM.write(pos, whatToSend);
}

void setPage(Page p) {

  switch(p) {
    case CONTROL: {
      ether.httpServerReply(controlPage());
      break;
    } 

    case SETTING: {
      ether.httpServerReply(resetPage());
      break;
    }

    case UNKNOWN: {
      ether.httpServerReply(httpNotFound());
      break;
    }

    case AUTHENTICATION: {
      ether.httpServerReply(httpUnauthorized());
      break;
    }
    case TEST: {
      ether.httpServerReply(httpTest());
    }
    case FOUND:
      ether.httpServerReply(http_Found());
  }
}

void authHandler(char *request) {

  String temp;
  temp += main_login;
  temp += ":";
  temp += main_password;
  Serial.print(F("\n Authorization: "));
  Serial.print(temp);
  Serial.println(F(""));
  String authHash = encodeBase64(temp);
  if (strstr(request, authHash.c_str()) != NULL) {
    setPage(CONTROL);
  } else setPage(AUTHENTICATION);
}

void postHandler(char* request) {

  if (strstr(request, "ip=") != NULL) {
    char *post = strstr(request, "ip=");
    char *buffer;
    char *token = strtok_r(post, "&", &buffer);// делим на токены post_pos, разделитель &
    byte i = 1; // нумерация токенов
    while (token != NULL) {
      if (i == 1) {
        char* ip = token;
        if (ip[3] != NULL) {
          ip = strtok(ip, "ip=");
          char* ip_token = strtok(ip, ".");
          byte n = 1;
          while (ip_token != NULL) {
            //Функция atoi(char * foo) преобразует символьный массив и возвращает число
            if (n == 1) loadToEEPROM(1, (byte) atoi(ip_token));
            if (n == 2) loadToEEPROM(2, (byte) atoi(ip_token));
            if (n == 3) loadToEEPROM(3, (byte) atoi(ip_token));
            if (n == 4) loadToEEPROM(4, (byte) atoi(ip_token));
            ip_token = strtok(NULL,".");
            n++;
          }
        } else ip = 0;
      }

      if (i == 2) {
        char* gtw = token;
        if (gtw[4] != NULL) {
          gtw = strtok(gtw, "gtw=");
          char* gtw_token = strtok(gtw, ".");
          int n = 1;
          while (gtw_token != NULL) {
            if (n == 1) loadToEEPROM(5, (byte) atoi(gtw_token));
            if (n == 2) loadToEEPROM(6, (byte) atoi(gtw_token));
            if (n == 3) loadToEEPROM(7, (byte) atoi(gtw_token));
            if (n == 4) loadToEEPROM(8, (byte) atoi(gtw_token));
            gtw_token = strtok(NULL,".");
            n++;
          }
        } else gtw = 0;
      }

      if (i == 3) {
        char* dns = token;
        if (dns[4] != NULL) {
          dns = strtok(dns, "dns=");
          char* dns_token = strtok(dns,".");
          int n = 1;
          while (dns_token != NULL) {
            if (n == 1) loadToEEPROM(9, (byte) atoi(dns_token));
            if (n == 2) loadToEEPROM(10, (byte) atoi(dns_token));
            if (n == 3) loadToEEPROM(11, (byte) atoi(dns_token));
            if (n == 4) loadToEEPROM(12, (byte) atoi(dns_token));
            dns_token = strtok(NULL,".");
            n++;
          }
        } else dns = 0;
      }
      if (i == 4) {
        char* subm = token;
        if (subm[5] != NULL) {
          subm = strtok(subm, "subm=");
          char* subm_token = strtok(subm,".");
          int n = 1;
          while (subm_token != NULL) {
            if (n == 1) loadToEEPROM(13, (byte) atoi(subm_token));
            if (n == 2) loadToEEPROM(14, (byte) atoi(subm_token));
            if (n == 3) loadToEEPROM(15, (byte) atoi(subm_token));
            if (n == 4) loadToEEPROM(16, (byte) atoi(subm_token));
            subm_token = strtok(NULL, ".");
            n++;
          }
        }
      }
      if (i == 5) {
        char *login = token;
        if (login[4] != NULL) { 
          login = strtok(login, "log=");
          
          /* Перед загрузкой очищаем память в заданном диапооне
          *  Это делается для того, чтобы очистить старый логин
          *  Если не производить очистку, то новый логин смешается со старым
          */

          cleanEEPROM(17, 23); 

          for (int i = 0; i < 6; i++) {
            if (login[i] != NULL) { // Если встречен конец строки - прекращаем
              loadToEEPROM(17 + i, login[i]);
              main_login[i] = login[i];
            } else break;
          }
        }
      }
      if (i == 6) {
        char *password = token;
        if (password[5] != NULL) { // Если данные новые не пришли, то ничего не делаем
          password = strtok(password, "pass=");

          cleanEEPROM(23, 29);

          for (int i = 0; i < 6; i++) {
            if (password[i] != NULL) {
              loadToEEPROM(23 + i, password[i]);
              main_password[i] = password[i];
            } else break;
          }
        }
      }
      token = strtok_r(NULL, "&", &buffer); // выделение следующей части строки (поиск нового токена и выделение его)
      i++; //нужен для определения какой на данный момент номер токена
    }

  } if (strstr(request, "settings=SETTINGS") != NULL) {
    setPage(SETTING);
  } if (strstr(request, "Authorization") != NULL) {
    authHandler(request);
  }
}

void getHandler(char* request) {
  if (strstr(request, "Authorization") != NULL) {
    authHandler(request);
  }
}

/*
* Если найдено совпадение с запросом GET, то передаем управление функции getHandler(char* request)
* Тоже самое и с запросом POST
* Если нет ниодного совпадения - отображаем страницу 404 Not Found
*/

void requestHandler(char* request) {
  if (strstr(request, "GET /") != NULL) {
    getHandler(request);
  } else if (strstr(request, "POST /") != NULL) {
    postHandler(request);
  } else {
    setPage(UNKNOWN);
  }
}

static word httpNotFound() {

  bfill.emit_p(PSTR(
    "HTTP/1.0 404 Not Found"
  ));
  return bfill.position();
}

static word httpTest() {
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "<title> Test </title>"
    "<form method = 'post'>"
    "<p> <input type = 'text' name = 'text' size = 20></p>"
    "<p> <input type = 'submit' value = 'submit'>  </p>"
    "</form>"
    "<a href = '?test=test'> TEST </a>"
    ));
  return bfill.position();
}

static word resetPage() {

  // bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 202 Accepted\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "<title> Reset Page </title>"
    "<body text = '#505452' bgcolor = '#f2f2f2'>"
    "</body>"
    "<h2 align = 'center'> Setup Ethernet Arduino </h2>"
    "<hr>"
    "<p align = 'right'>by Savinov (KS-234) ver 1.0.2018</p>"
    "<center>"
    "<form method = 'post'>"
    "<p> <em> IP address: </em> </p>"
    "<p> <input type = 'text' name = 'ip' size = 20> </p>"
    "<p> <em> Gateway address: </em> </p>"
    "<input type = 'text' name = 'gtw' size = 20>"
    "<p> <em> DNS address: </em> </p>"
    "<input type = 'text' name = 'dns' size = 20>"
    "<p> <em> Subnet mask </em> </p>"
    "<p> <input type = 'text' name = 'subm' size = 20>"
    "<p> <h3> Login Setup </h3> </p>"
    "<p> <em> Login </em> </p>"
    "<p> <input type = 'number' name = 'log'> </p>"
    "<p> <em> Password </em> </p>"
    "<p> <input type = 'number' name = 'pass'> </p>"
    "<p> <input type = 'submit' value = 'Submit'> </p>"
  "</center>"

));
  return bfill.position();
}

static word httpUnauthorized() {
  // bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 401 Unauthorized\r\n"
    "WWW-Authenticate: Basic realm=\"Arduino\""
    "Content-Type: text/html\r\n\r\n"));
  return bfill.position();
}

static word controlPage() {

  String IP;
  IP = EEPROM.read(1);
  IP += ".";
  IP += EEPROM.read(2);
  IP += ".";
  IP += EEPROM.read(3);
  IP += ".";
  IP += EEPROM.read(4);

  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "<title> Control Page </title>"
    "<body text = '#505452' bgcolor = '#f2f2f2'>"
    "<h2 align = 'center'> Control Arduino </h2>"
    "<hr>"
    "<p align = 'right'>by Savinov (KS-234) ver 1.0.2018</p>"
    "<center>"
    "<p> ledStatus:</p>"
    "<form method='post'>"
    "<input type = 'submit' value = 'SETTINGS' name='settings'>"
    "</form>"
    "<p> <a href='http://log:out@$S/'> EXIT </p>"
    "</center>"
  ), IP.c_str());
    return bfill.position();
}

static word http_Found() {
  bfill.emit_p(PSTR(
    "HTTP/1.1 302 Found\r\n"
    "Location: /\r\n\r\n"));
  return bfill.position();
}
// Возращает true, если хотя бы один из датчиков сработал.
boolean checkSensor() {
  if (digitalRead(SENSOR_1_PIN) || digitalRead(SENSOR_2_PIN)) {
    return true;
  } else return false;
}
