#include <Wire.h>
#include <Preferences.h>
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
const char* mqtt_client = "ESP32Client4_bg_hume_tierra122";
const char* mqtt_server = "terie.mooo.com";
const char* mqtt_topic = "senInfo";
const char* mqtt_user = "pi";
const char* mqtt_password = "wasimodo98_code";
bool wifiCreddata = false;
BLECharacteristic *pCharacteristic;
BLEService *pService;
BLEServer *pServer;
StaticJsonDocument<200> jsonTopic_save;
WiFiClient espClient;
PubSubClient client(espClient);

//sensor info
const int gpios[] = {34, 35, 32, 36, 39, 33};


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

    receivedMessage += value;

    if (receivedMessage.endsWith("\n")) {
      Serial.print("Complete message: ");
      Serial.println(receivedMessage.c_str());

      receivedMessage.trim();

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
    Serial.println("Connected to MQTT");
    Serial.println(topicStr.length());
    if (topicStr.length() > 0) {
      Serial.println("hay datos saved cabronesss"); // Aquí se usan comillas dobles
      mqtt_info = true;
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
    Serial.println("Recibió un mensaje del servidor:");
    Serial.println(topic);

    String receivedPayload = "";
    for (unsigned int i = 0; i < length; i++) {
        receivedPayload += (char)payload[i];
    }

    Serial.println("Comando recibido: " + receivedPayload);

    if (receivedPayload.startsWith("send")) {
        String alphanumericPart = receivedPayload.substring(5);

        if (alphanumericPart.length() > 0 && jsonTopic_save["token"] == alphanumericPart) {
            
            // Creamos un documento JSON para almacenar los valores de los sensores
            StaticJsonDocument<200> sensorDataJson;

            // Recorremos los GPIOs y leemos sus valores
            for (int i = 0; i < sizeof(gpios)/sizeof(gpios[0]); i++) {
                int sensorValue = analogRead(gpios[i]);
                String key = String(i + 1); // Usamos números del 1 al n como claves en el JSON
                sensorDataJson[key] = sensorValue; // Guardamos el valor en el JSON
                delay(1000);
            }

            // Creamos un nuevo documento JSON para el mensaje final que enviaremos
            StaticJsonDocument<300> jsonDoc2;

            // Agregamos los datos requeridos
            jsonDoc2["sensores"] = sensorDataJson; // Añadimos los datos de los sensores
            jsonDoc2["info"] = topic; // Añadimos la información del topic
            jsonDoc2["token"] = jsonTopic_save["token"]; // Añadimos el token
            jsonDoc2["space"] = jsonTopic_save["space"]; // Añadimos el espacio

            // Serializamos el documento JSON a una cadena de caracteres
            char jsonPayload[300];
            serializeJson(jsonDoc2, jsonPayload);

            // Enviamos el JSON al tópico MQTT
            const char* datoNow = jsonTopic_save["topic"];
            if (strlen(datoNow) > 0) {
                client.publish(datoNow, jsonPayload);
            } else {
                Serial.println("ERRORES EN SEND!!!!!!!!!");
            }
            sensorDataJson.clear();
            jsonDoc2.clear();
            //jsonPayload.clear();
        } else {
            Serial.println("mal token");
        }
    } else if (receivedPayload.startsWith("delete")) {
        borrarDato("prin", "wasii98");
        borrarDato("wifi", "ssid");
        borrarDato("wifi", "password");
        Serial.println("Datos borrados de EEPROM");
    } else {
        DynamicJsonDocument jsonDoc_mqtt(200);
        DeserializationError error = deserializeJson(jsonDoc_mqtt, receivedPayload);
        if (error) {
            Serial.println("Error al analizar JSON: ");
            Serial.println(error.c_str());
            Serial.println(receivedPayload);
        } else {
            if (jsonDoc_mqtt["topic"]) {
                Serial.println("JSON analizado correctamente");
                jsonTopic_save.clear();
                jsonTopic_save["topic"] = jsonDoc_mqtt["topic"];
                jsonTopic_save["token"] = jsonDoc_mqtt["token"];
                jsonTopic_save["name"] = jsonDoc_mqtt["name"];
                jsonTopic_save["space"] = jsonDoc_mqtt["space"];
                String jsonDoc_mqtt_f;
                serializeJson(jsonDoc_mqtt, jsonDoc_mqtt_f);
                escribirDato("prin", "wasii98", jsonDoc_mqtt_f);
                client.subscribe(jsonDoc_mqtt["topic"]);
                mqtt_info = true;
                changeState(WAITING_MQTT_INFO);
            }
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
int wifiRetryCount = 0;
void loop() {
  switch (currentState) {
  
  case CONNECTING_WIFI:
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to WiFi");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      wifiRetryCount = 0; // Reset retry counter
      changeState(CONNECTED_WIFI);
    } else if (millis() - stateStartTime > 15000) { // timeout de 15 segundos
      wifiRetryCount++;
      Serial.printf("Retry WiFi connection attempt #%d\n", wifiRetryCount);
  
      if (wifiRetryCount >= 9) {
        Serial.println("Failed after 3 attempts, starting BLE...");
        setupBLE();
        changeState(IDLE);
        wifiRetryCount = 0;
      } else {
        String ssid, password;
        loadCredentials(ssid, password);
        if (ssid.length() > 0 && password.length() > 0) {
          connectToWiFi(ssid.c_str(), password.c_str()); // reintentar
        }
      }
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
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi se ha desconectado, volviendo a conectar...");
        // Reseteas contadores si quieres
        wifiRetryCount = 0;
        // Cortas WiFi si procede
        WiFi.disconnect(); 
        delay(100);
        // Pasas al estado CONNECTING_WIFI
        changeState(CONNECTING_WIFI);
      }
      else if (!client.connected()) {
        Serial.println("MQTT se ha desconectado, intentando reconectar...");
        changeState(CONNECTING_MQTT);
      }
      client.loop();
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
