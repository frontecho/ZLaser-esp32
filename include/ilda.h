#ifndef __ILDA_H
#define __ILDA_H

#include <Arduino.h>
#include <FS.h>

void setupILDA();
void nextMedia(int position);
void fileBufferLoop(void *pvParameters);

typedef struct
{
  char ilda[4];
  uint8_t reserved1[3];
  uint8_t format;
  char frame_name[8];
  char company_name[8];
  volatile uint16_t records;
  uint16_t frame_number;
  uint16_t total_frames;
  uint8_t projector_number;
  uint8_t reserved2;
} ILDA_Header_t;

typedef struct
{
  volatile int16_t x;
  volatile int16_t y;
  volatile int16_t z;
  volatile uint8_t status_code;
  volatile uint8_t color;
} ILDA_Record_t;

typedef struct
{
  ILDA_Record_t *records;
  uint16_t number_records;
  bool isBuffered = false;
} ILDA_Frame_t;

class ILDAFile
{
private:
  void dump_header(const ILDA_Header_t &header);

public:
  ILDAFile();
  ~ILDAFile();
  void resetFrames();
  bool read(fs::FS &fs, const char *fname);
  bool tickNextFrame();
  bool parseStream(uint8_t *data, size_t len, int index, int totalLen);
  ILDA_Frame_t *frames;
  volatile int file_frames;
  volatile int cur_frame;
  volatile int cur_buffer;
};

extern ILDAFile *ilda;
extern const int recordLen;
extern const int bufferFrames;

#endif