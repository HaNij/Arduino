/*
  *  1) EEPRROM загрузка и выгрузка
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

#include "EtherCard.h"
#include "EEPROM.h"
#include <rBase64.h>

/*
* Пины для датчиков движения
*/

#define SENSOR_1_PIN 10
#define SENSOR_2_PIN 11
#define SENSOR_3_PIN 12
#define SENSOR_4_PIN 13

/*
* Пин кнопки сброса
*/

//TODO Выбрать номер пина
#define BUTTON_RESET_PIN 0

/*
* Пин реле питания (свет)
*/

//TODO Выбрать номер пина
#define RELE_PIN 0

static byte mymac[] = {0x74,0x69,0x69,0x2D,0x30,0x32};
static byte myip[] = {EEPROM.read(1),EEPROM.read(2),EEPROM.read(3),EEPROM.read(4)};
static byte defip[] = {192,168,0,13};

/*
* Настройки по умолчанию сетевых настроек
*/

static byte defgtw[] = {};
static byte defdns[] = {};
static byte defnetmask = {};

/*
* Логин и пароль по умолчанию
*/

char *deflogin = "123";
char *defpassword = "123";

/*
* Перечисления страниц
*/

enum Page {
  CONTROL,
  UNKNOWN,
  SETTING,
  AUTHENTICATION
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
* Функция void authHandler(char *log, char *pass)
* Сравнивает login и password с дефолтными login и password
*   *Если login и password соотвуствуют (авторизация произошла успешно
*                                         запускает страницу controlPage
*/

void authHandler(char *log, char *pass);

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
  isActivatedSession = false;
  EEPROM.write(0,0);
  pinMode(SENSOR_1_PIN, INPUT); // Подключение датчка D1 на вход.
  pinMode(SENSOR_2_PIN, INPUT); // Подключение датчка D1 на вход.
  pinMode(RELE_PIN, OUTPUT); // Подключение светодиода S3 на выход.
  Serial.begin(9600);
  if(ether.begin(sizeof Ethernet::buffer,mymac,10) == 0) {
    Serial.println("Failed to access Ethernet controller");
  } else Serial.println("Ethernet controller is ok");
  if(EEPROM.read(0) == 1) {
    ether.staticSetup(defip);
  } else if (EEPROM.read(0) == 0) {
    ether.staticSetup(myip);
  } else Serial.println("Error with EEPROM");

  if(!ether.dhcpSetup()) Serial.println("DHCP Failed"); //Установка сетевых параметров по DHCP
  ether.printIp("Ip: ", ether.myip);
  ether.printIp("Netmask: ", ether.netmask);
  ether.printIp("GW Ip:" ,ether.gwip);
  ether.printIp("DNS Ip:", ether.dnsip);
}

void loop() {
  word len = ether.packetReceive();
  word pos = ether.packetLoop(len);

  /* Будет кнопка - будет работать)
  if (digitalRead(R1)) {
    EEPROM.write(0,1); // записываем в 0 ячейку сигнал о том, что произошел сброс
    delay(1000);
    Serial.println("Hello");
    restartArduino(); // рестарт ардуино и переход на страницу настройки

  }
 */

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
    data = (char *) Ethernet::buffer + pos;
    requestHandler(data);
    if (EEPROM.read(0) == 1) {
      setPage(SETTING);
    } else  if (EEPROM.read(0) == 0) {
      if (isActivatedSession) {
        setPage(CONTROL);
      } else setPage(AUTHENTICATION);
    }
  }
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
      ether.httpServerReply(loginPage());
      break;
    }
  }
}

void authHandler(char *log, char *pass) {
  if (strcmp(log, deflogin) == 0 && strcmp(pass, defpassword) == 0) {
    isActivatedSession = true;
    setPage(CONTROL);
  } else {
    isActivatedSession = false;
    setPage(AUTHENTICATION);
  }
}

void postHandler(char* request) {
  if (strstr(request, "ip=") != NULL) {
    char *post = strstr(request, "ip=");
    char *buffer;
    char *token = strtok_r(post, "&", &buffer);// делим на токены post_pos, разделитель &
    byte i = 1; // нумерация токенов
    while (token != NULL) {
      // EEPROM.write(0, 0) // при перезагрузки мы перейдем в controlPage()
      if (i == 1) {
        char* ip = token;
        if (ip[3] != NULL) {
          ip = strtok(ip, "ip=");
          char* ip_token = strtok(ip, ".");
          byte n = 1;
          while (ip_token != NULL) {
            //Функция atoi(char *) преобразует символьный массив и возвращает число
            if (n == 1) EEPROM.write(1, atoi(ip_token)); //Serial.println((byte) atoi(ip_token));
            if (n == 2) EEPROM.write(2, atoi(ip_token));
            if (n == 3) EEPROM.write(3, atoi(ip_token));
            if (n == 4) EEPROM.write(4, atoi(ip_token));
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
            if (n == 1) EEPROM.write(5, atoi(gtw_token));
            if (n == 2) EEPROM.write(6, atoi(gtw_token));
            if (n == 3) EEPROM.write(7, atoi(gtw_token));
            if (n == 4) EEPROM.write(8, atoi(gtw_token));
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
            if (n == 1) EEPROM.write(9, atoi(dns_token));
            if (n == 2) EEPROM.write(10, atoi(dns_token));
            if (n == 3) EEPROM.write(11, atoi(dns_token));
            if (n == 4) EEPROM.write(12, atoi(dns_token));
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
            if (n == 1) EEPROM.write(13, atoi(subm_token));
            if (n == 2) EEPROM.write(14, atoi(subm_token));
            if (n == 3) EEPROM.write(15, atoi(subm_token));
            if (n == 4) EEPROM.write(16, atoi(subm_token));
            subm_token = strtok(NULL, ".");
            n++;
          }
        } else subm = 0;
      }
      token = strtok_r(NULL, "&", &buffer); // выделение следующей части строки (поиск нового токена и выделение его)
      i++; //нужен для определения какой на данный момент номер токена
    }
  } if (strstr(request, "log=") != NULL) {
      char *post = strstr(request, "log=");
      char *buffer;
      char *token = strtok_r(post, "&", &buffer);
      char *login;
      char *password;
      byte i = 1;
      while (token != NULL) {
        if (i == 1) {
          /*
          *  Это делается для того, чтобы не трогать выделенную часть token'а
          *   т.к в дальнейшем будем работать с токеном
          */
          login = token;
          login = strtok(login, "log=");
        }
        if (i == 2) {
          password = token;
          password = strtok(password, "pass=");
        }
        token = strtok_r(NULL, "&", &buffer);
        i++;
      }
      Serial.println(login);
      Serial.println(password);
      authHandler(login, password);
    } else {
      Serial.println("Error occured: Which post?");
  }
}

void getHandler(char* request) {
  if (strstr(request, "GET /?EXIT") != NULL) {
    setPage(AUTHENTICATION);
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
    Serial.println("POST");
    postHandler(request);
  } else {
    setPage(UNKNOWN);
  }
}

/*
* TODO Проблема в сохранении сессии
*/

static word loginPage() {
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "<title> Login </title>"
    "</head>"
    "<body>"
    "<body text = #505452 bgcolor = '#f2f2f2'>"
    "<h2 align = 'center'> Access to setup </h2>"
    "<hr>"
    "<center>"
    "<p> <em> Login </em>"
    "<form method = 'post'>"
    "<input type = 'text' name = 'log' size = 20>"
    "<p> <em> Password </em> </p>"
    "<input type = 'text' name = 'pass' size = 20>"
    "<p> <input type = 'submit' value = 'Submit'> </p>"
    "</form>"
    "</center>"
  ));
  return bfill.position();
}

static word httpNotFound() {
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 404 Not Found"
  ));
  return bfill.position();
}

static word resetPage() {
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "<title> Reset Page </title>"
    "<body text = '#505452' bgcolor = '#f2f2f2'>"
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
    "</form>"
    "</center>"

));
  return bfill.position();
}

static word httpUnauthorized() {
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 401 Unauthorized\r\n"
    "WWW-Authenticate: Basic realm=\"Access\""
    "Content-Type: text/html\r\n\r\n"));
  return bfill.position();
}

static word controlPage() {
bfill = ether.tcpOffset();
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
    "<a href=\"?EXIT\">EXIT</a><br />"
    "</center>"

  ));
  return bfill.position();
}
// Возращает true, если хотя бы один из датчиков сработал.
boolean checkSensor() {
  if (digitalRead(SENSOR_1_PIN) || digitalRead(SENSOR_2_PIN)) {
    return true;
  } else return false;
}
