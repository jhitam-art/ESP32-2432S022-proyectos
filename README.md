# ESP32-2432S022-proyectos

![Placa ESP32-2432S022 (vista trasera)](/img/ESP32back.jpg)

Colección de sketches de prueba y proyectos para la placa **ESP32-2432S022**,
un módulo "todo en uno" con ESP32, pantalla TFT ST7789 de 2.2" (240x320,
bus paralelo 8080), táctil capacitivo CST816S y ranura para microSD.

> ⚠️ No confundir con la ESP32-2432S028 ("Cheap Yellow Display"): son placas
> distintas, con pines y configuración de LovyanGFX diferentes.

Cada carpeta es un proyecto independiente y autocontenido, pensado tanto
como referencia de código funcional para esta placa concreta (pines,
configuración de LovyanGFX, librerías necesarias) como para ir aprendiendo
paso a paso: de un test simple de pantalla/táctil, a leer una SD, a
escanear redes WiFi, etc.

## Hardware común a todos los proyectos

- Placa ESP32-2432S022
- Pantalla ST7789, 2.2", 240x320 px, bus paralelo 8080
- Táctil capacitivo CST816S (I2C, dirección `0x15`)
- Arduino IDE + core ESP32 (Espressif Systems) **v3.1.3**
  (versiones más recientes como 4.0.0-alpha1 rompen la compilación
  con LovyanGFX por cambios internos en ESP-IDF)
- Librería [LovyanGFX](https://github.com/lovyan03/LovyanGFX)

## Proyectos incluidos

| Proyecto | Descripción |
|---|---|
| [`sd-card-test/`](sd-card-test/) | Monta la microSD, muestra tipo/capacidad, lista archivos y visualiza imágenes JPG a pantalla completa |
| [`wifi-scan/`](wifi-scan/) | Escanea redes WiFi cercanas y muestra SSID, señal (RSSI) y tipo de cifrado, con botón táctil para repetir el escaneo |

*(se irán añadiendo más a medida que avancen las pruebas)*

## Cómo usar estos sketches

1. Instala el core ESP32 v3.1.3 desde el Gestor de Placas de Arduino IDE
2. Instala la librería LovyanGFX desde el Gestor de Librerías
3. Abre el `.ino` del proyecto que quieras probar (cada uno está en su
   propia carpeta con ese mismo nombre, como exige Arduino IDE)
4. Selecciona placa **ESP32 Dev Module** y el puerto correspondiente
5. Compila y sube
