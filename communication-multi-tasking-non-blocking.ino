#include <WiFi.h>
#include <PubSubClient.h>

// shared variable
TaskHandle_t t0;
TaskHandle_t t1;

const char* ssid = "192.168.1.1";             // my ssid is 192.168.1.1
const char* password = "aaasssdd";
const char* mqtt_server = "203.150.107.212";

#define mqtt_port 1883

WiFiClient espClient;
//PubSubClient client(espClient);
PubSubClient *client = NULL; // pointer

long lastMsg = 0;
char msg[50];
int value = 0;

long lastReconnectAttempt = 0;
String STATE = "None";

void setup_wifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

boolean reconnect() {
  if (client->connect("arduinoClient")) {
    Serial.println("MQTT connected");
    // Once connected, publish an announcement...
    client->publish("device/outTopic", "hello world");
    // ... and resubscribe
    client->subscribe("device/inTopic");
  }
  return client->connected();
}


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);

  // create a task to handle led LED_BUILTIN (core 0)
  xTaskCreatePinnedToCore(
    tLedAndSerialFunc,  /* Task function. */
    "LedAndSerial",     /* name of task. */
    10000,              /* Stack size of task */
    NULL,               /* parameter of the task */
    1,                  /* priority of the task */
    &t1,                /* Task handle to keep track of created task */
    0);                 /* pin task to core 0 */
  delay(500);


  //create a task to handle Network Connection (WiFi&MQTT) (core 1)
  xTaskCreatePinnedToCore(
    tNetworkFunc,       /* Task function. */
    "Network",          /* name of task. */
    10000,              /* Stack size of task */
    NULL,               /* parameter of the task */
    1,                  /* priority of the task */
    &t0,                /* Task handle to keep track of created task */
    1);                 /* pin task to core 0 */
  delay(500);
}

void loop() {
  // no coding here
}
// tNetworkFunc: check WiFi/MQTT Connection for releae events
void tNetworkFunc(void *params) {
  // setup
  Serial.print("tNetworkFunc running on core ");
  Serial.println(xPortGetCoreID());
  setup_wifi();


  // loop
  while (true) {
    if (WiFi.status() != WL_CONNECTED) { 
      STATE = "None";
    } else if (WiFi.status() == WL_CONNECTED && STATE == "None") {\
      // setup MQTT when WiFi connected
      Serial.println("WiFi connected");
      STATE = "WiFi-Connect";
      client = new PubSubClient(espClient);
      client->setServer(mqtt_server, mqtt_port);
      client->setCallback(callback);
    } else if (WiFi.status() != WL_CONNECTED && STATE == "MQTT-Connect") {
      // wifi disconnect while STATE is "MQTT-Connect", Back to STATE "None" and delete client
      delete client;
      STATE = "None";
    } else { // when wifi connected and STATE is "WiFi connected" or "MQTT-Connect"
      if (!client->connected()) {
        long now = millis();
        if (now - lastReconnectAttempt > 5000) {
          lastReconnectAttempt = now;
          // Attempt to reconnect
          if (reconnect()) {
            lastReconnectAttempt = 0;
          }
        }
        STATE = "WiFi-Connect";
      } else {
        // Client connecte
        STATE = "MQTT-Connect";
        client->loop();

        long now = millis();
        if (now - lastMsg > 2000) {
          lastMsg = now;
          ++value;
          snprintf (msg, 50, "hello world #%ld", value);
          Serial.print("Publish message: ");
          Serial.println(msg);
          client->publish("device/outTopic", msg);
        }
      }
    }
    delay(10);
  }
}

// Led: blinks every 500ms
// Serialprint: print every 1000ms
void tLedAndSerialFunc(void *params) {
  // setup
  Serial.print("tLedAndSerialFunc running on core ");
  Serial.println(xPortGetCoreID());
  unsigned int Counter = 0;
  Serial.println((String)"Counter is reset(" + Counter + ")");
  uint32_t _nextSerial = millis() + 1000;
  uint32_t _nextLED = millis() + 500;

  // loop
  while (true) {
    uint32_t cur = millis();
    if (cur >= _nextSerial) {
      Serial.println((String)"Counter is changed to " + Counter++);
      _nextSerial = cur + 1000;
    }

    if (cur >= _nextLED) {
      if (STATE == "WiFi-Connect") {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      } else if (STATE == "MQTT-Connect") {
        digitalWrite(LED_BUILTIN, LOW);
      } else if (STATE == "None") {
        digitalWrite(LED_BUILTIN, HIGH); // turn off led when STATE = None
      }
      _nextLED = cur + 500;
    }
    delay(10);
  }
}
