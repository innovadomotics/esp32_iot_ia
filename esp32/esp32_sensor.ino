/*
 * ESP32 - Envío de datos aleatorios + recepción de comandos (Netlify)
 *
 * Combina dos mecanismos sobre HTTP:
 *  1) PIGGYBACK: cada POST /data devuelve los comandos vigentes en la respuesta.
 *  2) POLLING:   un GET /commands periódico (intervalo configurable) por si la
 *                web cambia un comando entre dos POSTs.
 *
 * Librería externa requerida (Arduino IDE -> Library Manager):
 *   - "ArduinoJson" by Benoit Blanchon (v7+)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// ====== CONFIGURACIÓN ======
const char* WIFI_SSID = "RED-INNOVA";
const char* WIFI_PASS = "lm24lg07geek#$L";

// Dominio del sitio Netlify (sin https://)
const char* NETLIFY_HOST    = "esp32-iot.netlify.app";
const char* PATH_DATA       = "/.netlify/functions/data";
const char* PATH_COMMANDS   = "/.netlify/functions/commands";

// Intervalos (ms) - ambos mayores a 3 segundos
unsigned long INTERVALO_ENVIO_MS = 5000;    // POST de temperatura/humedad
unsigned long INTERVALO_POLL_MS  = 10000;   // GET de comandos (configurable)
// ===========================

unsigned long ultimoEnvio = 0;
unsigned long ultimoPoll  = 0;

// Estado actual de los comandos recibidos
bool   estadoSwitch  = false;
String estadoTexto   = "";

// ---------- WiFi ----------
void conectarWiFi() {
  Serial.print("Conectando a WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Conectado. IP: ");
  Serial.println(WiFi.localIP());
}

// ---------- Aplicar comandos ----------
void aplicarComandos(bool sw, const String& txt, const char* origen) {
  bool cambio = false;
  if (sw != estadoSwitch) {
    estadoSwitch = sw;
    Serial.printf("[%s] SWITCH -> %s\n", origen, sw ? "ON" : "OFF");
    digitalWrite(LED_BUILTIN, sw ? HIGH : LOW);
    cambio = true;
  }
  if (txt != estadoTexto) {
    estadoTexto = txt;
    Serial.printf("[%s] TEXTO  -> \"%s\"\n", origen, txt.c_str());
    cambio = true;
  }
  if (!cambio) {
    Serial.printf("[%s] sin cambios (switch=%s, texto=\"%s\")\n",
                  origen, sw ? "ON" : "OFF", txt.c_str());
  }
}

// ---------- POST de datos + piggyback ----------
bool enviarDatos(float temperatura, float humedad) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  String url = String("https://") + NETLIFY_HOST + PATH_DATA;
  if (!https.begin(client, url)) {
    Serial.println("Error iniciando HTTPS (POST /data)");
    return false;
  }

  https.addHeader("Content-Type", "application/json");
  String payload = "{\"temperatura\":" + String(temperatura, 2) +
                   ",\"humedad\":" + String(humedad, 2) + "}";

  Serial.print("POST -> ");
  Serial.println(payload);

  int code = https.POST(payload);
  String resp = https.getString();
  Serial.printf("POST /data HTTP %d\n", code);

  if (code == 200) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (!err && doc["comandos"].is<JsonObject>()) {
      bool sw    = doc["comandos"]["switch"] | false;
      String txt = String((const char*) (doc["comandos"]["texto"] | ""));
      aplicarComandos(sw, txt, "PIGGYBACK");
    }
  } else {
    Serial.printf("Respuesta: %s\n", resp.c_str());
  }

  https.end();
  return code >= 200 && code < 300;
}

// ---------- GET de comandos ----------
bool consultarComandos() {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  String url = String("https://") + NETLIFY_HOST + PATH_COMMANDS;
  if (!https.begin(client, url)) {
    Serial.println("Error iniciando HTTPS (GET /commands)");
    return false;
  }

  int code = https.GET();
  String resp = https.getString();
  Serial.printf("GET /commands HTTP %d\n", code);

  if (code == 200) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (!err) {
      bool sw    = doc["switch"] | false;
      String txt = String((const char*) (doc["texto"] | ""));
      aplicarComandos(sw, txt, "POLL");
    }
  }

  https.end();
  return code >= 200 && code < 300;
}

// ---------- Generador aleatorio ----------
float aleatorio(float min, float max) {
  return min + (float)random(0, 10000) / 10000.0 * (max - min);
}

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(LED_BUILTIN, OUTPUT);
  randomSeed(esp_random());
  conectarWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  // 1) Envío periódico de datos (con piggyback de comandos en la respuesta)
  if (millis() - ultimoEnvio >= INTERVALO_ENVIO_MS) {
    ultimoEnvio = millis();
    float t = aleatorio(18.0, 30.0);
    float h = aleatorio(40.0, 80.0);
    Serial.printf("Sensor T=%.2f°C  H=%.2f%%\n", t, h);
    enviarDatos(t, h);
  }

  // 2) Polling separado de comandos
  if (millis() - ultimoPoll >= INTERVALO_POLL_MS) {
    ultimoPoll = millis();
    consultarComandos();
  }
}
