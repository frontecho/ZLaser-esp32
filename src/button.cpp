#include "Button2.h"
#include "button.h"

void goPrev();
void goNext();

Button2 buttonL, buttonR;
// 让Core0 和 Core1的操作不要同时出现，不然就读着读着跳下一个文件就Crash了
// 无操作 -1   上一个 1  下一个 2  自动下一个 3  不要自动 4
int buttonState = 0;

void setupButton()
{
  buttonL.begin(21, INPUT_PULLUP, false);
  buttonL.setTapHandler(click);

  buttonR.begin(22, INPUT_PULLUP, false);
  buttonR.setTapHandler(click);
}

void click(Button2 &btn)
{
  if (btn == buttonL)
  {
    goPrev();
  }
  else if (btn == buttonR)
  {
    goNext();
  }
}

void goNext()
{
  buttonState = 2;
}

void goPrev()
{
  buttonState = 1;
}
