#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>
#include <WiFi.h>

#include <ArduinoJson.hpp>

#include "esp_camera.h"
#include "time.h"

#define CAMERA_MODEL_AI_THINKER  // Modelo de camara, tiene PSRAM

#include "camera_pins.h"

#define Flash_GPIO_NUM 4
#define BuiltInLED_GPIO_NUM 33
#define PIR_GPIO_NUM 12
#define Relay_GPIO_NUM 15

unsigned long tiempoJson;
unsigned long tiempoPoll;
unsigned long tiempoAlarma;
unsigned long tiempoNTP;

int apagarAlarma = 0;
int sensorPIR = 0;
int alarma = 0;
String modo = "vigilancia";
String json;
String jsonRecibido;

// Configuracion servidor NTP

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -14400;
const int daylightOffset_sec = 3600;

void printLocalTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Fallo al obtener la hora");
        analogWrite(Flash_GPIO_NUM, 1);
        delay(1000);
        analogWrite(Flash_GPIO_NUM, 0);
        return;
    }
    Serial.print("Fecha: ");
    Serial.println(&timeinfo, "%A %d de %B de %Y. La hora es: %H:%M:%S");

    int currentHour = timeinfo.tm_hour;
    Serial.println(currentHour);
}

// PWM LED Flash pulso

unsigned long tiempoLED = 0;
int brightness = 0;
boolean fadingIn = true;

void pulsoLEDalarma()  // Pulso intermitente
{
    if (millis() - tiempoLED >= 20) {
        if (fadingIn) {
            brightness += 2;
            if (brightness >= 40) {
                brightness = 40;
                fadingIn = false;
            }
        } else {
            brightness -= 2;
            if (brightness <= 0) {
                brightness = 0;
                fadingIn = true;
            }
        }
        tiempoLED = millis();
        analogWrite(Flash_GPIO_NUM, brightness);  // Set the LED brightness
    }
}

void pulsoLEDmodo()  // Un pulso para modo vigilancia, dos para modo nocturno
{
    analogWrite(Flash_GPIO_NUM, 1);  // Set the LED brightness
    delay(200);
    analogWrite(Flash_GPIO_NUM, 0);  // Set the LED brightness
    if (modo == "nocturno") {
        delay(200);
        analogWrite(Flash_GPIO_NUM, 1);  // Set the LED brightness
        delay(200);
        analogWrite(Flash_GPIO_NUM, 0);  // Set the LED brightness
    }
}

// JSON enviado

void serializarObjetoPIR() {
    json = "";
    StaticJsonDocument<300> doc;
    doc["sensorPIR"] = sensorPIR;

    serializeJson(doc, json);
    json = json + "\n";
    sensorPIR = 0;
}

void serializarObjetoModo() {
    json = "";
    StaticJsonDocument<300> doc;
    doc["modo"] = modo;

    serializeJson(doc, json);
    json = json + "\n";
}

// JSON recibido

void deserializarObjeto() {
    StaticJsonDocument<300> doc;
    DeserializationError error = deserializeJson(doc, jsonRecibido);
    if (error) {
        return;
    }

    JsonObject obj = doc.as<JsonObject>();
    if (obj.containsKey("modo")) {
        modo = doc["modo"].as<String>();
        pulsoLEDmodo();
    }
    if (obj.containsKey("apagarAlarma")) {
        apagarAlarma = doc["apagarAlarma"];
    }
    if (obj.containsKey("flash")) {
        if (doc["flash"] == 1) {
            analogWrite(Flash_GPIO_NUM, 40);
        } else {
            analogWrite(Flash_GPIO_NUM, 0);
        }
    }

    jsonRecibido = "0";
}

// Credenciales de WiFi

const char* ssid = "xxxx";
const char* password = "xxxx";
const char* websockets_server_host = "xxxx";
const uint16_t websockets_server_port = 8080;

using namespace websockets;
WebsocketsClient client;

// Funcion que realiza la interrupcion del sensor PIR

void detectaMovimiento() {
    sensorPIR = 1;
}  // solamente cambia el estado de la bandera sensorPIR, las interrupciones
   // deben ser instrucciones sencillas

void startCameraServer();

void setup() {
    pinMode(Flash_GPIO_NUM, OUTPUT);        // Configura el pin 4 como salida
    pinMode(BuiltInLED_GPIO_NUM, OUTPUT);   // Configura el pin 33 como salida
    pinMode(PIR_GPIO_NUM, INPUT_PULLDOWN);  // Configura el pin 12 como entrada
    attachInterrupt(digitalPinToInterrupt(PIR_GPIO_NUM), detectaMovimiento,
                    HIGH);  // Interrupcion por señal en alto sensor PIR
    pinMode(Relay_GPIO_NUM, OUTPUT);    // Configura el pin 15 como salida
    digitalWrite(Relay_GPIO_NUM, LOW);  // Escribe el pin 15 en bajo

    Serial.begin(115200);
    Serial.setDebugOutput(true);
    Serial.println();

    // Configuracion camara

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.frame_size = FRAMESIZE_VGA;
    config.pixel_format = PIXFORMAT_JPEG;  // para streaming
    // config.pixel_format = PIXFORMAT_RGB565; // para reconocimiento facial
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 1;
    config.fb_count = 1;

    // Si tiene PSRAM, Inicia con resolucion UXGA para mayor resolucion y
    // calidad
    if (config.pixel_format == PIXFORMAT_JPEG) {
        if (psramFound()) {
            config.jpeg_quality = 10;
            config.fb_count = 2;
            config.grab_mode = CAMERA_GRAB_LATEST;
        } else {
            // Limita el tamaño del frame cuando el modelo no tiene PSRAM
            config.frame_size = FRAMESIZE_SVGA;
            config.fb_location = CAMERA_FB_IN_DRAM;
        }
    } else {
        // Mejor opcion para reconocimiento facial
        config.frame_size = FRAMESIZE_240X240;
    }

    // Iniciacion de la camara
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Error en la iniciacion de la camara: 0x%x", err);
        return;
    }

    sensor_t* s = esp_camera_sensor_get();
    // Los sensores iniciales son volteados verticalmente y los colores
    // saturados
    if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1);        // flip
        s->set_brightness(s, 1);   // incrementa el brillo
        s->set_saturation(s, -2);  // reduce la saturacion
    }
    // Reduce el tamano del frame para mayor frame rate
    if (config.pixel_format == PIXFORMAT_JPEG) {
        s->set_framesize(s, FRAMESIZE_VGA);
    }

    // Fin configuracion camara

    WiFi.begin(ssid, password);
    WiFi.setSleep(false);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi conectado");

    // Iniciacion NTP y obtiene la fecha y hora
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();

    startCameraServer();

    Serial.print("Camara Lista! Usar 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' para conectar");

    // Conexion al WebSocket
    Serial.println("Conectando al servidor.");
    bool connected =
        client.connect(websockets_server_host, websockets_server_port, "/");
    if (connected) {
        Serial.println("Conectado!");
        client.send("Hola servidor");
    } else {
        Serial.println("No conectado!");
    }

    tiempoJson = millis();
    tiempoPoll = millis();
    tiempoAlarma = millis();

    // Recibe mensaje del servidor
    client.onMessage([&](WebsocketsMessage message) {
        Serial.print("Mensaje recibido: ");
        jsonRecibido = message.data();
        Serial.println(jsonRecibido);
    });
}

void loop() {
    if ((sensorPIR == 1) &&
        (alarma ==
         0)) {  // Se utiliza la bandera alarma porque el sensorPIR envia un
                // pulso con una duracion mayor al ciclo loop()
        if (modo ==
            "nocturno") {  // Si esta en modo nocturno se activa la alarma
            delay(500);    // Evita perdida de frames en el video
            digitalWrite(Relay_GPIO_NUM, HIGH);
            alarma = 1;
            tiempoAlarma = millis();
        }
        if (millis() - tiempoJson >=
            2000) {  // Evita que se envie el JSON multiples veces
            tiempoJson = millis();
            serializarObjetoPIR();
            client.send(json);
        }
    }

    if (alarma) {  // Si la alarma esta activada se enciende el LED intermitente
        pulsoLEDalarma();
    }

    if (((alarma == 1) && (millis() - tiempoAlarma >= 10000) ||
         (apagarAlarma ==
          1))) {  // Luego de 10 segundos o si se recibe el json apagarAlarma se
                  // desactiva la alarma y reinician las banderas
        tiempoAlarma = millis();
        sensorPIR = 0;
        alarma = 0;
        apagarAlarma = 0;
        analogWrite(Flash_GPIO_NUM, 0);
        digitalWrite(Relay_GPIO_NUM, LOW);
    }

    if (millis() - tiempoNTP >=
        900000000) {  // cada 15 min toma la hora del servidor NTP y actualiza
                      // el modo
        tiempoNTP = millis();
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            int currentHour = timeinfo.tm_hour;
            if ((currentHour >= 0 && currentHour < 6) &&
                (modo == "vigilancia")) {
                modo = "nocturno";
                serializarObjetoModo();
                client.send(json);
            } else if (modo == "nocturno") {
                modo = "vigilancia";
                serializarObjetoModo();
                client.send(json);
            }
            Serial.println("modo actualizado  mediante NTP: " + modo);
            pulsoLEDmodo();
        } else {
            Serial.println("Fallo al obtener la hora");
        }
    }

    if (client.available() && (millis() - tiempoPoll >=
                               3000)) {  // Poll del WebSocket cada 3 segundos
                                         // para no consumir recursos
        tiempoPoll = millis();
        client.poll();
        if (jsonRecibido !=
            "0") {  // Deserializa el objeto solo si el WebSocket recibe algo
            deserializarObjeto();
        }
    }
}
