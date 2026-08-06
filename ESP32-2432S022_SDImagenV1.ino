// ============================================================
//  ESP32-2432S022 - Test lector microSD + imagen JPG
//  - Monta la tarjeta microSD
//  - Muestra capacidad y tipo en pantalla
//  - Lista los archivos de la raiz
//  - Si encuentra un .jpg, lo muestra pulsando "VER IMAGEN"
//  - Toca la imagen para volver al diagnostico
// ============================================================

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>

// ── Pines pantalla / táctil (ya validados) ───────────────────
#define BL_PIN      0
#define TOUCH_SDA  21
#define TOUCH_SCL  22
#define TOUCH_ADDR 0x15

// ── Pines microSD (VSPI, tipicos en ESP32-2432S022) ──────────
#define SD_CS       5
#define SD_MOSI    23
#define SD_MISO    19
#define SD_SCK     18

// ── Paleta (RGB888) ──────────────────────────────────────────
const uint32_t COLOR_FONDO  = 0x000000;
const uint32_t COLOR_TEXTO  = 0xFFFFFF;
const uint32_t COLOR_ACENTO = 0x00D0FF;
const uint32_t COLOR_OK     = 0x00FF00;
const uint32_t COLOR_AVISO  = 0xFFA500;
const uint32_t COLOR_ERROR  = 0xFF3030;
const uint32_t COLOR_GRIS   = 0xB0B0B0;
const uint32_t COLOR_BOTON  = 0x2060C0;
const uint32_t COLOR_BOTON2 = 0x208040;

// Botón inferior
const int BOTON_X = 20;
const int BOTON_Y = 275;
const int BOTON_W = 200;
const int BOTON_H = 35;

// ── Configuración de pantalla (la ya validada) ───────────────
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789   _panel_instance;
  lgfx::Bus_Parallel8  _bus_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.freq_write = 25000000;
      cfg.pin_wr = 4;
      cfg.pin_rd = 2;
      cfg.pin_rs = 16;
      cfg.pin_d0 = 15;
      cfg.pin_d1 = 13;
      cfg.pin_d2 = 12;
      cfg.pin_d3 = 14;
      cfg.pin_d4 = 27;
      cfg.pin_d5 = 25;
      cfg.pin_d6 = 33;
      cfg.pin_d7 = 32;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs    = 17;
      cfg.pin_rst   = -1;
      cfg.pin_busy  = -1;
      cfg.panel_width  = 240;
      cfg.panel_height = 320;
      cfg.readable     = false;
      cfg.invert       = false;
      cfg.rgb_order    = false;
      cfg.dlen_16bit   = false;
      cfg.bus_shared   = true;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

static LGFX tft;
static SPIClass sdSPI(VSPI);

bool   sdMontada       = false;
String imagenEncontrada = "";
bool   mostrandoImagen  = false;

// ── Táctil CST816S ───────────────────────────────────────────
bool leerTouch(int &x, int &y) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((uint8_t)TOUCH_ADDR, (uint8_t)6);
  if (Wire.available() < 6) return false;
  uint8_t data[6];
  for (int i = 0; i < 6; i++) data[i] = Wire.read();
  if ((data[1] & 0x0F) == 0) return false;
  x = ((data[2] & 0x0F) << 8) | data[3];
  y = ((data[4] & 0x0F) << 8) | data[5];
  return true;
}

// ── Helpers ──────────────────────────────────────────────────
void dibujarCabecera(const char* titulo) {
  tft.fillRect(0, 0, 240, 30, COLOR_ACENTO);
  tft.setTextColor(0x000000);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString(titulo, 120, 15);
}

void dibujarBoton(const char* texto, uint32_t color) {
  tft.fillRoundRect(BOTON_X, BOTON_Y, BOTON_W, BOTON_H, 8, color);
  tft.setTextColor(COLOR_TEXTO);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString(texto, BOTON_X + BOTON_W / 2, BOTON_Y + BOTON_H / 2);
}

bool dentroDelBoton(int x, int y) {
  return (x >= BOTON_X && x <= BOTON_X + BOTON_W &&
          y >= BOTON_Y && y <= BOTON_Y + BOTON_H);
}

const char* nombreTipoSD(uint8_t t) {
  switch (t) {
    case CARD_NONE:  return "sin tarjeta";
    case CARD_MMC:   return "MMC";
    case CARD_SD:    return "SDSC";
    case CARD_SDHC:  return "SDHC";
    default:         return "desconocido";
  }
}

// ── Montar SD ────────────────────────────────────────────────
bool montarSD() {
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  // 20 MHz suele ser seguro; bajar a 4 MHz si hay problemas
  if (!SD.begin(SD_CS, sdSPI, 20000000)) {
    return false;
  }
  if (SD.cardType() == CARD_NONE) {
    return false;
  }
  return true;
}

// ── Listar + buscar primer .jpg ──────────────────────────────
void diagnosticoSD() {
  mostrandoImagen = false;
  imagenEncontrada = "";
  tft.fillScreen(COLOR_FONDO);
  dibujarCabecera("SD Card Test");

  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);

  int y = 45;
  if (!sdMontada) {
    tft.setTextColor(COLOR_ERROR);
    tft.drawString("[ERROR] No se pudo montar la SD", 10, y);
    y += 15;
    tft.setTextColor(COLOR_GRIS);
    tft.drawString("Revisa:", 10, y); y += 15;
    tft.drawString(" - Tarjeta insertada bien", 10, y); y += 15;
    tft.drawString(" - Formato FAT32", 10, y); y += 15;
    tft.drawString(" - Capacidad <= 32 GB", 10, y); y += 15;
    dibujarBoton("REINTENTAR", COLOR_BOTON);
    return;
  }

  // Info SD
  uint8_t  tipo    = SD.cardType();
  uint64_t totalMB = SD.cardSize() / (1024ULL * 1024ULL);
  uint64_t usadaMB = SD.usedBytes() / (1024ULL * 1024ULL);

  tft.setTextColor(COLOR_OK);
  tft.drawString("[OK] Tarjeta montada", 10, y); y += 18;

  tft.setTextColor(COLOR_TEXTO);
  char buf[48];
  snprintf(buf, sizeof(buf), "Tipo:    %s", nombreTipoSD(tipo));
  tft.drawString(buf, 10, y); y += 14;
  snprintf(buf, sizeof(buf), "Tamano:  %llu MB", totalMB);
  tft.drawString(buf, 10, y); y += 14;
  snprintf(buf, sizeof(buf), "Usada:   %llu MB", usadaMB);
  tft.drawString(buf, 10, y); y += 20;

  // Listar raíz
  tft.setTextColor(COLOR_ACENTO);
  tft.drawString("Archivos en la raiz:", 10, y); y += 15;

  File dir = SD.open("/");
  int mostrados = 0;
  while (true) {
    File f = dir.openNextFile();
    if (!f) break;

    String nombre = String(f.name());
    // Quitar prefijo "/" si llega con él
    if (nombre.startsWith("/")) nombre = nombre.substring(1);

    if (mostrados < 10) {
      if (f.isDirectory()) {
        tft.setTextColor(COLOR_AVISO);
        tft.drawString("[dir] " + nombre, 10, y);
      } else {
        tft.setTextColor(COLOR_TEXTO);
        size_t sz = f.size();
        char linea[48];
        snprintf(linea, sizeof(linea), "%-20s %6u B",
                 nombre.substring(0, 20).c_str(),
                 (unsigned)sz);
        tft.drawString(linea, 10, y);
      }
      y += 13;
      mostrados++;
    }

    // Buscar primer .jpg / .jpeg (si aún no hay)
    if (imagenEncontrada.length() == 0 && !f.isDirectory()) {
      String lower = nombre;
      lower.toLowerCase();
      if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) {
        imagenEncontrada = "/" + nombre;
      }
    }
    f.close();
  }
  dir.close();

  if (mostrados == 0) {
    tft.setTextColor(COLOR_AVISO);
    tft.drawString("(vacia)", 10, y); y += 13;
  }

  y += 8;
  if (imagenEncontrada.length() > 0) {
    tft.setTextColor(COLOR_OK);
    tft.drawString("JPG encontrado: " + imagenEncontrada, 10, y);
    dibujarBoton("VER IMAGEN", COLOR_BOTON2);
  } else {
    tft.setTextColor(COLOR_AVISO);
    tft.drawString("No hay .jpg en la raiz.", 10, y);
    dibujarBoton("REINTENTAR", COLOR_BOTON);
  }
}

// ── Mostrar error de imagen centrado ─────────────────────────
void errorImagen(const char* msg) {
  tft.fillScreen(COLOR_FONDO);
  tft.setTextColor(COLOR_ERROR);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString(msg, 120, 140);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GRIS);
  tft.drawString(imagenEncontrada.c_str(), 120, 170);
  tft.drawString("Toca para volver", 120, 200);
}

// ── Mostrar imagen a pantalla completa ───────────────────────
//  Carga el JPG a RAM y lo pinta con drawJpg(buffer, tamano).
//  Evitamos drawJpgFile(SD, ...) porque la plantilla da problemas
//  con ciertas combinaciones de LovyanGFX + core ESP32 3.x.
void mostrarImagenJpg() {
  if (imagenEncontrada.length() == 0) return;
  mostrandoImagen = true;
  tft.fillScreen(COLOR_FONDO);

  File f = SD.open(imagenEncontrada.c_str(), FILE_READ);
  if (!f) {
    errorImagen("No abre");
    return;
  }

  size_t tam = f.size();
  if (tam == 0) {
    f.close();
    errorImagen("Archivo 0B");
    return;
  }

  uint8_t* buf = (uint8_t*) malloc(tam);
  if (!buf) {
    f.close();
    char aviso[32];
    snprintf(aviso, sizeof(aviso), "Sin RAM (%u B)", (unsigned)tam);
    errorImagen(aviso);
    return;
  }

  size_t leidos = f.read(buf, tam);
  f.close();

  bool ok = false;
  if (leidos == tam) {
    ok = tft.drawJpg(buf, tam, 0, 0);
  }
  free(buf);

  if (!ok) {
    errorImagen("Error JPG");
  }
}

// ── Setup ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== SD Card + JPG Test ===");

  pinMode(BL_PIN, OUTPUT);
  digitalWrite(BL_PIN, HIGH);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(COLOR_FONDO);

  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(400000);

  dibujarCabecera("SD Card Test");
  tft.setTextColor(COLOR_GRIS);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.drawString("Montando tarjeta microSD...", 120, 60);

  sdMontada = montarSD();
  delay(200);
  diagnosticoSD();
}

// ── Loop ─────────────────────────────────────────────────────
void loop() {
  int tx, ty;
  if (leerTouch(tx, ty)) {
    if (mostrandoImagen) {
      // Un toque en cualquier parte -> volver al diagnostico
      diagnosticoSD();
    } else {
      if (dentroDelBoton(tx, ty)) {
        if (imagenEncontrada.length() > 0) {
          mostrarImagenJpg();
        } else {
          // Reintentar montaje
          sdMontada = montarSD();
          diagnosticoSD();
        }
      }
    }
    delay(250);  // antirebote
  }
  delay(30);
}
