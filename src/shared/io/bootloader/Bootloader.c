#include "Bootloader.h"
#include "../../util.h"
#include <avr/interrupt.h>
#include <avr/wdt.h>
#ifdef __AVR_ATmega32U4__
#define MAGIC_KEY_POS 0x0800
#else
#define MAGIC_KEY_POS (RAMEND - 1)
#endif
#define MAGIC_KEY 0x7777
volatile uint16_t *const bootKeyPtr = (volatile uint16_t *)MAGIC_KEY_POS;
volatile uint16_t *const bootKeyPtrCDC =
    (volatile uint16_t *)(MAGIC_KEY_POS - 2);
uint16_t bootKeyVal;
int i = 1;
void bootloader(void) {
  // close interrupts
  cli();

  // write magic key to ram
  *bootKeyPtr = MAGIC_KEY;

  // watchdog reset
  wdt_enable(WDTO_15MS);
  for (;;) {
  }
}
// Jump to bootloader if F_CPU is incorrect.
void check_freq(void) {
  bootKeyVal = *bootKeyPtrCDC;
  *bootKeyPtrCDC = 0;
  cli();
  wdt_enable(WDTO_15MS);
  _WD_CONTROL_REG = (1 << WDIE) | (1 << WDP2) | (1 << WDP0);
  sei();
  _delay_ms(60);
  // 60 / 15 = 4, since the watchdog timer does not rely on F_CPU, both the
  // delay and the timer should be the same
  if (i != 4) {
    bootloader();
  }
}

void serial(void) {
  // close interrupts
  cli();

  // write magic key to ram
  *bootKeyPtrCDC = MAGIC_KEY;

  // watchdog reset
  wdt_enable(WDTO_15MS);
  for (;;) {
  }
}

bool check_serial() {
  return bootKeyVal == MAGIC_KEY;
}

ISR(WDT_vect) {
  _WD_CONTROL_REG |= (1 << WDIE);
  i++;
  if (i > 4 && !check_serial()) {
    bootloader();
  }
}