#include <Wire.h>
#include <Preferences.h>
#include "DHT.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <SHT1x-ESP.h>
#include <BLEServer.h>
#include <ArduinoJson.h>
#include <NewPing.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

Preferences preferences;
bool mqtt_info = false;
bool wait_mqtt_info = false;
const char* mqtt_client = "Esp32Client2_dht22_senmamis2456f";
const char* mqtt_server = "HOST";
const char* mqtt_topic = "senInfo";
const char* mqtt_user = "USER";
const char* mqtt_password = "PASS";
bool wifiCreddata = false;
BLECharacteristic *pCharacteristic;
BLEServer *pServer;
BLEService *pService;
StaticJsonDocument<200> jsonTopic_save;
WiFiClient espClient;
PubSubClient client(espClient);

//sensor info
#define DHTPIN 4
#define DHTTYPE DHT22   // Sensor DHT22
DHT dht(DHTPIN, DHTTYPE);

float lastTemperature = 0.0;
char lastReceivedCommand[100] = "";
String receivedMessage = "";

enum State { IDLE, CONNECTING_WIFI, CONNECTED_WIFI, CONNECTING_MQTT, CONNECTED_MQTT, WAITING_MQTT_INFO, SENDING_SENSOR_DATA };
State currentState = IDLE;
unsigned long stateStartTime = 0;

void changeState(State newState) {
  currentState = newState;
  stateStartTime = millis();
}

void escribirDato(const char *espacio, const char *clave, const String &valor) {
  preferences.begin(espacio, false);
  preferences.putString(clave, valor);
  preferences.end();
}

String leerDato(const char *espacio, const char *clave) {
  preferences.begin(espacio, true);
  String valorLeido = preferences.getString(clave, "");
  preferences.end();
  return valorLeido;
}

void borrarDato(const char *espacio, const char *clave) {
  preferences.begin(espacio, false);
  preferences.remove(clave);
  preferences.end();
}

void saveCredentials(const char* ssid, const char* password) {
  escribirDato("wifiCreds", "ssid", ssid);
  escribirDato("wifiCreds", "password", password);
  Serial.println("Credentials saved to Preferences");
}

void loadCredentials(String& ssid, String& password) {
  ssid = leerDato("wifiCreds", "ssid");
  password = leerDato("wifiCreds", "password");
}

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    const String value = pCharacteristic->getValue();
    Serial.print("Received: ");
    Serial.println(value);

    // Append received fragment to the message
    receivedMessage += value;

    // Check if the message is complete
    if (receivedMessage.endsWith("\n")) {
      Serial.print("Complete message: ");
      Serial.println(receivedMessage.c_str());

      // Remove the newline character
      receivedMessage.trim();

      // Parse the received credentials (expected format: "SSID:PASSWORD")
      int delimiterIndex = receivedMessage.indexOf(':');
      if (delimiterIndex > 0) {
        String ssid = receivedMessage.substring(0, delimiterIndex);
        String password = receivedMessage.substring(delimiterIndex + 1);

        Serial.print("Parsed SSID: ");
        Serial.println(ssid);
        Serial.print("Parsed Password: ");
        Serial.println(password);

        saveCredentials(ssid.c_str(), password.c_str());
        connectToWiFi(ssid.c_str(), password.c_str());
      } else {
        Serial.println("Invalid credentials format");
      }

      // Clear the received message buffer
      receivedMessage = "";
    }
  }
};

void connectToWiFi(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  changeState(CONNECTING_WIFI);
}

void connectMqtt() {
  //wait_mqtt_info = true;
  //"prin", "wasii98", jsonDoc_mqtt_f
  Serial.println("aquituuuu");
  Serial.println(leerDato("prin", "wasii98"));
  String jsonString = leerDato("prin", "wasii98");
  // Crear un JsonDocument para almacenar los datos deserializados
  StaticJsonDocument<200> dataSaved;
  // Deserializar el String en el JsonDocument
  DeserializationError error = deserializeJson(dataSaved, jsonString);
  // Verificar si hubo un error en la deserialización
  if (error) {
    Serial.print("Error al deserializar JSON: ");
    Serial.println(error.c_str());
    client.setServer(mqtt_server, 1883);
    client.setCallback(RecibirMQTT);
    Serial.println("Connecting to MQTT...");
    if (client.connect(mqtt_client, mqtt_user, mqtt_password)) {
      Serial.println("Connected to MQTT");
      infoMQTT();
    }
    //changeState(CONNECTED_MQTT);
  }
  // Ahora puedes usar el JsonDocument
  const char* topic = dataSaved["topic"];
  const char* token = dataSaved["token"];
  String topicStr = String(topic);
  Serial.println(topic);
  Serial.println(token);
  Serial.println(topicStr);
  client.setServer(mqtt_server, 1883);
  client.setCallback(RecibirMQTT);
  Serial.println("Connecting to MQTT...");
  if (client.connect(mqtt_client, mqtt_user, mqtt_password)) {
    Serial.println("Connected to MQTT from good");
    Serial.println(topicStr.length());
    if (topicStr.length() > 0) {
      Serial.println("hay datos saved cabronesss"); // Aquí se usan comillas dobles
      mqtt_info = true;
      //wait_mqtt_info = true
      jsonTopic_save = dataSaved;
      changeState(CONNECTED_MQTT);
      client.subscribe(topic);
    } else {
      infoMQTT();
      changeState(CONNECTED_MQTT);
    }
  
  } else {
    Serial.print("MQTT connect failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    delay(5000); // Retry after 5 seconds
    changeState(CONNECTING_MQTT); // Retry connecting to MQTT
  }
  dataSaved.clear();
}

void setupBLE() {
  BLEDevice::init("Step-sen");
  pServer = BLEDevice::createServer();
  pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->setValue("Hello World");
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE server is ready and waiting for credentials");
}

void RecibirMQTT(char* topic, byte* payload, unsigned int length) {
  Serial.println("📩 Mensaje recibido en MQTT:");
  Serial.println(topic);

  String receivedCommand = "";
  for (int i = 0; i < length; i++) {
    receivedCommand += (char)payload[i];
  }

  Serial.println("Comando recibido: " + receivedCommand);
  
  if (receivedCommand.startsWith("send")) {
    // Extraer token y verificar
    String tokenRecibido = receivedCommand.substring(5);
    if (tokenRecibido != jsonTopic_save["token"]) {
      Serial.println("🚫 Token incorrecto");
      return;
    }

    // Reinicializar el sensor antes de cada lectura
    dht.begin();
    delay(2000);  // Permitir que el sensor se estabilice

    float temperatura = NAN, humedad = NAN;

    for (int i = 0; i < 5; i++) {
      humedad = dht.readHumidity();
      temperatura = dht.readTemperature();

      if (!isnan(temperatura) && !isnan(humedad)) {
        break;  // Si la lectura es válida, salimos del loop
      }

      Serial.println("⚠️ Lectura fallida, reintentando...");
      dht.begin(); // Reinicializar sensor
      delay(2000); // Esperar antes de intentar nuevamente
    }

    if (isnan(temperatura) || isnan(humedad)) {
      Serial.println("❌ Error: No se pudo obtener datos del sensor.");
      return;
    }

    Serial.print("✅ Temperatura: ");
    Serial.print(temperatura);
    Serial.print("°C | Humedad: ");
    Serial.print(humedad);
    Serial.println("%");

    // Enviar datos por MQTT
    StaticJsonDocument<200> jsonDoc;
    jsonDoc["temperatura"] = String(temperatura, 2);
    jsonDoc["humedad"] = String(humedad, 2);
    jsonDoc["info"] = topic;
    jsonDoc["token"] = jsonTopic_save["token"];
    jsonDoc["space"] = jsonTopic_save["space"];

    char jsonPayload[200];
    serializeJson(jsonDoc, jsonPayload);
    
    const char* datoNow = jsonTopic_save["topic"];
    if (strlen(datoNow) > 0) {
      client.publish(datoNow, jsonPayload);
      Serial.println("📤 Datos enviados por MQTT.");
    } else {
      Serial.println("❌ ERROR: No se ha definido un topic para enviar datos.");
    }
  }
}


void infoMQTT() {
  Serial.println("Enviando petición para información MQTT");
  try {
    if (client.subscribe(mqtt_topic)) {
      if (client.publish(mqtt_topic, "infoplss")) {
        wait_mqtt_info = true;
        Serial.println("Solicitud de información MQTT enviada, esperando respuesta...");
      } else {
        throw "Error al publicar mensaje en el tema MQTT";
      }
    } else {
      throw "Error al suscribirse al tema MQTT";
    }
  } catch (const char *error) {
    Serial.print("Error en infoMQTT(): ");
    Serial.println(error);
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  String ssid, password;
  loadCredentials(ssid, password);

  if (ssid.length() > 0 && password.length() > 0) {
    Serial.println("Credentials found in Preferences. Connecting to WiFi...");
    changeState(CONNECTING_WIFI);
    connectToWiFi(ssid.c_str(), password.c_str());
  } else {
    Serial.println("No credentials found in Preferences. Starting BLE server...");
    setupBLE();
  }
}

void loop() {
  switch (currentState) {
    case CONNECTING_WIFI:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        wifiCreddata = true;
        changeState(CONNECTED_WIFI);
      } else if (millis() - stateStartTime > 20000) { // 20 seconds timeout
        Serial.println("Failed to connect to WiFi after multiple attempts. Starting BLE server...");
        setupBLE();
        changeState(IDLE);
      }
      break;

    case CONNECTED_WIFI:
      connectMqtt();
      break;

    case CONNECTING_MQTT:
      // Aquí intentas conectar a MQTT. Mantenemos client.loop() para procesar la conexión
      client.loop();
      break;

    case CONNECTED_MQTT:
      // *** Verificar WiFi y MQTT en caso de desconexión ***
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi se ha desconectado, volviendo a conectar...");
        changeState(CONNECTING_WIFI); 
      }
      if (!client.connected()) {
        Serial.println("MQTT se ha desconectado, intentando reconectar...");
        changeState(CONNECTING_MQTT);
      }

      if (wait_mqtt_info) {
        Serial.println("Waiting for MQTT info...");
      }
      client.loop(); // Mantener la conexión MQTT
      break;

    case WAITING_MQTT_INFO:
      // *** Igual que en CONNECTED_MQTT ***
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi se ha desconectado, volviendo a conectar...");
        changeState(CONNECTING_WIFI); 
      }
      if (!client.connected()) {
        Serial.println("MQTT se ha desconectado, intentando reconectar...");
        changeState(CONNECTING_MQTT);
      }
      client.loop();
      break;

    case SENDING_SENSOR_DATA:
      // Add your sensor data sending logic here
      break;

    default:
      break;
  }
  delay(100);
}
