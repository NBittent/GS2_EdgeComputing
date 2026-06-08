/*
 * ============================================================
 *  DRAGON CAPSULE TELEMETRY SYSTEM
 *  Global Solution 2026 — Edge Computing & Computer Systems
 *  SpaceX Dragon C214 | Protocolo: MQTT | Backend: FIWARE Orion
 * ============================================================
 *
 *  Parâmetros monitorados:
 *    1. Pressão interna da cabine (DHT22 simulado → sensor BMP280)
 *    2. Temperatura interna (DHT22)
 *    3. Radiação cósmica (LDR simulando sensor Geiger)
 *    4. Velocidade orbital (potenciômetro simulando GPS/IMU)
 *
 *  Atuadores:
 *    - LED verde  → telemetria nominal
 *    - LED amarelo → alerta (parâmetro fora da faixa)
 *    - LED vermelho → crítico (potencial falha de sistema)
 *    - Buzzer    → alerta sonoro em condição crítica
 *
 *  Fluxo:
 *    ESP32 → WiFi → MQTT Broker (HiveMQ/Mosquitto)
 *              → FIWARE IoT Agent → Orion Context Broker
 *              → Dashboard (Node-RED / Grafana)
 * ============================================================
 */
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ─── Configuração de Rede ───────────────────────────────────
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// ─── MQTT / FIWARE ─────────────────────────────────────────
const char* MQTT_BROKER   = "broker.hivemq.com";
const int   MQTT_PORT     = 1883;
const char* MQTT_CLIENT   = "dragon_c214_esp32";

// Tópicos MQTT (padrão FIWARE IoT Agent UL)
const char* TOPIC_TELEMETRY = "dragon/c214/telemetry";
const char* TOPIC_ALERTS    = "dragon/c214/alerts";
const char* TOPIC_STATUS    = "dragon/c214/status";

// ─── Pinos ─────────────────────────────────────────────────
#define DHT_PIN       4      // Sensor de temperatura/umidade
#define DHT_TYPE      DHT22
#define LDR_PIN       34     // Sensor de luminosidade (radiação simulada)
#define POT_PIN       35     // Potenciômetro (velocidade orbital simulada)
#define LED_GREEN     25     // Status OK
#define LED_YELLOW    26     // Alerta
#define LED_RED       27     // Crítico
#define BUZZER_PIN    32     // Alarme sonoro

// ─── Limites Operacionais da Dragon ────────────────────────
// Fonte: SpaceX Dragon Environmental Control & Life Support
const float PRES_NOM_MIN  = 99.0;   // kPa
const float PRES_NOM_MAX  = 103.0;  // kPa
const float PRES_WARN_MIN = 97.0;   // kPa
const float PRES_WARN_MAX = 105.0;  // kPa

const float TEMP_NOM_MIN  = 20.0;   // °C
const float TEMP_NOM_MAX  = 25.0;   // °C
const float TEMP_WARN_MIN = 18.0;   // °C
const float TEMP_WARN_MAX = 27.0;   // °C

const float RAD_WARN      = 0.8;    // mSv/h  (limite alerta)
const float RAD_CRIT      = 1.5;    // mSv/h  (limite crítico)

const float VEL_NOM_MIN   = 7.60;   // km/s (ISS orbit)
const float VEL_NOM_MAX   = 7.75;   // km/s
const float VEL_WARN_MIN  = 7.50;   // km/s
const float VEL_WARN_MAX  = 7.85;   // km/s

// ─── Instâncias ────────────────────────────────────────────
DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ─── Variáveis de Estado ───────────────────────────────────
unsigned long lastPublish  = 0;
unsigned long lastBlink    = 0;
const long    PUBLISH_INTERVAL = 1000;  // 1 segundos entre publicações
int alertLevel = 0; // 0=OK, 1=WARN, 2=CRIT
unsigned long missionElapsed = 0;
unsigned long missionStart   = 0;

// ─── Protótipos ────────────────────────────────────────────
void connectWiFi();
void connectMQTT();
float readPressure();
float readTemperature();
float readRadiation();
float readOrbitalVelocity();
int   evaluateStatus(float pres, float temp, float rad, float vel);
void  publishTelemetry(float pres, float temp, float rad, float vel, int status);
void  publishAlert(String sensor, float value, String level, String msg);
void  setIndicators(int status);
void  triggerBuzzer(int pulses);
String getStatusString(int s);
String formatMET(unsigned long ms);

// ═══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println(F("\n======================================"));
  Serial.println(F("  DRAGON C214 TELEMETRY SYSTEM v1.0"));
  Serial.println(F("  Global Solution 2026 - Edge Computing"));
  Serial.println(F("======================================\n"));

  // Pinos de saída
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Sequência de boot — testa todos os LEDs
  Serial.println(F("[BOOT] Testando indicadores..."));
  digitalWrite(LED_RED,    HIGH); delay(300);
  digitalWrite(LED_RED,    LOW);
  digitalWrite(LED_YELLOW, HIGH); delay(300);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_GREEN,  HIGH); delay(300);
  digitalWrite(LED_GREEN,  LOW);
  triggerBuzzer(2);
  Serial.println(F("[BOOT] Indicadores OK"));

  // Inicializa sensor DHT
  dht.begin();
  delay(2000); // DHT22 precisa de 2s após power-on
  Serial.println(F("[BOOT] Sensor DHT22 inicializado"));

  // Conecta WiFi e MQTT
  connectWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback([](char* topic, byte* payload, unsigned int len) {
    // Callback para comandos recebidos do ground control
    String msg = "";
    for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
    Serial.println("[CMD] Recebido: " + msg);
  });
  connectMQTT();

  missionStart = millis();

  // Publica status de inicialização
  mqttClient.publish(TOPIC_STATUS,
    "{\"device\":\"dragon_c214\",\"status\":\"online\",\"system\":\"telemetry_v1.0\"}");

  Serial.println(F("\n[READY] Sistema de telemetria ativo\n"));
}

// ═══════════════════════════════════════════════════════════
void loop() {
  // Mantém conexões ativas
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop();

  missionElapsed = millis() - missionStart;
  unsigned long now = millis();

  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;

    // ── Leitura dos sensores ──────────────────────────────
    float pressure    = readPressure();
    float temperature = readTemperature();
    float radiation   = readRadiation();
    float velocity    = readOrbitalVelocity();

    // Validação de leitura do DHT (pode retornar NaN)
    if (isnan(temperature)) {
      Serial.println(F("[WARN] Falha na leitura do DHT22 — usando último valor válido"));
      temperature = 22.0; // fallback
    }

    // ── Avaliação de status ───────────────────────────────
    alertLevel = evaluateStatus(pressure, temperature, radiation, velocity);

    // ── Publicação MQTT ───────────────────────────────────
    publishTelemetry(pressure, temperature, radiation, velocity, alertLevel);

    // ── Alertas específicos por sensor ───────────────────
    if (pressure < PRES_WARN_MIN || pressure > PRES_WARN_MAX) {
      publishAlert("pressure", pressure,
        (pressure < PRES_NOM_MIN || pressure > PRES_NOM_MAX) ? "CRITICAL" : "WARNING",
        "Pressao de cabine fora dos limites nominais");
    }
    if (radiation >= RAD_WARN) {
      publishAlert("radiation", radiation,
        (radiation >= RAD_CRIT) ? "CRITICAL" : "WARNING",
        "Nivel de radiacao elevado — possivel SAA ou solar flare");
    }
    if (velocity < VEL_WARN_MIN || velocity > VEL_WARN_MAX) {
      publishAlert("velocity", velocity, "WARNING",
        "Velocidade orbital fora da faixa de referencia ISS");
    }

    // ── Indicadores visuais/sonoros ───────────────────────
    setIndicators(alertLevel);
    if (alertLevel == 2) triggerBuzzer(3);

    // ── Log serial ────────────────────────────────────────
    Serial.printf("\n[MET %s] Telemetria publicada:\n", formatMET(missionElapsed).c_str());
    Serial.printf("  Pressao:    %.1f kPa   [%s]\n", pressure,    getStatusString(alertLevel).c_str());
    Serial.printf("  Temperatura:%.1f °C    [%s]\n", temperature, getStatusString(alertLevel).c_str());
    Serial.printf("  Radiacao:   %.2f mSv/h [%s]\n", radiation,   getStatusString(alertLevel).c_str());
    Serial.printf("  Velocidade: %.2f km/s  [%s]\n", velocity,    getStatusString(alertLevel).c_str());
    Serial.printf("  Status: %s\n", getStatusString(alertLevel).c_str());
  }
}

// ═══════════════════════════════════════════════════════════
//  LEITURA DE SENSORES
// ═══════════════════════════════════════════════════════════

float readPressure() {
  // No Wokwi: DHT22 fornece a base; mapeamos umidade → pressão
  // representando o sensor BMP280 que estaria em produção
  // Faixa nominal ISS: 101.3 kPa ± 2
  float raw = dht.readHumidity(); // 0–100 %
  if (isnan(raw)) raw = 50.0;
  // Mapeia 30–70% de umidade → 99–104 kPa (simulação realista)
  float pressure = map(raw, 30, 70, 990, 1040) / 10.0;
  pressure = constrain(pressure, 95.0, 108.0);
  return pressure;
}

float readTemperature() {
  float t = dht.readTemperature(); // °C
  return isnan(t) ? 22.0 : constrain(t, 10.0, 40.0);
}

float readRadiation() {
  // LDR no pino 34 — maior luminosidade = maior "contagem" Geiger
  int raw = analogRead(LDR_PIN); // 0–4095 (12-bit ADC)
  // Mapeia: escuro (0) → 0.1 mSv/h, muito claro (4095) → 2.0 mSv/h
  // Simula passagem pela South Atlantic Anomaly (SAA)
  float rad = (raw / 4095.0) * 2.0 + 0.05;
  return round(rad * 100) / 100.0;
}

float readOrbitalVelocity() {
  // Potenciômetro no pino 35 simulando telemetria de GPS/IMU
  // Órbita ISS: ~7.66 km/s
  int raw = analogRead(POT_PIN); // 0–4095
  float vel = 7.40 + (raw / 4095.0) * 0.55;
  return round(vel * 100) / 100.0;
}

// ═══════════════════════════════════════════════════════════
//  AVALIAÇÃO DE STATUS
// ═══════════════════════════════════════════════════════════
int evaluateStatus(float pres, float temp, float rad, float vel) {
  bool critical = false, warning = false;

  // Pressão
  if (pres < PRES_WARN_MIN || pres > PRES_WARN_MAX) {
    if (pres < PRES_NOM_MIN || pres > PRES_NOM_MAX) critical = true;
    else warning = true;
  }
  // Temperatura
  if (temp < TEMP_WARN_MIN || temp > TEMP_WARN_MAX) {
    if (temp < TEMP_NOM_MIN || temp > TEMP_NOM_MAX) critical = true;
    else warning = true;
  }
  // Radiação
  if (rad >= RAD_CRIT) critical = true;
  else if (rad >= RAD_WARN) warning = true;
  // Velocidade
  if (vel < VEL_WARN_MIN || vel > VEL_WARN_MAX) warning = true;

  if (critical) return 2;
  if (warning)  return 1;
  return 0;
}

// ═══════════════════════════════════════════════════════════
//  PUBLICAÇÃO MQTT
// ═══════════════════════════════════════════════════════════
void publishTelemetry(float pres, float temp, float rad, float vel, int status) {
  StaticJsonDocument<512> doc;

  doc["device"]      = "dragon_c214";
  doc["mission"]     = "ISS-Crew9-GS2026";
  doc["met_ms"]      = missionElapsed;
  doc["met"]         = formatMET(missionElapsed);
  doc["status"]      = getStatusString(status);

  JsonObject sensors = doc.createNestedObject("sensors");
  sensors["pressure_kpa"]    = serialized(String(pres, 1));
  sensors["temperature_c"]   = serialized(String(temp, 1));
  sensors["radiation_msvh"]  = serialized(String(rad, 2));
  sensors["velocity_kms"]    = serialized(String(vel, 2));

//  JsonObject thresholds = doc.createNestedObject("thresholds");
//  thresholds["pressure_nominal"]  = "99.0–103.0 kPa";
//  thresholds["temperature_nominal"] = "20.0–25.0 °C";
//  thresholds["radiation_warning"] = "0.8 mSv/h";
//  thresholds["radiation_critical"] = "1.5 mSv/h";

  char buffer[512];
  serializeJson(doc, buffer);
  ///mqttClient.publish(TOPIC_TELEMETRY, buffer);
  Serial.println("TOPIC:");
  Serial.println(TOPIC_TELEMETRY);
  Serial.println("PAYLOAD:");
  Serial.println(buffer);
  bool ok = mqttClient.publish(TOPIC_TELEMETRY, buffer);

  if (ok) {
    Serial.println("[MQTT] Publish OK");
  } else {
    Serial.println("[MQTT] Publish FAILED");
  }
}

void publishAlert(String sensor, float value, String level, String msg) {
  StaticJsonDocument<256> doc;
  doc["device"]  = "dragon_c214";
  doc["sensor"]  = sensor;
  doc["value"]   = value;
  doc["level"]   = level;
  doc["message"] = msg;
  doc["met"]     = formatMET(missionElapsed);

  char buffer[256];
  serializeJson(doc, buffer);
  mqttClient.publish(TOPIC_ALERTS, buffer);
  Serial.printf("[ALERT][%s] %s: %.2f\n", level.c_str(), sensor.c_str(), value);
}

// ═══════════════════════════════════════════════════════════
//  INDICADORES
// ═══════════════════════════════════════════════════════════
void setIndicators(int status) {
  digitalWrite(LED_GREEN,  status == 0 ? HIGH : LOW);
  digitalWrite(LED_YELLOW, status == 1 ? HIGH : LOW);
  digitalWrite(LED_RED,    status == 2 ? HIGH : LOW);
}

void triggerBuzzer(int pulses) {
  for (int i = 0; i < pulses; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

// ═══════════════════════════════════════════════════════════
//  CONECTIVIDADE
// ═══════════════════════════════════════════════════════════
void connectWiFi() {
  Serial.printf("[WiFi] Conectando a %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Conectado! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println(F("\n[WiFi] Falha na conexão — operando em modo offline"));
  }
}

void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.printf("[MQTT] Conectando ao broker %s...", MQTT_BROKER);
    if (mqttClient.connect(MQTT_CLIENT)) {
      Serial.println(F(" Conectado!"));
      mqttClient.subscribe("dragon/c214/cmd"); // Escuta comandos do ground control
    } else {
      Serial.printf(" Falhou (rc=%d). Tentando em 3s...\n", mqttClient.state());
      delay(3000);
    }
  }
}

// ═══════════════════════════════════════════════════════════
//  UTILITÁRIOS
// ═══════════════════════════════════════════════════════════
String getStatusString(int s) {
  if (s == 2) return "CRITICAL";
  if (s == 1) return "WARNING";
  return "NOMINAL";
}

String formatMET(unsigned long ms) {
  unsigned long s   = ms / 1000;
  unsigned long h   = s / 3600;
  unsigned long min = (s % 3600) / 60;
  unsigned long sec = s % 60;
  char buf[12];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, min, sec);
  return String(buf);
}
