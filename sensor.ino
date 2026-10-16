/*
  =============================================================================
  Sistema Ciberfísico IoT de Detección y Mitigación Activa de GLP - Fase 2
  Universidad Nacional de Trujillo (UNT) - Grupo 5
  Placa: ESP32-WROOM-32
  =============================================================================
  
  Características:
  - Fusión sensorial: MQ-5 (Principal GLP) y MQ-2 (Secundario Gases/Humo).
  - Mitigación activa inmediata: Control de Relé para Extractor/Ventilador (GPIO 18).
  - Telemetría en tiempo real: Dashboard Web AJAX sin recargas (Laptop/Móvil).
  - Alarma sonora local (Buzzer) + Alarma sonora web instantánea en navegador.
  - Notificaciones en segundo plano a Telegram sin retrasar la respuesta física.
  - Control bidireccional: Silenciado remoto de alarma desde la interfaz web.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>

// ---------- 1. Credenciales de Red ----------
const char* ssid     = "Aida";             // Modificar según la red de cada usuario
const char* password = "ChoLacho2020";

// ---------- 2. Credenciales Telegram ----------
const char* BOT_TOKEN = "8527573181:AAGVGj-miB3DwjKTSxOROq-zP2DTHzLcHMg";
const char* CHAT_ID   = "6227427865";      // ID de usuario o de grupo de Telegram

WebServer server(80);

// ---------- 3. Asignación de Pines ----------
const uint8_t PIN_MQ2       = 34;  // Entrada analógica ADC1 (Divisor 10k/20k)
const uint8_t PIN_MQ5       = 35;  // Entrada analógica ADC1 (Divisor 10k/20k)
const uint8_t PIN_LED_VERDE = 25;  // Indicador de Estado Normal
const uint8_t PIN_LED_ROJO  = 23;  // Indicador de Alerta
const uint8_t PIN_BUZZER    = 19;  // Alarma acústica local
const uint8_t PIN_EXTRACTOR = 18;  // Salida a Módulo Relé (Ventilación/Extractor)

// ---------- 4. Parámetros de Operación ----------
const uint16_t MUESTRAS_PROMEDIO = 16;
const int DELTA_PRECAUCION       = 150;
const int DELTA_ALERTA           = 400;

enum Estado { NORMAL, PRECAUCION, ALERTA };
Estado estado = NORMAL;
Estado estadoAnterior = NORMAL;

float baseMq2 = 0;
float baseMq5 = 0;
int delta2 = 0, delta5 = 0;
bool buzzerSilenciado = false;
uint32_t tUltimoTelegram = 0;

// Variables para envío asíncrono de Telegram
String mensajePendiente = "";
bool hayMensajePendiente = false;

// Lectura analógica estabilizada
int leerPromedio(uint8_t pin) {
  uint32_t suma = 0;
  for (uint16_t i = 0; i < MUESTRAS_PROMEDIO; i++) {
    suma += analogRead(pin);
    delay(2);
  }
  return suma / MUESTRAS_PROMEDIO;
}

// Control inmediato de actuadores físicos (Latencia < 1 ms)
void salidas(bool verde, bool rojo, bool buzzer, bool extractor) {
  digitalWrite(PIN_LED_VERDE, verde);
  digitalWrite(PIN_LED_ROJO, rojo);
  digitalWrite(PIN_BUZZER, (buzzer && !buzzerSilenciado) ? HIGH : LOW);
  digitalWrite(PIN_EXTRACTOR, extractor ? HIGH : LOW); // Activa relé del ventilador
}

// Envío HTTPS a Telegram
void enviarTelegram(String mensaje) {
  WiFiClientSecure client;
  client.setInsecure(); // No requiere cargar certificado X.509
  client.setTimeout(3000); // Límite de espera de 3 segundos para no demorar

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("[Telegram] Error al conectar con api.telegram.org");
    return;
  }

  mensaje.replace(" ", "%20");
  mensaje.replace("\n", "%0A");

  String url = "/bot" + String(BOT_TOKEN) + "/sendMessage?chat_id=" + String(CHAT_ID) + "&text=" + mensaje;
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: api.telegram.org\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("[Telegram] Alerta transmitida.");
}

// Tarea en segundo plano para Telegram (Core 0 de FreeRTOS)
void tareaSegundoPlanoTelegram(void *pvParameters) {
  for (;;) {
    if (hayMensajePendiente) {
      String msg = mensajePendiente;
      hayMensajePendiente = false;
      enviarTelegram(msg);
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// Disparador no bloqueante para Telegram
void notificarTelegramAsincrono(String msg) {
  mensajePendiente = msg;
  hayMensajePendiente = true;
}

// ---------- 5. Servidor Web con AJAX y Sirena Local en Laptop ----------
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Sistema Ciberfísico IoT - GLP UNT</title>
  <style>
    body { font-family: 'Segoe UI', Arial, sans-serif; background: #070d1e; color: #f8fafc; text-align: center; margin: 0; padding: 20px; }
    .card { background: #131d36; max-width: 480px; margin: auto; padding: 25px; border-radius: 20px; box-shadow: 0 15px 35px rgba(0,0,0,0.6); border: 1px solid #1e293b; }
    h2 { margin: 0 0 5px 0; font-size: 21px; color: #cbd5e1; }
    .subtitle { font-size: 13px; color: #64748b; margin-bottom: 20px; }
    .status-badge { font-size: 20px; font-weight: bold; padding: 14px; border-radius: 12px; margin: 15px 0; transition: all 0.3s; }
    .NORMAL { background: #15803d; color: #dcfce7; }
    .PRECAUCION { background: #b45309; color: #fef3c7; }
    .ALERTA { background: #dc2626; color: #ffffff; animation: pulse 0.7s infinite; }
    @keyframes pulse { 0% { transform: scale(1); box-shadow: 0 0 0 0 rgba(220, 38, 38, 0.7); } 70% { transform: scale(1.02); box-shadow: 0 0 0 15px rgba(220, 38, 38, 0); } 100% { transform: scale(1); } }
    .metrics { display: flex; justify-content: space-around; background: #0b1329; border-radius: 14px; padding: 15px; margin: 15px 0; }
    .metric-val { font-size: 32px; font-weight: bold; color: #38bdf8; }
    .metric-label { font-size: 13px; color: #94a3b8; }
    .actuator-box { background: #0f172a; border-radius: 10px; padding: 12px; margin: 15px 0; display: flex; justify-content: space-between; align-items: center; }
    .fan-status { font-weight: bold; padding: 4px 10px; border-radius: 6px; }
    .fan-off { background: #334155; color: #94a3b8; }
    .fan-on { background: #0284c7; color: white; animation: spin 1s linear infinite; }
    .btn { background: #2563eb; color: white; border: none; padding: 12px 24px; font-size: 15px; font-weight: bold; border-radius: 8px; cursor: pointer; transition: 0.2s; margin-top: 10px; }
    .btn:hover { background: #1d4ed8; }
    .footer { font-size: 12px; color: #64748b; margin-top: 20px; }
  </style>
</head>
<body>
  <div class="card">
    <h2>Sistema IoT de Seguridad GLP</h2>
    <div class="subtitle">Detección y Disipación Activa Forzada — UNT</div>
    
    <div id="statusBox" class="status-badge NORMAL">CONECTANDO...</div>
    
    <div class="metrics">
      <div>
        <div class="metric-label">Sensor MQ-5 (GLP)</div>
        <div id="valMq5" class="metric-val">0</div>
        <div class="metric-label">&Delta; cuentas ADC</div>
      </div>
      <div>
        <div class="metric-label">Sensor MQ-2 (Gases)</div>
        <div id="valMq2" class="metric-val">0</div>
        <div class="metric-label">&Delta; cuentas ADC</div>
      </div>
    </div>

    <div class="actuator-box">
      <span style="font-size: 14px;">Extractor de Ventilación (Relé):</span>
      <span id="fanStatus" class="fan-status fan-off">APAGADO</span>
    </div>

    <button class="btn" onclick="silenciar()">Silenciar Alarma Local</button>
    <div class="footer">Telemetría AJAX en tiempo real (Cero recargas)</div>
  </div>

  <script>
    let audioCtx = null;
    function beepSiren() {
      try {
        if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        const osc = audioCtx.createOscillator();
        const gain = audioCtx.createGain();
        osc.connect(gain);
        gain.connect(audioCtx.destination);
        osc.frequency.setValueAtTime(800, audioCtx.currentTime);
        osc.frequency.exponentialRampToValueAtTime(1400, audioCtx.currentTime + 0.3);
        gain.gain.setValueAtTime(0.2, audioCtx.currentTime);
        osc.start();
        osc.stop(audioCtx.currentTime + 0.3);
      } catch (e) {}
    }

    function actualizarDatos() {
      fetch('/api/data')
        .then(res => res.json())
        .then(data => {
          document.getElementById('valMq5').innerText = data.d5;
          document.getElementById('valMq2').innerText = data.d2;
          
          const box = document.getElementById('statusBox');
          box.className = 'status-badge ' + data.estado;
          
          const fan = document.getElementById('fanStatus');
          if (data.extractor) {
            fan.className = 'fan-status fan-on';
            fan.innerText = 'ACTIVADO (DISIPANDO)';
          } else {
            fan.className = 'fan-status fan-off';
            fan.innerText = 'EN ESPERA';
          }

          if (data.estado === 'NORMAL') {
            box.innerText = 'ESTADO: NORMAL (SEGURO)';
          } else if (data.estado === 'PRECAUCION') {
            box.innerText = 'PRECAUCIÓN (GAS LEVE)';
          } else {
            box.innerText = '🚨 ¡FUGA DE GLP! MITIGACIÓN ACTIVA';
            beepSiren(); // Alarma sonora instantánea en la laptop
          }
        })
        .catch(err => console.error(err));
    }

    function silenciar() {
      fetch('/api/silenciar');
    }

    setInterval(actualizarDatos, 1000);
    actualizarDatos();
  </script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

// Endpoint JSON para consumo del frontend
void handleApiData() {
  const char* nombres[] = {"NORMAL", "PRECAUCION", "ALERTA"};
  String json = "{";
  json += "\"d2\":" + String(delta2) + ",";
  json += "\"d5\":" + String(delta5) + ",";
  json += "\"estado\":\"" + String(nombres[estado]) + "\",";
  json += "\"extractor\":" + String((estado == ALERTA) ? "true" : "false") + ",";
  json += "\"silenciado\":" + String(buzzerSilenciado ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleApiSilenciar() {
  buzzerSilenciado = true;
  server.send(200, "text/plain", "OK");
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_EXTRACTOR, OUTPUT);
  salidas(false, false, false, false);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Serial.println("\n========================================================");
  Serial.println("Sistema Ciberfísico IoT de GLP - Mitigación y Detección");
  Serial.println("========================================================");

  // Conexión WiFi
  Serial.printf("Conectando a WiFi: %s\n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado con éxito!");
  Serial.print("Servidor Web disponible en: http://");
  Serial.println(WiFi.localIP());

  // Rutas del Servidor Web
  server.on("/", handleRoot);
  server.on("/api/data", handleApiData);
  server.on("/api/silenciar", handleApiSilenciar);
  server.begin();

  // Iniciar hilo de FreeRTOS para Telegram en Core 0
  xTaskCreatePinnedToCore(
    tareaSegundoPlanoTelegram,
    "TareaTelegram",
    8192,
    NULL,
    1,
    NULL,
    0
  );

  // Enviar confirmación de inicio en segundo plano
  notificarTelegramAsincrono("✅ Sistema IoT Iniciado con éxito.\nMonitoreando balones de gas con extracción automática lista.");

  // Calibración inicial de línea base
  Serial.println("Calibrando sensores en aire limpio...");
  float s2 = 0, s5 = 0;
  for (int i = 0; i < 20; i++) {
    s2 += leerPromedio(PIN_MQ2);
    s5 += leerPromedio(PIN_MQ5);
    delay(80);
  }
  baseMq2 = s2 / 20.0;
  baseMq5 = s5 / 20.0;
  salidas(true, false, false, false); // Normal (Verde)
  Serial.printf("Línea Base: MQ-2=%.0f | MQ-5=%.0f\n", baseMq2, baseMq5);
}

// ---------- Loop Principal (Core 1) ----------
void loop() {
  server.handleClient(); // Atiende peticiones web de la laptop

  static uint32_t tUltima = 0;
  if (millis() - tUltima >= 1000) {
    tUltima = millis();

    int mq2 = leerPromedio(PIN_MQ2);
    int mq5 = leerPromedio(PIN_MQ5);
    delta2 = mq2 - (int)baseMq2;
    delta5 = mq5 - (int)baseMq5;

    int deltaMax = max(delta2, delta5);

    if (deltaMax >= DELTA_ALERTA) {
      estado = ALERTA;
      // ACTUACIÓN INMEDIATA (< 1 ms): Buzzer ON + Extractor ON + LED Rojo ON
      salidas(false, true, true, true);

      // Notificación asíncrona a Telegram (no bloquea el microcontrolador)
      if (estadoAnterior != ALERTA || (millis() - tUltimoTelegram > 30000)) {
        tUltimoTelegram = millis();
        notificarTelegramAsincrono(
          "🚨 ¡ALERTA DE FUGA DE GLP! (MITIGACIÓN ACTIVADA)\n\n"
          "💨 Extractor de ventilación: ENCENDIDO\n"
          "⚠️ MQ-5 (Gas): +" + String(delta5) + " cuentas\n"
          "⚠️ MQ-2 (Gases): +" + String(delta2) + " cuentas\n\n"
          "📍 Ubicación: Cocina Principal\n"
          "🛑 Protocolo: Válvula forzada activada. Ventilar y no generar chispas."
        );
      }
    } else if (deltaMax >= DELTA_PRECAUCION) {
      estado = PRECAUCION;
      salidas(false, true, false, false);
      buzzerSilenciado = false;
    } else {
      estado = NORMAL;
      salidas(true, false, false, false); // Extractor OFF, Buzzer OFF, Verde ON
      buzzerSilenciado = false;
    }

    estadoAnterior = estado;
  }
}