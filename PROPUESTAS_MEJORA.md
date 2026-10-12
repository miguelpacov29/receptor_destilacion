# Propuestas de mejora - receptor_destilacion

Revision de `receptor_destilacion.ino` (Mega). Son **propuestas**: ninguna se aplico al codigo salvo el envio a la pantalla. Ordenadas por importancia para no perder datos.

## Prioridad alta (proteger los datos)

### 1. Ajustar el reloj cuando pierde la hora
Hoy solo avisa y sigue guardando con fecha falsa (normalmente 2000-01-01).
```cpp
if (rtc.lostPower()) {
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // hora de compilacion
}
```
Es aproximado (hora de la ultima compilacion); la alternativa es no guardar hasta que alguien ajuste la hora, y la pantalla ya muestra `RTC:MAL`.

### 2. Reintentar la SD si falla
Un fallo de escritura pone `sdOK = false` y nunca se recupera: un mal contacto momentaneo pierde todos los datos hasta reiniciar.
```cpp
unsigned long ultimoReintentoSD = 0;
// en loop():
if (!sdOK && millis() - ultimoReintentoSD > 30000) {
  ultimoReintentoSD = millis();
  sdOK = SD.begin(SD_CS);
}
```
(Si el archivo no existe tras recuperar, volver a crear el encabezado.)

### 3. Guardar aunque falle el RTC
Con `rtcOK` falso no se guarda nada. Guardar igual, usando `millis()` como marca de tiempo, para no perder la corrida.

### 4. No usar `timestamp()` (String)
`ahora.timestamp(...)` crea un `String` en cada ciclo y fragmenta la RAM en funcionamiento largo. Reutilizar el `snprintf` con buffer que ya existe en `escribirCSV`.

### 5. Descartar 85.0 C del DS18B20
Un DS18B20 que acaba de encenderse o fallo devuelve 85.0 C y se toma como lectura valida.
```cpp
if (t == DEVICE_DISCONNECTED_C || t == 85.0) t = NAN;
```

## Prioridad media

### 6. Periodo exacto con `millis()`
Cada `requestTemperatures()` espera ~750 ms y luego hay `delay(2000)`: el ciclo real queda en ~3.7 s y varia.
```cpp
ds1.setWaitForConversion(false);
ds2.setWaitForConversion(false);
// pedir ambas, esperar 750 ms sin bloquear, leer, y repetir cada N segundos con millis()
```

### 7. PT100: quitar el primer `readFault()`
`temperature()` ya limpia el estado al iniciar, asi que el primer chequeo solo reporta fallas viejas un ciclo antes de medir. El chequeo util es el segundo.

### 8. Constantes
Usar `SD_CS` en lugar del `53` escrito a mano en `setup()`.

## A verificar en el hardware
- **Placa**: el pin 53 como CS de la SD y `Serial1` implican un **Mega**. En un UNO no funciona.
- **MAX31865**: `MAX31865_3WIRE` exige el jumper de la placa soldado en modo 3 hilos.
- **DS18B20**: cada bus necesita pull-up de 4.7 kOhm a 5 V.
- **SPI compartido** entre la SD y los dos MAX31865: si el PT100 da lecturas raras al escribir en la SD, revisar ese bus.

## Ya incluido en esta rama
- Envio de datos al UNO con pantalla (`enviarDatosPantalla`, `Serial1`), sin cambiar la logica de lectura ni de guardado. `escribirCSV` ahora devuelve `bool` para poder informar el estado de la SD.
- Sketch del receptor UNO + pantalla ILI9341 (`receptor_uno_pantalla/`).
