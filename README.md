# receptor_destilacion
Se guarda el codigo de arduino para el receptor de destilacion de Omar

## Contenido

| Carpeta / archivo | Placa | Funcion |
|---|---|---|
| `receptor_destilacion.ino` | Arduino **Mega 2560** | Lee 2x DS18B20, 2x PT100 (MAX31865) y el reloj DS3231; guarda `medicion.csv` en la SD y envia cada ciclo los datos a la pantalla. |
| `receptor_uno_pantalla/receptor_uno_pantalla.ino` | Arduino **UNO** + shield TFT ILI9341 2.4" | Recibe los datos del Mega y los muestra. |
| `PROPUESTAS_MEJORA.md` | - | Propuestas de mejora del codigo completo. |

## Conexion Mega -> UNO

| Mega | UNO |
|---|---|
| TX1 (pin 18) | A5 |
| GND | GND |

- Enlace serie de un solo sentido a 9600 baud, ambos a 5 V. El RX1 (pin 19) del Mega no se usa.
- El shield TFT ocupa D2-D9, A0-A4 y D10-D13; por eso el UNO recibe con `SoftwareSerial` en **A5**.

## Formato de la linea (una por ciclo)

```
AAAA/MM/DD HH:MM:SS,DS18B20_1,DS18B20_2,PT100_1,PT100_2,estado,filas
2026/10/12 12:30:05,78.25,78.00,92.10,91.80,15,120
```

- `-999` = lectura invalida (sensor desconectado o con falla); la pantalla muestra `ERROR`.
- `estado` = suma de banderas: `1` SD disponible, `2` ultima escritura verificada, `4` RTC con hora valida, `8` ya hubo un intento de guardado. Es el del ciclo anterior.
- `filas` = filas guardadas y verificadas en la SD desde el arranque del Mega.
- Si el RTC no esta disponible se envia la fecha `0000/00/00 00:00:00`.
- La pantalla muestra `SIN DATOS` si pasan 30 s sin recibir una linea.

## Librerias necesarias

- Mega: `OneWire`, `DallasTemperature`, `Adafruit MAX31865`, `RTClib`, `SD`.
- UNO: `Adafruit GFX`, `MCUFRIEND_kbv`.

## Probar la pantalla sin el Mega

Con el UNO conectado por USB, abrir el Monitor Serie (9600, "Nueva linea") y pegar:

```
2026/10/12 12:30:05,78.25,78.00,92.10,-999,15,120
```

Debe verse la fecha en verde, 3 temperaturas, `ERROR` en PT100 2 y la franja `SD:OK RTC:OK ERR:1`.

Basado en el receptor del proyecto datalogger mezcal (receptor UNO v8.1).
