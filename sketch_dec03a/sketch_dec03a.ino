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

static byte mymac[] = {0x74,0x69,0x69,0x2D,0x30,0x32};
static byte myip[] = {EEPROM.read(1),EEPROM.read(2),EEPROM.read(3),EEPROM.read(4)};
static byte defip[] = {192,168,0,13};
byte Ethernet::buffer[700];

byte D1 = 10; // Номер пина для 1-го датчика движения.
byte D2 = 11; // Номер пина для 2-го датчика движения.
byte S3 = 9; // Номер пина для 3-го светодиода -> в дальнейшем для реле.
byte R1; // Номер пина кнопки сбрасывания
long previousMillis; // Последняя отправка.
int interval = 10000; // Сколько будет светиться светодиод (S3).

BufferFiller bfill;
// Функция программной перезагрузки ардуино

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
    "<p> <input type = 'submit' value = 'Submit'> </p>"
    "</form>"
    "</center>"
 
));
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
    
    "</center>"

  ));
  return bfill.position();
}



void setup() {
  EEPROM.write(0,1);
  pinMode(D1, INPUT); // Подключение датчка D1 на вход.
  pinMode(D2, INPUT); // Подключение датчка D1 на вход.
  pinMode(S3, OUTPUT); // Подключение светодиода S3 на выход.
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
    digitalWrite(S3,1);
  } else { digitalWrite(S3,0); }
  
  // Если подключение к сети отсутсвует, датчик все равно запуститься 
  if(pos) {
    if (EEPROM.read(0) == 1) {
      char *post_pos = strstr((char *) Ethernet::buffer + pos,"ip=");
      char *ip ,*gtw, *dns, *last, *subm;
      char *token = strtok_r(post_pos, "&", &last);// делим на токены post_pos, разделитель &
      byte i = 1; // нумерация токенов
      while (token != NULL) {
        if (i == 1) {
          ip = token;
          if (ip[3] != NULL) {
            ip = strtok(ip, "ip=");
            char* ip_token = strtok(ip, ".");
            byte n = 1;
            while (ip_token != NULL) {
              //Функция atoi(char *) преобразует символьный массив и возращает число 
              if (n == 1) EEPROM.write(1,atoi(ip_token)); //Serial.println((byte) atoi(ip_token));
              if (n == 2) EEPROM.write(2,atoi(ip_token));
              if (n == 3) EEPROM.write(3,atoi(ip_token));
              if (n == 4) EEPROM.write(4,atoi(ip_token));
              ip_token = strtok(NULL,".");
              n++;
            }
          } else ip = 0;
        }
      
        if (i == 2) {
          gtw = token;
          if (gtw[4] != NULL) {
            gtw = strtok(gtw, "gtw=");
            char* gtw_token = strtok(gtw, ".");
            int n = 1;
            while (gtw_token != NULL) {
              if (n == 1) EEPROM.write(5,atoi(gtw_token));
              if (n == 2) EEPROM.write(6,atoi(gtw_token));
              if (n == 3) EEPROM.write(7,atoi(gtw_token));
              if (n == 4) EEPROM.write(8,atoi(gtw_token));
              gtw_token = strtok(NULL,".");
              n++;
            }
          } else gtw = 0;
        }
      
        if (i == 3) {
          dns = token;
          if (dns[4] != NULL) {
            dns = strtok(dns, "dns=");
            char* dns_token = strtok(dns,".");
            int n = 1;
            while (dns_token != NULL) {
              if (n == 1) EEPROM.write(9,atoi(dns_token));
              if (n == 2) EEPROM.write(10,atoi(dns_token));
              if (n == 3) EEPROM.write(11,atoi(dns_token));
              if (n == 4) EEPROM.write(12,atoi(dns_token));
              dns_token = strtok(NULL,".");
              n++;
            }
          } else dns = 0;
        }
        if (i == 4) {
          subm = token;
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
        token = strtok_r(NULL, "&" ,&last); // выделение следующей части строки (поиск нового токена и выделение его)
        i++; //нужен для определения какой на данный момент номер токена
      }
    
    //Serial.println(ip);
    //Serial.println(gtw);
    //Serial.println(dns);
    ether.httpServerReply(resetPage()); //отправить http response (отобразить страницу)
    } else {
      ether.httpServerReply(controlPage());
    }
  }
}

// Возращает true, если хотя бы один из датчиков сработал.
boolean checkSensor() {
  if (digitalRead(D1) || digitalRead(D2)) {
    return true;
  } else return false;
}


