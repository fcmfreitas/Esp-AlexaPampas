#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <ESP32Servo.h>

// CONFIGURAÇÕES WEB E MEMÓRIA
WebServer server(80);
Preferences memoria;
String ssid, pass;
int flag = 0;

// CONFIGURAÇÕES MQTT
const char* mqtt_broker = "broker.emqx.io";
const int mqtt_port = 1883;
#define MQTT_ID "esp_alexa_pampas"

// Tópicos
#define TOPICO_LED   "PAMPA-RS/IOT/ALEXA_LED"
#define TOPICO_HUMID "PAMPA-RS/IOT/ALEXA_HUMID"
#define TOPICO_TEMP  "PAMPA-RS/IOT/ALEXA_TEMP"
#define TOPICO_RELE  "PAMPA-RS/IOT/ALEXA_RELE"
#define TOPICO_SERVO "PAMPA-RS/IOT/ALEXA_SERVO" 

WiFiClient espClient;
PubSubClient MQTT(espClient);

// HARDWARE E SENSORES
// Relé real configurado no pino 22
#define PINO_RELE 22 
#define PINO_LED 21

// Servo Motor
Servo myservo;
int servoPin = 17;

// DHT22
#define DHTPIN 4     
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// PROTÓTIPOS DE FUNÇÕES
void setupAP();
void setupSTA();
void PaginaConfig();
void PaginaSalva();
void conectaBroker();
void callback(char* topic, byte* payload, unsigned int length);
void TaskSensores(void *pvParameters);

// FUNÇÕES DO PORTAL CAPTIVE
void PaginaConfig() {
  File arquivo = SPIFFS.open("/index.html", "r");
  server.streamFile(arquivo, "text/html");
  arquivo.close();
}

void PaginaSalva() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    String NovoSSID = server.arg("ssid");
    String NovaSenha = server.arg("password");
    
    memoria.begin("wifi", false);
    memoria.putString("ssid", NovoSSID);
    memoria.putString("password", NovaSenha);
    memoria.end();
    
    server.send(200, "text/html", "<h3>Configuração salva! Reinicie o ESP32.</h3>");
  } else {
    server.send(400, "text/plain", "Erro: parâmetros inválidos");
  }
}

void setupAP() {
  flag = 0;
  WiFi.softAP("ESP32HOME", "12345678");
  server.on("/", HTTP_GET, PaginaConfig);
  server.on("/save", HTTP_POST, PaginaSalva);
  server.serveStatic("/", SPIFFS, "/");
  server.begin();
  Serial.println("Modo AP iniciado. Conecte-se a ESP32HOME e acesse 192.168.4.1");
}

void setupSTA() {
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("Conectando em %s...\n", ssid.c_str());
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado com sucesso!");
    Serial.println(WiFi.localIP());
    flag = 1;

    // Inicializa a Task do DHT apenas quando a rede estiver conectada
    xTaskCreatePinnedToCore(TaskSensores, "TaskSensores", 2048, NULL, 1, NULL, 1);
  } else {
    Serial.println("\nFalha ao conectar, iniciando AP novamente...");
    setupAP();
  }
}

// FUNÇÕES MQTT
void conectaBroker() {
  if (!MQTT.connected()) {
    Serial.print("Conectando ao Broker MQTT...");
    if (MQTT.connect(MQTT_ID)) {
      Serial.println(" Conectado!");
      // Inscreve nos tópicos que vão receber comandos
      MQTT.subscribe(TOPICO_RELE);
      MQTT.subscribe(TOPICO_SERVO);
      MQTT.subscribe(TOPICO_LED);
    } else {
      Serial.print(" Falha, rc=");
      Serial.println(MQTT.state());
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String mensagem = "";
  for (int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }
  
  Serial.printf("Mensagem recebida [%s]: %s\n", topic, mensagem.c_str());

  // Lógica do Relé 
  if (String(topic) == TOPICO_RELE) {
    if (mensagem == "liga") {
      digitalWrite(PINO_RELE, HIGH);
    } else if (mensagem == "desliga") {
      digitalWrite(PINO_RELE, LOW);
    }
  }

  // Lógica do LED (Dimerização PWM no Pino 21)
  else if (String(topic) == TOPICO_LED) {
    
    int valorPWM;

    // Verifica se a mensagem é um comando de texto para ligar/desligar
    if (mensagem == "ligar") {
      valorPWM = 255; // Liga no 100%
    } 
    else if (mensagem == "desligar" ) {
      valorPWM = 0;   // Apaga totalmente
    } 
    else {
      // converte a string do número que o app enviou
      valorPWM = mensagem.toInt();
    }
    
    // Aplica o valor final PWM (0 a 255) ao pino do LED
    analogWrite(PINO_LED, valorPWM);
    
    Serial.printf("LED Ajustado para PWM: %d\n", valorPWM);
  }
  
  // Lógica do Servo
  else if (String(topic) == TOPICO_SERVO) {
    if (mensagem == "abre") {
      myservo.write(180); // Abre a janela de uma vez
    } else if (mensagem == "fecha") {
      myservo.write(0);   // Fecha a janela de uma vez
    }
  }
}

// TASK DO FREERTOS (Publicação de Sensores)
void TaskSensores(void *pvParameters) {
  long long ultimoTempoDHT = 0;
  const int intervaloPublicacao = 5000; // Publica a cada 5 segundos
  
  for (;;) {
    if (flag == 1 && MQTT.connected()) {
      if (millis() - ultimoTempoDHT > intervaloPublicacao) {
        ultimoTempoDHT = millis(); 
        
        float temp = dht.readTemperature();
        float umid = dht.readHumidity();
        
        if (!isnan(temp) && !isnan(umid)) {
          MQTT.publish(TOPICO_TEMP, String(temp, 1).c_str());
          MQTT.publish(TOPICO_HUMID, String(umid, 1).c_str());
          Serial.printf("Publicado -> Temp: %.1f°C | Umid: %.1f%%\n", temp, umid);
        } else {
          Serial.println("Falha na leitura do DHT!");
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500)); // Aguarda meio segundo antes de checar novamente
  }
}

// SETUP E LOOP
void setup() {
  Serial.begin(115200);

  // Configuração dos Pinos
  pinMode(PINO_RELE, OUTPUT);
  pinMode(PINO_LED, OUTPUT);
  
  // Nota: Muitos módulos de relé ligam em nível baixo (LOW). 
  // Se o seu relé iniciar atracado, mude a linha abaixo para HIGH.
  digitalWrite(PINO_RELE, LOW); 
  digitalWrite(PINO_LED, LOW); 

  myservo.setPeriodHertz(50);    
  myservo.attach(servoPin, 500, 2500); // Valores 180º
  myservo.write(0); // Inicia fechado

  dht.begin();

  // Configuração MQTT
  MQTT.setServer(mqtt_broker, mqtt_port);
  MQTT.setCallback(callback);

  if (!SPIFFS.begin(true)) {
    Serial.println("Erro ao montar SPIFFS");
    return;
  }

  memoria.begin("wifi", true);
  ssid = memoria.getString("ssid", "");
  pass = memoria.getString("password", "");
  memoria.end();

  if (ssid == "" || pass == "") {
    setupAP();
  } else {
    setupSTA();
  }
}

void loop() {
  // Se estiver no modo AP, o servidor web é a prioridade
  if (flag == 0) {
    server.handleClient();
  } 
  // Se estiver no modo Station (Conectado ao roteador)
  else if (flag == 1) {
    // Reconexão do WiFi se cair
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Conexão WiFi perdida!");
      setupSTA();
    } else {
      // Reconexão e rotina do MQTT
      conectaBroker();
      MQTT.loop();
    }
  }
  
  // Um pequeno delay no loop principal para evitar watchdog timer resets
  delay(10); 
}