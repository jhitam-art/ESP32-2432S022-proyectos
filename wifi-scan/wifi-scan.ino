// ============================================================
//  ESP32-2432S022 - Test WiFi Scanner
//  Escanea las redes WiFi cercanas y las muestra en pantalla.
//  Boton tactil "RESCAN" para repetir el escaneo.
// ============================================================

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <Wire.h>

// ── Pines ────────────────────────────────────────────────────
#define BL_PIN      0
#define TOUCH_SDA  21
#define TOUCH_SCL  22
#define TOUCH_ADDR 0x15

// ── Paleta (RGB888, como exige LovyanGFX en uint32_t) ────────
const uint32_t COLOR_FONDO    = 0x000000;
const uint32_t COLOR_TEXTO    = 0xFFFFFF;
const uint32_t COLOR_ACENTO   = 0x00D0FF;
const uint32_t COLOR_OK       = 0x00FF00;
const uint32_t COLOR_AVISO    = 0xFFA500;
const uint32_t COLOR_ERROR    = 0xFF3030;
const uint32_t COLOR_GRIS     = 0xB0B0B0;
const uint32_t COLOR_GRIS_OSC = 0x404040;
const uint32_t COLOR_BOTON    = 0x2060C0;

// Geometría del botón RESCAN
const int BOTON_X = 55;
const int BOTON_Y = 265;
const int BOTON_W = 130;
const int BOTON_H = 40;

// ── Configuración de pantalla (la ya validada para tu placa) ─
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
      cfg.invert       = false;   // ← validado para tu placa
      cfg.rgb_order    = false;
      cfg.dlen_16bit   = false;
      cfg.bus_shared   = true;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

static LGFX tft;

// ── Leer táctil CST816S ──────────────────────────────────────
bool leerTouch(int &x, int &y) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission(false) != 0) return false;

  Wire.requestFrom((uint8_t)TOUCH_ADDR, (uint8_t)6);
  if (Wire.available() < 6) return false;

  uint8_t data[6];
  for (int i = 0; i < 6; i++) data[i] = Wire.read();

  uint8_t puntos = data[1] & 0x0F;
  if (puntos == 0) return false;

  x = ((data[2] & 0x0F) << 8) | data[3];
  y = ((data[4] & 0x0F) << 8) | data[5];
  return true;
}

// ── Helpers de dibujo ────────────────────────────────────────
void dibujarCabecera() {
  tft.fillRect(0, 0, 240, 30, COLOR_ACENTO);
  tft.setTextColor(0x000000);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString("WiFi Scanner", 120, 15);
}

void dibujarEstado(const char* msg, uint32_t color) {
  tft.fillRect(0, 30, 240, 25, COLOR_FONDO);
  tft.setTextColor(color);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.drawString(msg, 120, 42);
}

void dibujarBoton() {
  tft.fillRoundRect(BOTON_X, BOTON_Y, BOTON_W, BOTON_H, 10, COLOR_BOTON);
  tft.setTextColor(COLOR_TEXTO);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString("RESCAN", 120, BOTON_Y + BOTON_H / 2);
}

bool dentroDelBoton(int x, int y) {
  return (x >= BOTON_X && x <= BOTON_X + BOTON_W &&
          y >= BOTON_Y && y <= BOTON_Y + BOTON_H);
}

const char* tipoCifrado(wifi_auth_mode_t a) {
  switch (a) {
    case WIFI_AUTH_OPEN:            return "open";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2E";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA23";
    default:                        return "?";
  }
}

uint32_t colorSenal(int rssi) {
  if (rssi >= -60) return COLOR_OK;
  if (rssi >= -75) return COLOR_AVISO;
  return COLOR_ERROR;
}

// ── Ejecutar escaneo y pintar resultados ─────────────────────
void ejecutarScan() {
  // Limpiar zona de lista
  tft.fillRect(0, 55, 240, BOTON_Y - 55, COLOR_FONDO);
  dibujarEstado("Escaneando redes...", COLOR_AVISO);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();

  if (n <= 0) {
    dibujarEstado("No se encontraron redes", COLOR_ERROR);
    return;
  }

  char buf[32];
  snprintf(buf, sizeof(buf), "%d redes encontradas", n);
  dibujarEstado(buf, COLOR_OK);

  int limite = n < 10 ? n : 10;
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);

  for (int i = 0; i < limite; i++) {
    int y = 65 + i * 20;

    // Línea separadora discreta
    tft.drawFastHLine(8, y + 17, 224, COLOR_GRIS_OSC);

    // Número
    tft.setTextColor(COLOR_GRIS);
    char num[6];
    snprintf(num, sizeof(num), "%2d.", i + 1);
    tft.drawString(num, 8, y + 4);

    // SSID (truncado si es largo)
    tft.setTextColor(COLOR_TEXTO);
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) ssid = "(oculta)";
    if (ssid.length() > 16) ssid = ssid.substring(0, 16);
    tft.drawString(ssid, 32, y + 4);

    // RSSI
    tft.setTextColor(colorSenal(WiFi.RSSI(i)));
    char rssiBuf[8];
    snprintf(rssiBuf, sizeof(rssiBuf), "%d", WiFi.RSSI(i));
    tft.drawString(rssiBuf, 165, y + 4);

    // Cifrado
    tft.setTextColor(COLOR_GRIS);
    tft.drawString(tipoCifrado(WiFi.encryptionType(i)), 200, y + 4);

    // Log por Serial
    Serial.printf("%2d  %-20s  %4d dBm  %s\n",
                  i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i),
                  tipoCifrado(WiFi.encryptionType(i)));
  }

  WiFi.scanDelete();
}

// ── Setup ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== WiFi Scanner ESP32-2432S022 ===");

  pinMode(BL_PIN, OUTPUT);
  digitalWrite(BL_PIN, HIGH);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(COLOR_FONDO);

  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(400000);
  delay(100);

  dibujarCabecera();
  dibujarBoton();

  ejecutarScan();
}

// ── Loop ─────────────────────────────────────────────────────
void loop() {
  int tx, ty;
  if (leerTouch(tx, ty)) {
    if (dentroDelBoton(tx, ty)) {
      // Feedback visual al pulsar
      tft.fillRoundRect(BOTON_X, BOTON_Y, BOTON_W, BOTON_H, 10, COLOR_ACENTO);
      tft.setTextColor(0x000000);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(2);
      tft.drawString("RESCAN", 120, BOTON_Y + BOTON_H / 2);
      delay(120);

      ejecutarScan();
      dibujarBoton();
    }
  }
  delay(30);
}
