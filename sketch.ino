
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <time.h>
#include <math.h>

// ==========================================
// VINHERIA AGNELLO - CHECKPOINT 05
// GRUPO 067
// ESP32 + DHT22 + LDR + MQTT + NTP
// ==========================================

// PINOS DOS SENSORES
#define DHT_PIN 15
#define LDR_PIN 34

DHTesp dht;

// CONFIGURACAO WI-FI
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// SERVIDOR MQTT DO PROFESSOR
const char* mqtt_server = "3.135.234.101";
const int mqtt_port = 1883;

// TOPICOS MQTT
const char* topico_dados =
  "vinheria/agnello/cp05/grupo067/dados";

const char* topico_comandos =
  "vinheria/agnello/cp05/grupo067/comandos";

// CLIENTE MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// INTERVALO DE PUBLICACAO
unsigned long ultimaPublicacao = 0;
const unsigned long intervalo = 5000;

// ==========================================
// RECEBIMENTO DE COMANDOS MQTT
// ==========================================

void callback(
  char* topic,
  byte* payload,
  unsigned int length
) {
  String mensagem = "";

  for (unsigned int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }

  Serial.println();
  Serial.println("===== COMANDO MQTT =====");

  Serial.print("Topico: ");
  Serial.println(topic);

  Serial.print("Comando recebido: ");
  Serial.println(mensagem);

  Serial.println("========================");
}

// ==========================================
// CONEXAO WI-FI
// ==========================================

void conectarWiFi() {
  Serial.print("Conectando ao Wi-Fi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi conectado!");

  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());
}

// ==========================================
// SINCRONIZACAO NTP
// ==========================================

void sincronizarHorario() {
  Serial.println();
  Serial.println("Sincronizando horario NTP...");

  // Relogio interno sincronizado em UTC
  configTime(
    0,
    0,
    "pool.ntp.org",
    "time.google.com"
  );

  struct tm horario;

  if (getLocalTime(&horario, 10000)) {
    Serial.println("Horario sincronizado!");
  } else {
    Serial.println(
      "Aviso: NTP ainda nao sincronizado."
    );
  }
}

// ==========================================
// CONEXAO MQTT
// ==========================================

void conectarMQTT() {
  while (!client.connected()) {

    Serial.print(
      "Conectando ao servidor MQTT... "
    );

    String clientId = "VinheriaGrupo067-";
    clientId += String(
      (uint32_t)ESP.getEfuseMac(),
      HEX
    );

    if (client.connect(clientId.c_str())) {

      Serial.println("CONECTADO!");

      if (client.subscribe(topico_comandos)) {
        Serial.println(
          "Inscrito no topico de comandos."
        );
      } else {
        Serial.println(
          "Falha ao inscrever no topico."
        );
      }

    } else {

      Serial.print("Falha MQTT. Codigo: ");
      Serial.println(client.state());

      delay(5000);
    }
  }
}

// ==========================================
// CONFIGURACAO INICIAL
// ==========================================

void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println("===========================");
  Serial.println("    VINHERIA AGNELLO");
  Serial.println("      CHECKPOINT 05");
  Serial.println("        GRUPO 067");
  Serial.println("===========================");

  // Inicializa sensores
  dht.setup(DHT_PIN, DHTesp::DHT22);
  pinMode(LDR_PIN, INPUT);

  // Conecta ao Wi-Fi
  conectarWiFi();

  // Sincroniza data e hora
  sincronizarHorario();

  // Configura MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(512);

  Serial.println();
  Serial.println("Sistema inicializado!");
}

// ==========================================
// LEITURA E PUBLICACAO DOS SENSORES
// ==========================================

void publicarSensores() {

  // Leitura do DHT22
  TempAndHumidity dados =
    dht.getTempAndHumidity();

  // Leitura do LDR
  int luminosidade = analogRead(LDR_PIN);

  // Valida sensor
  if (
    isnan(dados.temperature) ||
    isnan(dados.humidity)
  ) {
    Serial.println(
      "Erro na leitura do DHT22!"
    );
    return;
  }

  // ======================================
  // DATA E HORA DE BRASILIA (UTC-3)
  // ======================================

  time_t agora = time(nullptr);

  // Verifica sincronizacao NTP
  if (agora < 1609459200) {
    Serial.println(
      "Aguardando sincronizacao NTP..."
    );
    return;
  }

  // Converte UTC para Brasilia
  time_t agoraBrasil = agora - (3 * 3600);

  struct tm horarioBrasil;
  gmtime_r(&agoraBrasil, &horarioBrasil);

  char timestampBrasil[35];

  strftime(
    timestampBrasil,
    sizeof(timestampBrasil),
    "%Y-%m-%dT%H:%M:%S-03:00",
    &horarioBrasil
  );

  // ======================================
  // MONTA JSON
  // ======================================

  String mensagem = "{";

  mensagem += "\"temperatura\":";
  mensagem += String(dados.temperature, 1);

  mensagem += ",\"umidade\":";
  mensagem += String(dados.humidity, 1);

  mensagem += ",\"luminosidade\":";
  mensagem += String(luminosidade);

  mensagem += ",\"timestamp\":\"";
  mensagem += String(timestampBrasil);
  mensagem += "\"";

  mensagem += "}";

  // ======================================
  // PUBLICA NO MQTT
  // ======================================

  bool publicado = client.publish(
    topico_dados,
    mensagem.c_str()
  );

  // ======================================
  // MONITOR SERIAL
  // ======================================

  Serial.println();
  Serial.println(
    "----- LEITURA DOS SENSORES -----"
  );

  Serial.print("Temperatura: ");
  Serial.print(dados.temperature, 1);
  Serial.println(" C");

  Serial.print("Umidade: ");
  Serial.print(dados.humidity, 1);
  Serial.println(" %");

  Serial.print("Luminosidade (ADC): ");
  Serial.println(luminosidade);

  Serial.print("Horario Brasilia: ");
  Serial.println(timestampBrasil);

  // ALERTAS DE TEMPERATURA
  if (dados.temperature > 16) {
    Serial.println(
      "ALERTA: Temperatura elevada!"
    );
  } else if (dados.temperature < 10) {
    Serial.println(
      "ALERTA: Temperatura baixa!"
    );
  } else {
    Serial.println(
      "Temperatura dentro da faixa."
    );
  }

  // ALERTAS DE UMIDADE
  if (
    dados.humidity < 60 ||
    dados.humidity > 80
  ) {
    Serial.println(
      "ALERTA: Umidade inadequada!"
    );
  } else {
    Serial.println(
      "Umidade adequada."
    );
  }

  Serial.println();

  Serial.print("JSON: ");
  Serial.println(mensagem);

  if (publicado) {
    Serial.println(
      "Publicacao MQTT enviada!"
    );
  } else {
    Serial.println(
      "Falha ao publicar MQTT!"
    );
  }

  Serial.println(
    "-------------------------------"
  );
}

// ==========================================
// LOOP PRINCIPAL
// ==========================================

void loop() {

  // Reconecta Wi-Fi se necessario
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  // Reconecta MQTT se necessario
  if (!client.connected()) {
    conectarMQTT();
  }

  // Processa comandos recebidos
  client.loop();

  // Publica a cada 5 segundos
  unsigned long agora = millis();

  if (
    ultimaPublicacao == 0 ||
    agora - ultimaPublicacao >= intervalo
  ) {
    ultimaPublicacao = agora;
    publicarSensores();
  }

  delay(10);
}
