#include "./Direct.h"
void Direct::read_controller(Controller *controller) {
#if DEVICE_TYPE == DIRECT
  bool fret_inv = -1;
  #if FRETS_LED == 1 
    fret_inv = 1;
  #endif
  bit_write(fret_inv * IO::digitalRead(PIN_GREEN), controller->buttons, GREEN);
  bit_write(fret_inv * IO::digitalRead(PIN_RED), controller->buttons, RED);
  bit_write(fret_inv * IO::digitalRead(PIN_YELLOW), controller->buttons, YELLOW);
  bit_write(fret_inv * IO::digitalRead(PIN_BLUE), controller->buttons, BLUE);
  bit_write(fret_inv * IO::digitalRead(PIN_ORANGE), controller->buttons, ORANGE);
  bit_write(!IO::digitalRead(PIN_START), controller->buttons, START);
  bit_write(!IO::digitalRead(PIN_SELECT), controller->buttons, SELECT);
  controller->r_x = (IO::analogRead(PIN_WHAMMY) * 32) * WHAMMY_DIR + WHAMMY_START;
#if MOVE_MODE == JOY
  bit_write(!IO::digitalRead(PIN_UP) || IO::analogRead(PIN_JOY_Y) > 600,
            controller->buttons, UP);
  bit_write(!IO::digitalRead(PIN_DOWN) || IO::analogRead(PIN_JOY_Y) < 400,
            controller->buttons, DOWN);
  bit_write(IO::analogRead(PIN_JOY_X) < 400, controller->buttons, LEFT);
  bit_write(IO::analogRead(PIN_JOY_X) > 600, controller->buttons, RIGHT);
#elif MOVE_MODE == DPAD
  bit_write(!IO::digitalRead(PIN_UP), controller->buttons, UP);
  bit_write(!IO::digitalRead(PIN_DOWN), controller->buttons, DOWN);
  bit_write(!IO::digitalRead(PIN_LEFT), controller->buttons, LEFT);
  bit_write(!IO::digitalRead(PIN_RIGHT), controller->buttons, RIGHT);
#endif
#endif
}

void Direct::init() {
#if DEVICE_TYPE == DIRECT
  int fret_type = INPUT_PULLUP;
  #if FRETS_LED == 1 
    fret_type = INPUT;
  #endif
  IO::pinMode(PIN_GREEN, fret_type);
  IO::pinMode(PIN_RED, fret_type);
  IO::pinMode(PIN_YELLOW, fret_type);
  IO::pinMode(PIN_BLUE, fret_type);
  IO::pinMode(PIN_ORANGE, fret_type);
  IO::pinMode(PIN_START, INPUT_PULLUP);
  IO::pinMode(PIN_SELECT, INPUT_PULLUP);
  IO::pinMode(PIN_UP, INPUT_PULLUP);
  IO::pinMode(PIN_DOWN, INPUT_PULLUP);
  IO::pinMode(PIN_WHAMMY, INPUT);
#if MOVE_MODE == JOY
  IO::pinMode(PIN_JOY_X, INPUT);
  IO::pinMode(PIN_JOY_Y, INPUT);
#elif MOVE_MODE == DPAD
  IO::pinMode(PIN_LEFT, INPUT_PULLUP);
  IO::pinMode(PIN_RIGHT, INPUT_PULLUP);
#endif
#endif
}