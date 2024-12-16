#include "freertos/FreeRTOS.h"
#include <WiFi.h>
#include <AsyncTCP.h>          // https://github.com/me-no-dev/AsyncTCP
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#include <AsyncElegantOTA.h>   // https://github.com/ayushsharma82/AsyncElegantOTA
#include "network.h"
#include "ilda.h"

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void handleStream(uint8_t *data, size_t len, int index, int totalLen);

bool isStreaming = false;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

uint8_t chunkTemp[64];
int tempLen = 0;
// static const char *NETWORKTAG = "network";

void setupWifi()
{
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin("TEST", "12345678");
  Serial.print("Connecting to WiFi ");
  unsigned long wifiStartTime = millis();
  const unsigned long wifiTimeout = 10000;   // 超时时间设为 10 秒

  while (WiFi.status() != WL_CONNECTED && millis() - wifiStartTime < wifiTimeout) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Connect to WiFi timeout");
  }
}

void setupWebServer()
{
  if (WiFi.status() == WL_CONNECTED) {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->redirect("http://localhost:3000/?ip=" + WiFi.localIP().toString()); });
    AsyncElegantOTA.begin(&server); // Start ElegantOTA
    ws.onEvent(onWsEvent); // attach AsyncWebSocket
    server.addHandler(&ws);
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    server.begin();
  } else {
    Serial.println("Failed to start Web Server");
  }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    // client connected
    // ESP_LOGI(NETWORKTAG, "ws[%s][%u] connect\n", server->url(), client->id());
    // client->printf("I am bbLaser :)", client->id());
    // client->ping();
    isStreaming = true;
    ilda->resetFrames();
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    // client disconnecteds
    // ESP_LOGI(NETWORKTAG, "ws[%s][%u] disconnect: %u\n", server->url(), client->id());
    isStreaming = false;
    ilda->resetFrames();
    nextMedia(1);
  }
  else if (type == WS_EVT_DATA)
  {
    handleWebSocketMessage(arg, data, len);
  }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  // 单帧
  if (info->final && info->index == 0 && info->len == len)
  {
    handleStream(data, len, 0, info->len);
  }
  // 多帧
  else
  {
    if (info->index == 0)
    {
      // if (info->num == 0)
      // Serial.println("MSG Start");
      // Serial.println("Frame Start");
      // handleStream(data, len, 0, info->len);
    }
    // Serial.print(info->index);
    // Serial.print(" ");
    // Serial.println(len);
    if ((info->index + len) == info->len)
    {
      // Serial.println("Frame End");
      if (info->final)
      {
        // Serial.println("MSG End");
        // Serial.println(frameLen);
        handleStream(data, len, info->index, info->len);
      }
    }
    else
    {
      handleStream(data, len, info->index, info->len);
    }
  }
}

void handleStream(uint8_t *data, size_t len, int index, int totalLen)
{
  // Serial.println("Stream");
  int newtempLen = (tempLen + len) % recordLen;
  // Serial.print("newTemp:");
  // Serial.println(newtempLen);
  if (tempLen > 0)
  {
    // memcpy(chunkTemp+tempLen, data, len - newtempLen);
    uint8_t concatData[len - newtempLen + tempLen];
    memcpy(concatData, chunkTemp, tempLen);
    memcpy(concatData + tempLen, data, len - newtempLen); // copy the address
    // Serial.print("Temp Concat Len: ");
    // Serial.println(len-newtempLen+tempLen);
    ilda->parseStream(concatData, len - newtempLen + tempLen, index - tempLen, totalLen);
  }
  else
  {
    // Serial.print("No Concat Len: ");
    // Serial.println(len-newtempLen+tempLen);
    ilda->parseStream(data, len - newtempLen, index, totalLen);
  }
  for (size_t i = 0; i < newtempLen; i++)
  {
    chunkTemp[i] = data[len - newtempLen + i];
  }
  tempLen = newtempLen;
}
