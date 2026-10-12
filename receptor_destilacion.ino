
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_MAX31865.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

// ---------- DS18B20 ----------
const uint8_t PIN_DS1 = 2;
const uint8_t PIN_DS2 = 3;

OneWire ow1(PIN_DS1);
OneWire ow2(PIN_DS2);

DallasTemperature ds1(&ow1);
DallasTemperature ds2(&ow2);

// ---------- MAX31865 / PT100 ----------
const uint8_t CS_PT1 = 10;
const uint8_t CS_PT2 = 8;

Adafruit_MAX31865 pt1(CS_PT1);
Adafruit_MAX31865 pt2(CS_PT2);

#define RNOMINAL 100.0
#define RREF     430.0

// ---------- Micro SD ----------
const uint8_t SD_CS = 53;
bool sdOK = false;

// ---------- DS3231 ----------
RTC_DS3231 rtc;
bool rtcOK = false;

// ---------- Enlace al UNO con pantalla (Serial1: TX1 = pin 18 -> A5 del UNO) ----------
// Banderas del campo "estado" que se envia a la pantalla
#define EST_SD_DISPONIBLE  1
#define EST_GUARDADO_OK    2
#define EST_RTC_VALIDO     4
#define EST_HUBO_INTENTO   8

bool ultimoGuardadoOK = false;
bool huboIntento = false;
unsigned long filasGuardadas = 0;

void imprimirTemperatura(const char* nombre, float t) {
  Serial.print(nombre);
  Serial.print(": ");

  if (t == DEVICE_DISCONNECTED_C) {
    Serial.println("ERROR: sensor desconectado");
  } else {
    Serial.print(t, 2);
    Serial.println(" C");
  }
}

float leerPT100(Adafruit_MAX31865 &sensor,
                const char* nombre) {
  uint8_t fault = sensor.readFault();

  if (fault != 0) {
    Serial.print(nombre);
    Serial.print(" - Error MAX31865: 0x");
    Serial.println(fault, HEX);

    sensor.clearFault();
    return NAN;
  }

  float t = sensor.temperature(RNOMINAL, RREF);

  // Comprobar nuevamente el estado del convertidor
  fault = sensor.readFault();

  if (fault != 0) {
    Serial.print(nombre);
    Serial.print(" - Error durante lectura: 0x");
    Serial.println(fault, HEX);

    sensor.clearFault();
    return NAN;
  }

  Serial.print(nombre);
  Serial.print(": ");
  Serial.print(t, 2);
  Serial.println(" C");

  return t;
}

// Envia una linea al UNO con pantalla (-999 = lectura invalida):
// AAAA/MM/DD HH:MM:SS,DS18B20_1,DS18B20_2,PT100_1,PT100_2,estado,filas
// "estado" es el del ciclo anterior (la SD se guarda despues de enviar).
void enviarDatosPantalla(bool conHora, DateTime ahora,
                         float d1, float d2,
                         float p1, float p2) {
  char fecha[20];

  if (conHora) {
    snprintf(fecha, sizeof(fecha),
             "%04d/%02d/%02d %02d:%02d:%02d",
             ahora.year(), ahora.month(), ahora.day(),
             ahora.hour(), ahora.minute(), ahora.second());
  } else {
    strcpy(fecha, "0000/00/00 00:00:00");
  }

  uint8_t estado = 0;
  if (sdOK) estado |= EST_SD_DISPONIBLE;
  if (ultimoGuardadoOK) estado |= EST_GUARDADO_OK;
  if (conHora && !rtc.lostPower()) estado |= EST_RTC_VALIDO;
  if (huboIntento) estado |= EST_HUBO_INTENTO;

  float v[4] = {d1, d2, p1, p2};

  Serial1.print(fecha);
  for (uint8_t i = 0; i < 4; i++) {
    Serial1.print(',');
    if (isnan(v[i]) || v[i] == DEVICE_DISCONNECTED_C) Serial1.print(-999);
    else Serial1.print(v[i], 2);
  }
  Serial1.print(',');
  Serial1.print(estado);
  Serial1.print(',');
  Serial1.println(filasGuardadas);
}

// Devuelve true solo si el registro quedo guardado en la SD.
bool escribirCSV(DateTime ahora,
                 float d1, float d2,
                 float p1, float p2) {
  if (!sdOK) return false;

  File archivo = SD.open("mediciones.csv", FILE_WRITE);

  if (!archivo) {
    Serial.println("ERROR: no se pudo abrir mediciones.csv");
    sdOK = false;
    return false;
  }

  char fecha[20];
  snprintf(fecha, sizeof(fecha),
           "%04d-%02d-%02d %02d:%02d:%02d",
           ahora.year(), ahora.month(), ahora.day(),
           ahora.hour(), ahora.minute(), ahora.second());

  archivo.print(fecha);
  archivo.print(',');

  if (isnan(d1) || d1 == DEVICE_DISCONNECTED_C)
    archivo.print("ERROR");
  else archivo.print(d1, 2);
  archivo.print(',');

  if (isnan(d2) || d2 == DEVICE_DISCONNECTED_C)
    archivo.print("ERROR");
  else archivo.print(d2, 2);
  archivo.print(',');

  if (isnan(p1)) archivo.print("ERROR");
  else archivo.print(p1, 2);
  archivo.print(',');

  if (isnan(p2)) archivo.println("ERROR");
  else archivo.println(p2, 2);

  archivo.close();
  Serial.println("CSV: registro guardado");
  return true;
}

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);  // hacia el UNO con pantalla
  delay(1000);

  Serial.println(F("\n=== PRUEBA DE SENSORES ==="));

  // Importante: mantener como salidas los CS
  // inactivos antes de iniciar el bus SPI.
  pinMode(53, OUTPUT);
  digitalWrite(53, HIGH);

  pinMode(CS_PT1, OUTPUT);
  digitalWrite(CS_PT1, HIGH);

  pinMode(CS_PT2, OUTPUT);
  digitalWrite(CS_PT2, HIGH);

  // DS18B20
  ds1.begin();
  ds2.begin();

  Serial.print(F("DS18B20 #1 encontrados: "));
  Serial.println(ds1.getDeviceCount());

  Serial.print(F("DS18B20 #2 encontrados: "));
  Serial.println(ds2.getDeviceCount());

  // MAX31865: PT100 de tres hilos
  pt1.begin(MAX31865_3WIRE);
  pt2.begin(MAX31865_3WIRE);

  Serial.println(F("MAX31865 inicializados"));

  // Reloj DS3231
  Wire.begin();
  rtcOK = rtc.begin();

  if (rtcOK) {
    Serial.println(F("DS3231 detectado"));

    if (rtc.lostPower()) {
      Serial.println(
        F("AVISO: el reloj perdio la hora. "
          "Ajustala una vez con rtc.adjust().")
      );
    }
  } else {
    Serial.println(F("ERROR: DS3231 no detectado"));
  }

  // Tarjeta SD
  Serial.println(F("Iniciando tarjeta SD..."));

  sdOK = SD.begin(SD_CS);

  if (sdOK) {
    Serial.println(F("Tarjeta SD detectada"));

    if (!SD.exists("mediciones.csv")) {
      File archivo = SD.open("mediciones.csv", FILE_WRITE);

      if (archivo) {
        archivo.println(
          F("FechaHora,DS18B20_1_C,DS18B20_2_C,"
            "PT100_1_C,PT100_2_C")
        );
        archivo.close();
      } else {
        Serial.println(F("ERROR creando CSV"));
        sdOK = false;
      }
    }
  } else {
    Serial.println(F("ERROR: tarjeta SD no detectada"));
  }

  Serial.println(F("=== INICIO DE LECTURAS ==="));
}

void loop() {
  // Leer DS18B20
  ds1.requestTemperatures();
  ds2.requestTemperatures();

  float tDS1 = ds1.getTempCByIndex(0);
  float tDS2 = ds2.getTempCByIndex(0);

  imprimirTemperatura("DS18B20 #1", tDS1);
  imprimirTemperatura("DS18B20 #2", tDS2);

  // Leer PT100
  float tPT1 = leerPT100(pt1, "PT100 #1");
  float tPT2 = leerPT100(pt2, "PT100 #2");

  // Obtener fecha y hora
  DateTime ahora;
  bool horaDisponible = rtcOK;

  if (rtcOK) {
    ahora = rtc.now();

    Serial.print(F("Fecha y hora: "));
    Serial.print(ahora.timestamp(DateTime::TIMESTAMP_FULL));
    Serial.println();
  } else {
    Serial.println(F("Hora no disponible"));
  }

  // Enviar a la pantalla (UNO), haya o no hora disponible
  enviarDatosPantalla(horaDisponible, ahora, tDS1, tDS2, tPT1, tPT2);

  // Guardar solo si el reloj está disponible
  if (horaDisponible) {
    huboIntento = true;
    ultimoGuardadoOK = escribirCSV(ahora, tDS1, tDS2, tPT1, tPT2);
    if (ultimoGuardadoOK) filasGuardadas++;
  }

  Serial.println(F("----------------------------"));
  delay(2000);
}
