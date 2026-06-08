# 🚀 Dragon C214 Telemetry System

## Global Solution 2026 – Edge Computing & Computer Systems

### 📖 Descrição

Este projeto apresenta uma solução de telemetria para a cápsula espacial Dragon C214 da SpaceX, desenvolvida utilizando conceitos de Edge Computing, Internet das Coisas (IoT), MQTT e monitoramento em tempo real.

O objetivo é monitorar parâmetros críticos da espaçonave durante a missão e transmitir essas informações para a equipe em terra, permitindo o acompanhamento contínuo das condições operacionais da cápsula.

A solução utiliza um ESP32 como dispositivo de borda (Edge Device), responsável pela coleta, processamento e classificação dos dados antes do envio ao backend FIWARE através do protocolo MQTT.

---

## 🛰️ Arquitetura da Solução

```text
ESP32
   │
   ├── Temperatura (DHT22)
   ├── Pressão Interna
   ├── Radiação Cósmica (LDR)
   └── Velocidade Orbital (Potenciômetro)
   │
   ▼
MQTT Broker
   │
   ▼
FIWARE IoT Agent
   │
   ▼
Orion Context Broker
   │
   ▼
Node-RED Dashboard
```

---

## ⚡ Edge Computing

O processamento dos dados ocorre diretamente no ESP32.

O dispositivo realiza a análise dos parâmetros monitorados e classifica automaticamente a condição da cápsula em três estados:

* NOMINAL
* WARNING
* CRITICAL

Dessa forma, decisões e alertas podem ser gerados localmente antes mesmo da transmissão dos dados para a infraestrutura central.

---

## 📊 Parâmetros Monitorados

| Parâmetro             | Sensor                          |
| --------------------- | ------------------------------- |
| Temperatura da Cabine | DHT22                           |
| Pressão Interna       | Simulação baseada em telemetria |
| Radiação Cósmica      | LDR                             |
| Velocidade Orbital    | Potenciômetro                   |

---

## 🚨 Sistema de Alertas

A solução possui mecanismos de alerta visual e sonoro.

| Atuador      | Função             |
| ------------ | ------------------ |
| LED Verde    | Operação Normal    |
| LED Amarelo  | Situação de Alerta |
| LED Vermelho | Situação Crítica   |
| Buzzer       | Alerta Sonoro      |

---

## 📡 Comunicação MQTT

Os dados são publicados periodicamente através do protocolo MQTT.

### Tópicos Utilizados

```text
dragon/c214/telemetry
dragon/c214/alerts
dragon/c214/status
```

---

## 📈 Dashboard

O monitoramento em tempo real é realizado através do Node-RED Dashboard.

### Funcionalidades

* Visualização em tempo real dos sensores
* Indicadores de criticidade
* Gráficos históricos
* Monitoramento da missão
* Sistema de alertas

---

## 🛠️ Tecnologias Utilizadas

* ESP32
* Arduino Framework
* MQTT
* Mosquitto
* FIWARE IoT Agent
* Orion Context Broker
* MongoDB
* Node-RED
* Docker
* Wokwi

---

## 📷 Demonstração

### Wokwi

Adicionar captura da simulação do ESP32.

### Dashboard

Adicionar captura do dashboard em execução.

### Arquitetura

Adicionar imagem da arquitetura da solução.

---

## 🔗 Links

### Repositório GitHub

https://github.com/NBittent/GS2_EdgeComputing

### Simulação Wokwi

https://wokwi.com/projects/466225684447476737

---

## 👥 Integrantes

Adicionar os nomes dos integrantes do grupo.

---

## 🎯 Conclusão

O projeto demonstra a aplicação prática de Edge Computing em sistemas de telemetria aeroespacial, permitindo o monitoramento em tempo real de parâmetros críticos da cápsula Dragon C214 através de processamento local, comunicação MQTT e visualização centralizada utilizando a plataforma FIWARE e o Node-RED Dashboard.
