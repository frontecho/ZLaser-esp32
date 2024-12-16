#include <Arduino.h>
#include <FS.h>
#include <WiFi.h>
#include "driver/timer.h"

#include "ilda.h"
#include "SDCard.h"
#include "network.h"
#include "button.h"
#include "SPIRenderer.h"

ILDAFile *ilda = new ILDAFile();
const int recordLen = 6;
const int bufferFrames = 3;

File file;
ILDA_Header_t header;
const int maxRecords = 3000;
int curMedia = -1;
int loadedLen = 0;
unsigned long timeDog = 0;

static const char *ILDATAG = "ilda";

void setupILDA()
{
  Serial.print("RAM Before:");
  Serial.println(ESP.getFreeHeap());
  ilda->frames = (ILDA_Frame_t *)malloc(sizeof(ILDA_Frame_t) * bufferFrames);
  for (int i = 0; i < bufferFrames; i++)
  {
    ilda->frames[i].records = (ILDA_Record_t *)malloc(sizeof(ILDA_Record_t) * maxRecords);
  }
  Serial.print("RAM After:");
  Serial.println(ESP.getFreeHeap());
  nextMedia(1);
}

void nextMedia(int position)
{
  if (position == 1)
  {
    curMedia++;
  }
  else if (position == -1)
  {
    curMedia--;
  }
  else
  {
    curMedia = curMedia + position;
  }
  if (curMedia >= avaliableMedia.size())
    curMedia = 0;
  if (curMedia < 0)
    curMedia = avaliableMedia.size() - 1;
  String filePath = String("/bbLaser/") += avaliableMedia[curMedia].as<String>();
  ilda->cur_frame = 0;
  ilda->read(SD, filePath.c_str());
}

void fileBufferLoop(void *pvParameters)
{
  for (;;)
  {
    if (millis() - timeDog > 1000)
    {
      timeDog = millis();
      TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;
      TIMERG0.wdt_feed = 1;
      TIMERG0.wdt_wprotect = 0;
    }
    if (!isStreaming)
    {
      if (buttonState == 1)
      {
        nextMedia(-1);
        buttonState = 0;
      }
      else if (buttonState == 2)
      {
        nextMedia(1);
        buttonState = 0;
      }
      if (!ilda->tickNextFrame())
      {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      }
    }
  }
}


ILDAFile::ILDAFile()
{
  frames = NULL;
  file_frames = 0;
  cur_frame = 0;
  cur_buffer = 0;
}

ILDAFile::~ILDAFile()
{
  free(frames);
}

void ILDAFile::resetFrames()
{
  ilda->file_frames = 0;
  ilda->cur_frame = 0;
  ilda->cur_buffer = 0;
  for (int i = 0; i < bufferFrames; i++)
  {
    ilda->frames[i].number_records = 0;
    ilda->frames[i].isBuffered = false;
  }
}

void ILDAFile::dump_header(const ILDA_Header_t &header)
{
  char tmp[100];
  strncpy(tmp, header.ilda, 4);
  tmp[5] = '\0';
  ESP_LOGI(ILDATAG, "Header: %s", tmp);
  ESP_LOGI(ILDATAG, "Format Code: %d", header.format);
  strncpy(tmp, header.frame_name, 8);
  tmp[8] = '\0';
  ESP_LOGI(ILDATAG, "Frame Name: %s", tmp);
  strncpy(tmp, header.company_name, 8);
  tmp[8] = '\0';
  ESP_LOGI(ILDATAG, "Company Name: %s", tmp);
  ESP_LOGI(ILDATAG, "Number records: %d", header.records);
  ESP_LOGI(ILDATAG, "Number frames: %d", header.total_frames);
}

bool ILDAFile::read(fs::FS &fs, const char *fname)
{
  file = fs.open(fname);
  if (!file)
  {
    return false;
  }
  file.read((uint8_t *)&header, sizeof(ILDA_Header_t));
  header.records = ntohs(header.records);
  header.total_frames = ntohs(header.total_frames);
  dump_header(header);
  file_frames = header.total_frames;
  // Serial.println(file_frames);
  return true;
}

bool ILDAFile::tickNextFrame()
{
  if (frames[cur_buffer].isBuffered == false)
  {
    // frames[cur_buffer].isBuffered = true;  // 注释该行启用覆盖模式
    frames[cur_buffer].number_records = header.records;
    // frames[cur_buffer].records = (ILDA_Record_t *)malloc(sizeof(ILDA_Record_t) * header.records);
    ILDA_Record_t *records = frames[cur_buffer].records;
    for (int i = 0; i < header.records; i++)
    {
      file.read((uint8_t *)(records + i), sizeof(ILDA_Record_t));
      records[i].x = ntohs(records[i].x);
      records[i].y = ntohs(records[i].y);
      records[i].z = ntohs(records[i].z);
    }
    // read the next header
    file.read((uint8_t *)&header, sizeof(ILDA_Header_t));
    header.records = ntohs(header.records);
    header.total_frames = ntohs(header.total_frames);

    cur_buffer++;
    if (cur_buffer > bufferFrames - 1)
      cur_buffer = 0;

    cur_frame++;
    // Serial.println(cur_frame);
    // Serial.println(file_frames);
    if (cur_frame > file_frames - 1)
    {
      cur_frame = 0;
      // Serial.println("One media has been successfully displayed.\n");
      if (digitalRead(4) == HIGH) // Happy按钮，自动下一个
        nextMedia(1);
      else
        nextMedia(0);
    }
    return true;
  }
  else
    return false; // 该帧已缓存且未Render，可能是读文件、串流太快了？忽视掉就好 0w0
}

bool ILDAFile::parseStream(uint8_t *data, size_t len, int frameIndex, int totalLen)
{
  if (frames[cur_buffer].isBuffered == false)
  {
    // frames[cur_buffer].isBuffered = true; // 注释该行启用覆盖模式
    frames[cur_buffer].number_records = totalLen / recordLen;
    ILDA_Record_t *records = frames[cur_buffer].records;

    /*
     Serial.print("Len: ");
     Serial.println(len);
     Serial.print("Get Frame: ");

     Serial.print(frameIndex);
     Serial.print(" / ");
     Serial.print(totalLen);
     Serial.print("  ");
     Serial.println(cur_buffer);
     */

    for (size_t i = 0; i < len / recordLen; i++)
    {
      int16_t x = (data[i * recordLen] << 8) | data[i * recordLen + 1];
      int16_t y = (data[i * recordLen + 2] << 8) | data[i * recordLen + 3];

      /*
      Serial.print(frameIndex/recordLen + i);
      Serial.print(",");
      Serial.print(x);
      Serial.print(",");
      Serial.println(y);
      */
      /*
      Serial.print(",");
      Serial.print(data[i*recordLen+4]);
      Serial.print(",");
      Serial.println(data[i*recordLen+5]);
      Serial.println((data[i*recordLen+5] & 0b01000000) == 0);*/

      records[frameIndex / recordLen + i].x = x;
      records[frameIndex / recordLen + i].y = y;
      records[frameIndex / recordLen + i].z = 0;
      records[frameIndex / recordLen + i].color = data[i * recordLen + 4];
      records[frameIndex / recordLen + i].status_code = data[i * recordLen + 5];
    }
    loadedLen += len;

    if (loadedLen >= totalLen)
    {
      // Serial.println("Frame End");
      loadedLen = 0;
      cur_buffer++;
      if (cur_buffer > bufferFrames - 1)
        cur_buffer = 0;

      cur_frame++;
      // Serial.println(cur_frame);
      if (cur_frame > (1 << 15 - 1))
      {
        cur_frame = 0;
      }
    }

    return true;
  }
  else
    return false; // 该帧已缓存且未Render，可能是读文件、串流太快了？忽视掉就好 0w0
}
