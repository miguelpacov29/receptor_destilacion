//-----------------------------------------------------------
// RECEPTOR UNO + PANTALLA ILI9341 2.4" (SHIELD) - DESTILACION
//-----------------------------------------------------------
// Recibe del Mega (receptor_destilacion.ino) una linea por ciclo
// con 7 campos separados por coma:
//   AAAA/MM/DD HH:MM:SS,DS18B20_1,DS18B20_2,PT100_1,PT100_2,estado,filas
// Ejemplo:
//   2026/10/12 12:30:05,78.25,78.00,92.10,91.80,15,120
// y muestra los 4 valores en una columna y una franja de estado
// al pie: SD, RTC y sensores con error. -999 = lectura invalida
// en el Mega: se muestra "ERROR".
//
// "estado" = suma de banderas enviada por el Mega:
//   1 = SD disponible            2 = ultima escritura verificada OK
//   4 = RTC con hora valida      8 = ya hubo al menos un intento de guardado
// "filas" = filas guardadas y verificadas desde el arranque del Mega.
// El estado es el del ciclo ANTERIOR (el Mega guarda despues de enviar).
//
// Conexion: TX1 (pin 18) del Mega -> A5 del UNO, GND comun.
// El shield ocupa D2-D9, A0-A4 y D10-D13, por eso se usa
// SoftwareSerial en A5 (unico pin libre fuera de D0/D1).
//
// Para probar sin el Mega: pega la linea de ejemplo en el
// Monitor Serie (9600, "Nueva linea").
//
// Basado en receptor_uno_pantalla v8.1 del proyecto datalogger mezcal.
//-----------------------------------------------------------
#include <SoftwareSerial.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

#define VERSION_FIRMWARE "1.0"

// -------------------------------------------------------------
// PUERTO SERIE HACIA EL MEGA
// -------------------------------------------------------------
// RX = A5 (recibe del Mega). TX = D10, no se conecta: solo se
// reciben datos (D10 es el CS de la SD del shield, que no se usa).
const int datosRX = A5;
const int datosTX = 10;

SoftwareSerial datos(datosRX, datosTX);

// -------------------------------------------------------------
// PANTALLA
// -------------------------------------------------------------
MCUFRIEND_kbv tft;

#define NEGRO    0x0000
#define BLANCO   0xFFFF
#define ROJO     0xF800
#define VERDE    0x07E0
#define AMARILLO 0xFFE0
#define CIAN     0x07FF
#define GRIS     0x8410

// Posiciones (pantalla horizontal 320x240, texto tamano 2 = 12x16 px)
const int xEtiqueta    = 8;
const int xValor       = 150;
const int anchoValor   = 170;
const int yFecha       = 32;
const int yPrimeraFila = 70;
const int pasoFila     = 30;
const byte numFilas    = 4;
const int ySeparador   = 210;  // linea sobre la franja de estado
const int yFranja      = 216;  // franja de estado (SD, RTC, ERR)

// Campos de la linea: 0 = fecha, 1..4 = valores (una fila cada uno),
// 5 = estado, 6 = filas
const byte camposEsperados = 7;

// Banderas del campo "estado"
#define EST_SD_DISPONIBLE  1
#define EST_GUARDADO_OK    2
#define EST_RTC_VALIDO     4
#define EST_HUBO_INTENTO   8

// -------------------------------------------------------------
// RECEPCION
// -------------------------------------------------------------
const unsigned long tiempoSinDatos = 30000;  // 30 segundos

// Una linea completa mide ~60 caracteres: buffers de 110.
// Un buffer por cada entrada: Mega y USB (para pruebas)
#define LINEA_MAX 110
char lineaDatos[LINEA_MAX];
byte idxDatos = 0;
char lineaUSB[LINEA_MAX];
byte idxUSB = 0;

unsigned long ultimoDato = 0;
bool mostrandoSinDatos = false;

// -------------------------------------------------------------
// POSICION VERTICAL DE UNA FILA
// -------------------------------------------------------------
int yFila(byte fila) {
  return yPrimeraFila + fila * pasoFila;
}

// -------------------------------------------------------------
// DIBUJA EL MARCO FIJO (SOLO UNA VEZ)
// -------------------------------------------------------------
void dibujarMarco() {
  tft.fillScreen(NEGRO);

  tft.setTextColor(CIAN);
  tft.setTextSize(2);
  tft.setCursor(xEtiqueta, 5);
  tft.print(F("DESTILACION"));
  tft.drawFastHLine(0, 25, 320, CIAN);

  tft.setTextColor(BLANCO);
  tft.setTextSize(2);
  tft.setCursor(xEtiqueta, yFila(0)); tft.print(F("DS18B20 1"));
  tft.setCursor(xEtiqueta, yFila(1)); tft.print(F("DS18B20 2"));
  tft.setCursor(xEtiqueta, yFila(2)); tft.print(F("PT100 1"));
  tft.setCursor(xEtiqueta, yFila(3)); tft.print(F("PT100 2"));

  tft.drawFastHLine(0, ySeparador, 320, GRIS);
}

// -------------------------------------------------------------
// MUESTRA LA FECHA/HORA O UN AVISO EN LA LINEA SUPERIOR
// -------------------------------------------------------------
void mostrarFecha(const char *texto, uint16_t color) {
  tft.fillRect(0, yFecha, 320, 18, NEGRO);
  tft.setTextColor(color);
  tft.setTextSize(2);
  tft.setCursor(xEtiqueta, yFecha);
  tft.print(texto);
}

// -------------------------------------------------------------
// MUESTRA UN VALOR EN SU FILA (GRADOS CELSIUS)
// -------------------------------------------------------------
// -999 = lectura invalida en el Mega.
void mostrarValor(byte fila, float valor) {
  int y = yFila(fila);
  tft.fillRect(xValor, y, anchoValor, 18, NEGRO);
  tft.setTextSize(2);
  tft.setCursor(xValor, y);

  if (valor <= -998.0) {
    tft.setTextColor(ROJO);
    tft.print(F("ERROR"));
    return;
  }

  char buf[10];
  dtostrf(valor, 7, 2, buf);
  tft.setTextColor(AMARILLO);
  tft.print(buf);
  tft.write(247);  // simbolo de grado (con cp437)
  tft.print('C');
}

// -------------------------------------------------------------
// CONTADOR DE FILAS GUARDADAS (A LA DERECHA DEL TITULO)
// -------------------------------------------------------------
void mostrarFilas(bool conDato, unsigned long filas) {
  tft.fillRect(222, 5, 98, 16, NEGRO);
  tft.setTextSize(1);
  tft.setCursor(224, 9);
  if (conDato) {
    tft.setTextColor(VERDE);
    tft.print(F("FILAS "));
    tft.print(filas);
  } else {
    tft.setTextColor(GRIS);
    tft.print(F("FILAS --"));
  }
}

// -------------------------------------------------------------
// FRANJA DE ESTADO AL PIE: SD, RTC y SENSORES CON ERROR
// -------------------------------------------------------------
// conEstado = false -> sin estado del Mega (enlace caido):
// SD y RTC en gris "--". errores = -1 -> desconocido ("ERR:--").
void mostrarFranja(bool conEstado, byte estado, int errores) {
  tft.fillRect(0, ySeparador + 1, 320, 240 - (ySeparador + 1), NEGRO);
  tft.setTextSize(2);
  tft.setCursor(4, yFranja);

  // SD
  if (!conEstado || !(estado & EST_HUBO_INTENTO)) {
    tft.setTextColor(GRIS);
    tft.print(F("SD:--"));
  } else if (!(estado & EST_SD_DISPONIBLE)) {
    tft.setTextColor(ROJO);
    tft.print(F("SD:NO DISP"));
  } else if (estado & EST_GUARDADO_OK) {
    tft.setTextColor(VERDE);
    tft.print(F("SD:OK"));
  } else {
    tft.setTextColor(ROJO);
    tft.print(F("SD:FALLO"));
  }
  tft.print(' ');

  // RTC
  if (!conEstado) {
    tft.setTextColor(GRIS);
    tft.print(F("RTC:--"));
  } else if (estado & EST_RTC_VALIDO) {
    tft.setTextColor(VERDE);
    tft.print(F("RTC:OK"));
  } else {
    tft.setTextColor(ROJO);
    tft.print(F("RTC:MAL"));
  }
  tft.print(' ');

  // Sensores con error (valores en -999)
  if (errores < 0) {
    tft.setTextColor(GRIS);
    tft.print(F("ERR:--"));
  } else {
    tft.setTextColor(errores == 0 ? VERDE : (errores <= 2 ? AMARILLO : ROJO));
    tft.print(F("ERR:"));
    tft.print(errores);
  }
}

// -------------------------------------------------------------
// SEPARA UNA LINEA RECIBIDA Y ACTUALIZA LA PANTALLA
// -------------------------------------------------------------
void procesarLinea(char *linea) {
  // Un campo de mas permite detectar lineas con demasiados campos
  char *campos[camposEsperados + 1];
  byte n = 0;

  char *token = strtok(linea, ",");
  while (token != NULL && n < camposEsperados + 1) {
    campos[n++] = token;
    token = strtok(NULL, ",");
  }

  if (n != camposEsperados) {
    Serial.print(F("Linea descartada: "));
    Serial.print(n);
    Serial.print(F(" campos (se esperan "));
    Serial.print(camposEsperados);
    Serial.println(F(")"));
    return;
  }

  mostrarFecha(campos[0], VERDE);

  // Valores y conteo de sensores con error
  int errores = 0;
  for (byte i = 0; i < numFilas; i++) {
    float v = atof(campos[i + 1]);
    mostrarValor(i, v);
    if (v <= -998.0) errores++;
  }

  byte estado = (byte)atoi(campos[5]);
  unsigned long filas = strtoul(campos[6], NULL, 10);
  mostrarFranja(true, estado, errores);
  mostrarFilas(true, filas);

  ultimoDato = millis();
  mostrandoSinDatos = false;

  Serial.print(F("Recibido: "));
  Serial.print(campos[0]);
  Serial.print(F("  estado="));
  Serial.print(estado);
  Serial.print(F(" filas="));
  Serial.print(filas);
  Serial.print(F(" errores="));
  Serial.println(errores);
}

// -------------------------------------------------------------
// ACUMULA CARACTERES HASTA COMPLETAR UNA LINEA
// -------------------------------------------------------------
void leerEntrada(Stream &entrada, char *linea, byte &idx) {
  while (entrada.available()) {
    char c = entrada.read();

    if (c == '\r') continue;

    if (c == '\n') {
      linea[idx] = '\0';
      if (idx > 0) procesarLinea(linea);
      idx = 0;
    } else if (idx < LINEA_MAX - 1) {
      linea[idx++] = c;
    } else {
      // Linea demasiado larga: se descarta
      idx = 0;
    }
  }
}

// -------------------------------------------------------------
// SETUP
// -------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  datos.begin(9600);

  Serial.print(F("Receptor UNO + pantalla v"));
  Serial.println(F(VERSION_FIRMWARE));

  uint16_t ID = tft.readID();
  Serial.print(F("ID de pantalla: 0x"));
  Serial.println(ID, HEX);
  if (ID == 0xD3D3) ID = 0x9341;  // algunos shields no responden al leer el ID

  tft.begin(ID);
  tft.setRotation(1);  // horizontal
  tft.cp437(true);

  dibujarMarco();
  mostrarFecha("Esperando datos...", BLANCO);
  mostrarFilas(false, 0);
  mostrarFranja(false, 0, -1);

  ultimoDato = millis();
  Serial.println(F("Receptor listo"));
}

// -------------------------------------------------------------
// LOOP
// -------------------------------------------------------------
void loop() {
  leerEntrada(datos, lineaDatos, idxDatos);

  // Tambien se aceptan lineas por el Monitor Serie (para pruebas)
  leerEntrada(Serial, lineaUSB, idxUSB);

  if (!mostrandoSinDatos && millis() - ultimoDato > tiempoSinDatos) {
    mostrarFecha("SIN DATOS", ROJO);
    // El estado de la SD ya no es confiable: se pone en gris
    mostrarFranja(false, 0, -1);
    mostrarFilas(false, 0);
    mostrandoSinDatos = true;
  }
}
