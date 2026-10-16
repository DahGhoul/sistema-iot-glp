# 🚨 Sistema IoT de Detección Temprana y Mitigación Activa de Fugas de GLP
**Universidad Nacional de Trujillo (UNT) — Facultad de Ingeniería**  
*Curso: Internet de las Cosas (IoT) — Ciclo VIII*

---

## 📋 Descripción del Proyecto
Sistema ciberfísico embebido basado en el microcontrolador **ESP32-WROOM-32** diseñado para la detección temprana de fugas de Gas Licuado de Petróleo (GLP/Propano/Butano), estimación de riesgo preventivo, alerta multimodal en tiempo real (Local, Dashboard Web y Telegram) y **mitigación activa mediante ventilación/extracción forzada**.

---

## 🛠️ Hardware y Conexión de Pines (Pinout)

> [!WARNING]
> Los módulos MQ-2 y MQ-5 se alimentan a **5V (VIN)** para su resistencia calefactora, pero entregan salidas analógicas de hasta 5V. **Es obligatorio usar un divisor de voltaje (10kΩ / 20kΩ)** en cada pin analógico para proteger las entradas del ESP32 (lógica de 3.3V).

| Componente | Pin del Componente | Pin ESP32 (GPIO) | Tipo / Notas |
| :--- | :--- | :--- | :--- |
| **Sensor MQ-5 (Principal GLP)** | AO (salida analógica) | **GPIO 35** (ADC1_CH7) | Mediante divisor 10k/20k |
| **Sensor MQ-2 (Humo / Gases)** | AO (salida analógica) | **GPIO 34** (ADC1_CH6) | Mediante divisor 10k/20k |
| **LED Verde (Normal)** | Ánodo (+) con 220Ω | **GPIO 25** | Salida Digital |
| **LED Rojo (Alerta)** | Ánodo (+) con 220Ω | **GPIO 23** | Salida Digital |
| **Buzzer Activo** | Positivo (+) | **GPIO 19** | Alarma Sonora Local |
| **Módulo Relé (Ventilador DC)**| Entrada de Señal (IN) | **GPIO 18** | Extracción Forzada de Gas |
| **GND Común** | Negativo (-) | **GND** | Tierra común de todo el circuito |
| **Alimentación Sensores** | VCC | **VIN (5V)** | Alimentación desde cable USB |

---

## ⚙️ Guía de Puesta en Marcha Rápida (Para el Compañero)

### 1. Requisitos Previos en Arduino IDE
1. Tener instalado el soporte para placas **ESP32** en Arduino IDE (`Herramientas > Placa > Administrador de placas > esp32 by Espressif Systems`).
2. Placa seleccionada: **ESP32 Dev Module** (o ESP32-WROOM-32).
3. Velocidad del Monitor Serie: **115200 baudios**.

### 2. Configuración de Credenciales
Abre el archivo `sensor.ino` y modifica únicamente la sección de configuración:

```cpp
// 1. Tu red WiFi local (o zona WiFi de tu celular)
const char* ssid     = "NOMBRE_DE_TU_WIFI";
const char* password = "PASSWORD_DE_TU_WIFI";

// 2. Credenciales del Bot de Telegram (Ya configuradas por defecto)
const char* BOT_TOKEN = "8527573181:AAGVGj-miB3DwjKTSxOROq-zP2DTHzLcHMg";
const char* CHAT_ID   = "6227427865"; // ID de chat personal o del grupo
```

### 3. Activar el Bot de Telegram (Obligatorio la primera vez)
Para que Telegram permita al bot enviarte mensajes:
1. Abre tu Telegram y busca: **`@sensor_glp_unt_bot`** (o entra a [t.me/sensor_glp_unt_bot](https://t.me/sensor_glp_unt_bot)).
2. Presiona el botón **"Iniciar"** (o envía `/start`).

---

## 🚀 Cómo Probar el Sistema

1. **Subir el código:** Carga `sensor.ino` a la placa ESP32.
2. **Obtener la IP:** Abre el Monitor Serie a 115200 baudios. Verás:
   ```text
   WiFi Conectado con éxito!
   Dirección Web: http://192.168.1.XX
   ```
3. **Abrir el Dashboard:** En tu computadora o celular (conectado al mismo WiFi), abre esa IP en el navegador Chrome.
4. **Simular Fuga de Gas:** Con un encendedor común (sin chispa, solo gas presionado 2 segundos) cerca del sensor MQ-5:
   - **En el circuito:** LED rojo encendido, buzzer sonando y **relé de extracción activado**.
   - **En la computadora:** Pantalla roja parpadeante en el dashboard web y sirena sonora instantánea.
   - **En Telegram:** Notificación de emergencia con las lecturas de los sensores y protocolo de acción.
5. **Control Remoto:** Haz clic en **"Silenciar Alarma"** desde el navegador web para apagar el buzzer de forma remota.

---

## 👥 Equipo de Trabajo (Grupo 5 - UNT)
- Cuba Moya, Diego Joel
- Herrera Rodriguez, Anghela Tatiana
- Polonio Ramos, Franco Imanol
- Robles Romero, Ronaldo Carlos
- Villa Valdiviezo, Favian

**Docente:** Dr. Gomez Avila, Jose Alberto
