# 1. **Descripción**
Aqui se puden encontrar los diferentes ejemplos para diferentes sensores en arduino ESP32 y ESP32-C3.
Primero busca internet, en caso de no ternerlo entra en modo bluetooth esperando recibir configuracion de WiFi,
luego repite lo mismo pero con la configuracion, en caso de estar configurado se queda pendiente del mqtt para enviar info
y si no, se queda esperando la info en el canal de mqtt senInfo.

# 2. **Configuración**

Al principio del codigo estan las sigueintes const
```
const char* mqtt_server = "IP";
const char* mqtt_user = "USER";
const char* mqtt_password = "PASSWORD";
```
es necesario configurar bien estos datos de vuestro servidor mqtt excepto mqtt_topic, este se deja como esta.

MUY IMPORTANTE:
Al subir el codigo al ESP32 el valor de mqtt_client debe de ser siempre ÚNICO y en arduino hacer:
Herramientas - Partition Scheme - Huge App (3MB No OTA/1MB SPIFFS)
