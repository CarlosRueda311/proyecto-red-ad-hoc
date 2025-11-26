#include "painlessMesh.h"
#include <TinyGPS++.h>

// ----------------- CONFIGURACIÓN MESH -----------------
#define MESH_PREFIX     "RedAgricola"
#define MESH_PASSWORD   "cultivo123"
#define MESH_PORT       5555

painlessMesh mesh;

// ----------------- CONFIGURACIÓN GPS (UART2) -----------------
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
HardwareSerial neogps(2);
TinyGPSPlus gps;

// --- COORDENADAS SIMULADAS (FALLBACK) ---
const double SIM_LAT = 4.660975;
const double SIM_LON = -74.059510;
// ----------------------------------------

// ----------------- CONFIGURACIÓN TEMT6000 -----------------
#define ANALOG_PIN 36       
#define MAX_ADC_VALUE 4095  

// ----------------- FUNCIONES -----------------

void sendHybridData() {
  // 1. Lectura del Sensor de Luz (TEMT6000)
  int rawValue = analogRead(ANALOG_PIN);
  float lightLevel = map(rawValue, 0, MAX_ADC_VALUE, 0, 1000);

  // 2. Obtención y Fallback de Datos GPS
  double latitud;
  double longitud;
  
  if (gps.location.isValid()) {
    // GPS REAL: Usar los valores del satélite
    latitud = gps.location.lat();
    longitud = gps.location.lng();
    Serial.println("✅ GPS REAL: Señal válida.");
  } else {
    // FALLBACK: Usar las coordenadas simuladas
    latitud = SIM_LAT;
    longitud = SIM_LON;
    Serial.println("⚠ USANDO FALLBACK: Coordenadas simuladas.");
  }

  // 3. Obtener el ID único del nodo Mesh
  uint32_t nodeId = mesh.getNodeId();
  
  // 4. Construir el payload JSON con todos los datos
  String payload = "{\"nodo\":" + String(nodeId) + 
                   ",\"lat\":" + String(latitud, 6) +      
                   ",\"lon\":" + String(longitud, 6) +     
                   ",\"raw\":" + String(rawValue) +        
                   ",\"lux_rel\":" + String(lightLevel) +  
                   "}";
                   
  mesh.sendBroadcast(payload);
  Serial.println("📤 Enviado a la red mesh: " + payload);
}

// ----------------- CALLBACKS DE MESH -----------------
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("📩 Mensaje recibido de nodo %u: %s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("🔗 Nuevo nodo conectado: %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.println("🔄 Cambios en la red mesh detectados");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("🕒 Tiempo ajustado por la red: %d\n", offset);
}

// ----------------- SETUP -----------------
unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  delay(3000); 
  Serial.println("\n=== Nodo Híbrido (GPS + TEMT6000) con Fallback iniciado ===");

  neogps.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN); 

  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION); 
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_STA);

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  Serial.println("✅ Red mesh inicializada.");
}

// ----------------- LOOP -----------------
void loop() {
  mesh.update();

  while (neogps.available() > 0) {
    gps.encode(neogps.read());
  }

  // Enviar datos cada 5 segundos
  if (millis() - lastSend > 5000) {
    sendHybridData();
    lastSend = millis();
  }
}
