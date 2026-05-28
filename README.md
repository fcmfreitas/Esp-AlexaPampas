# ESP32 - Automação Residencial (Alexa dos Pampas)

Este projeto implementa um sistema de automação residencial para o ESP32. Ele permite o controle de um relé, um servo motor e de um LED via MQTT (integrável com um aplicativo mobile que desenvolvi em Flutter), além de realizar o monitoramento de temperatura e umidade usando um sensor DHT22. 

O projeto conta com um **Portal Cativo (Captive Portal)** utilizando SPIFFS para a configuração inicial das credenciais de Wi-Fi, eliminando a necessidade de *hardcode* de senhas no código-fonte. A leitura dos sensores é gerenciada de forma assíncrona através de uma *Task* do **FreeRTOS**.

## Funcionalidades

- **Configuração Wi-Fi Dinâmica:** Se o ESP32 não encontrar a rede salva, ele cria um Access Point (`ESP32HOME`) para configuração via interface web.
- **Controle de Cargas (Relé):** Liga e desliga equipamentos pelo pino 22.
- **Controle de Luminosidade (PWM):** Ajuste de brilho de um LED (0 a 255) no pino 21.
- **Controle de Abertura/Fechamento (Servo Motor):** Movimentação de 0º a 180º no pino 17.
- **Monitoramento Ambiental:** Leitura de temperatura e umidade (DHT22) publicada a cada 5 segundos de forma assíncrona (FreeRTOS).

## Tópicos MQTT

O sistema se conecta ao broker público `broker.emqx.io` (porta 1883) com os seguintes tópicos:

| Ação | Tópico | Payloads Esperados / Enviados |
| :--- | :--- | :--- |
| **Comando Relé** | `PAMPA-RS/IOT/ALEXA_RELE` | `liga` / `desliga` |
| **Comando LED** | `PAMPA-RS/IOT/ALEXA_LED` | `ligar` / `desligar` / *[Valor 0 a 255]* |
| **Comando Servo**| `PAMPA-RS/IOT/ALEXA_SERVO` | `abre` / `fecha` |
| **Leitura Temp.**| `PAMPA-RS/IOT/ALEXA_TEMP` | *Ex: 25.4* (Publicado pelo ESP32) |
| **Leitura Umid.**| `PAMPA-RS/IOT/ALEXA_HUMID`| *Ex: 60.0* (Publicado pelo ESP32) |
