#define ARDUINO_MAIN
#include "avr-nrf24l01/src/nrf24l01.h"
#include "bootloader/bootloader.h"
#include "eeprom/eeprom.h"
#include "input/input_handler.h"
#include "leds/leds.h"
#include "output/reports.h"
#include "output/serial_commands.h"
#include "output/serial_handler.h"
#include "pins/pins.h"
#include "rf/rf.h"
#include "timer/timer.h"
#include "util/util.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>
#include <avr/sfr_defs.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h>

#include "Descriptors.h"
#include "controller/guitar_includes.h"
#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Platform/Platform.h>
#define ARDUINO_MAIN
#include "pins_arduino.h"
/** LUFA CDC Class driver interface configuration and state information. This
 * structure is passed to all CDC Class driver functions, so that multiple
 * instances of the same class within a device can be differentiated from one
 * another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface = {
    .Config =
        {
            .ControlInterfaceNumber = INTERFACE_ID_CDC_CCI,
            .DataINEndpoint =
                {
                    .Address = CDC_TX_EPADDR,
                    .Size = CDC_TXRX_EPSIZE,
                    .Banks = 1,
                },
            .DataOUTEndpoint =
                {
                    .Address = CDC_RX_EPADDR,
                    .Size = CDC_TXRX_EPSIZE,
                    .Banks = 1,
                },
            .NotificationEndpoint =
                {
                    .Address = CDC_NOTIFICATION_EPADDR,
                    .Size = CDC_NOTIFICATION_EPSIZE,
                    .Banks = 1,
                },
        },
};
// Sleep pin: 7
__attribute__((section(".rfrecv"))) uint32_t rftxID = 0xDEADBEEF;
__attribute__((section(".rfrecv"))) uint32_t rfrxID = 0xDEADBEEF;
Controller_t controller;
Controller_t prevCtrl;
bool isRF = false;
bool typeIsGuitar;
bool typeIsDrum;
bool typeIsDJ;
long lastPoll = 0;
uint8_t inputType;
uint8_t deviceType;
uint8_t fullDeviceType;
void initialise(void) {
  Configuration_t config;
  loadConfig(&config);
  config.rf.rfInEnabled = false;
  fullDeviceType = config.main.subType;
  deviceType = fullDeviceType;
  inputType = config.main.inputType;
  typeIsDrum = isDrum(fullDeviceType);
  typeIsGuitar = isGuitar(fullDeviceType);
  typeIsDJ = isDJ(fullDeviceType);
  if (typeIsGuitar && deviceType <= XINPUT_TURNTABLE) {
    deviceType = REAL_GUITAR_SUBTYPE;
  }
  if (typeIsDrum && deviceType <= XINPUT_TURNTABLE) {
    deviceType = REAL_DRUM_SUBTYPE;
  }
  setupMicrosTimer();
  if (config.rf.rfInEnabled) {
    initRF(false, config.rf.id, generate_crc32());
    isRF = true;
  } else {
    initInputs(&config);
  }
  initReports(&config);
  initLEDs(&config);
  USB_Init();
  sei();
}
int main(void) {
  initialise();
  initRF(true, pgm_read_dword(&rftxID), pgm_read_dword(&rfrxID));
  long lastChange = millis();
  long lastButtons = 0;
  while (true) {
    if (millis() - lastChange > 600000) {
      lastChange = millis();
      //
      // disable ADC
      ADCSRA = 0;
      // Turn off RF
      nrf24_powerDown();
      // turn off various modules
      power_all_disable();

      set_sleep_mode(SLEEP_MODE_PWR_DOWN);
      cli(); // timed sequence follows
      EICRA |= _BV(ISC61);
      EIMSK |= _BV(INT6);
      EIFR = _BV(INTF6);
      sleep_enable();
      sei();       // guarantees next instruction executed
      sleep_cpu(); // sleep within 3 clock cycles of above
    }
    if (tickInputs(&controller) || millis() - lastPoll > 100) {
      tickLEDs(&controller);
      // Since we receive data via acks, we need to make sure data is always
      // being sent, so we send data every 100ms regardless.
      if (memcmp(&controller, &prevCtrl, sizeof(Controller_t)) != 0 ||
          millis() - lastPoll > 100) {
        lastPoll = millis();

        uint8_t data[12];
        if (tickRFTX((uint8_t *)&controller, data, sizeof(XInput_Data_t))) {
          uint8_t cmd = data[0];
          bool isRead = data[1];
          if (isRead) {
            processHIDReadFeatureReport(cmd, 0, NULL);
          } else {
            if (cmd == COMMAND_WRITE_CONFIG) {
              // 2 bytes for rf header
              processHIDWriteFeatureReport(cmd, 30, data + 2);
            } else {
              handleCommand(cmd);
            }
          }
        }
        memcpy(&prevCtrl, &controller, sizeof(Controller_t));
        if (lastButtons != controller.buttons) {
          lastButtons = controller.buttons;
          lastChange = millis();
        }
      }
    }
    USB_USBTask();
  }
}
void EVENT_USB_Device_ControlRequest(void) {
  CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}
void EVENT_CDC_Device_ControLineStateChanged(
    USB_ClassInfo_CDC_Device_t *const CDCInterfaceInfo) {
  /* You can get changes to the virtual CDC lines in this callback; a common
     use-case is to use the Data Terminal Ready (DTR) flag to enable and
     disable CDC communications in your application when set to avoid the
     application blocking while waiting for a host to become ready and read
     in the pending data from the USB endpoints.
  */
  bool HostReady = (CDCInterfaceInfo->State.ControlLineStates.HostToDevice &
                    CDC_CONTROL_LINE_OUT_DTR) != 0;

  (void)HostReady;
}
void EVENT_CDC_Device_LineEncodingChanged(
    USB_ClassInfo_CDC_Device_t *const CDCInterfaceInfo) {
  if (CDCInterfaceInfo->State.LineEncoding.BaudRateBPS == 1200) {
    bootloader();
  }
}

void writeToUSB(const void *const Buffer, uint8_t Length, uint8_t report, const void* request) {
  uint8_t data[32];
  tickRFTX((uint8_t *)Buffer, data, Length);
}
ISR(INT6_vect) {
  sleep_disable();
  power_all_enable();
  setupADC();
  initRF(true, pgm_read_dword(&rftxID), pgm_read_dword(&rfrxID));
  EICRA &= ~(_BV(ISC61));
  EIMSK &= ~(_BV(INT6));
}