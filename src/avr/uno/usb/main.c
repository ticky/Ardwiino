#include "LightweightRingBuff.h"
#include "bootloader/bootloader.h"
#include "config/config.h"
#include "config/defaults.h"
#include "config/defines.h"
#include "device_comms.h"
#include "output/control_requests.h"
#include "output/controller_structs.h"
#include "output/serial_commands.h"
#include "usb/usb.h"
#include "util/util.h"
#include <LUFA/Drivers/Board/Board.h>
#include <LUFA/Drivers/Board/LEDs.h>
#include <LUFA/Drivers/Peripheral/Serial.h>
#include <LUFA/Drivers/USB/Class/CDCClass.h>
#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Version.h>
#include <avr/wdt.h>
#define JUMP 0xDEAD8001

const uint8_t endpoints[] PROGMEM = {[REPORT_ID_CONTROL] = ENDPOINT_CONTROLEP,
                                     [REPORT_ID_XINPUT] = XINPUT_EPADDR_IN,
                                     [REPORT_ID_GAMEPAD] = HID_EPADDR_IN,
                                     [REPORT_ID_KBD] = HID_EPADDR_IN,
                                     [REPORT_ID_MOUSE] = HID_EPADDR_IN,
                                     [REPORT_ID_MIDI] = MIDI_EPADDR_IN};
typedef struct {
  uint32_t id;
  uint8_t deviceType;
} EepromConfig_t;

EepromConfig_t EEMEM config;
uint8_t defaultConfig[] = {0xa2, 0xd4, 0x15, 0x00, DEVICE_TYPE};

// if jmpToBootloader is set to JUMP, then the arduino will jump to bootloader
// mode after the next watchdog reset
uint32_t jmpToBootloader __attribute__((section(".noinit")));
uint16_t ticks;
unsigned long micros() {
  return ticks;
}
int main(void) {
  // jump to the bootloader at address 0x1000 if jmpToBootloader is set to JUMP
  if (jmpToBootloader == JUMP) {
    // We don't want to jump again after the bootloader returns control flow to
    // us
    jmpToBootloader = 0;
    asm volatile("jmp 0x1000");
  }

  /* Disable watchdog if enabled by bootloader/fuses */
  MCUSR &= ~(1 << WDRF);
  wdt_disable();

  Serial_InitInterrupt(BAUD, true);

  USB_Init();

  // Read the device type from eeprom. ARDWIINO_DEVICE_TYPE is used as a
  // signature to make sure that the data in eeprom is valid
  if (eeprom_read_dword(&config.id) != ARDWIINO_DEVICE_TYPE) {
    eeprom_update_dword(&config.id, ARDWIINO_DEVICE_TYPE);
    eeprom_update_byte(&config.deviceType, deviceType);
  } else {
    deviceType = eeprom_read_byte(&config.deviceType);
  }

  sei();
  uint8_t packetCount = 0;
  uint8_t state = 0;
  // Once the endpoint is ready, let the 328p know so it can send data
  uint8_t currentEndpoint = XINPUT_EPADDR_IN;
  if (deviceType >= MIDI_GAMEPAD) {
    currentEndpoint = MIDI_EPADDR_IN;
  } else if (deviceType >= KEYBOARD_GAMEPAD) {
    currentEndpoint = HID_EPADDR_IN;
  }
  bool checkEndpoint = true;
  AVR_RESET_LINE_DDR |= AVR_RESET_LINE_MASK;
  AVR_RESET_LINE_PORT |= AVR_RESET_LINE_MASK;
  while (true) {
    ticks++;
    //================================================================================
    // USARTtoUSB
    //================================================================================

    // This requires the USART RX buffer to be 256 bytes.
    uint8_t count = USARTtoUSB_WritePtr - USARTtoUSB_ReadPtr;

    // Check if we have something worth to send
    if (count) {

      // Prepare temporary pointer
      uint16_t tmp; // = 0x100 | USARTtoUSBReadPtr
      asm(
          // Do not initialize high byte, it will be done in first loop
          // below.
          "lds %A[tmp], %[readPtr]\n\t" // (1) Copy read pointer into
                                        // lower byte
          // Outputs
          : [tmp] "=&e"(tmp) // Pointer register, output only
          // Inputs
          : [readPtr] "m"(USARTtoUSB_ReadPtr) // Memory location
      );

      // Write all bytes from USART to the USB endpoint
      do {
        register uint8_t data;
        asm("ldi %B[tmp] , 0x01\n\t"     // (1) Force high byte to 0x01
            "ld %[data] , %a[tmp] +\n\t" // (2) Load next data byte, wraps
                                         // around 255
            // Outputs
            : [data] "=&r"(data), // Output only
              [tmp] "=e"(tmp)     // Input and output
            // Inputs
            : "1"(tmp));
        if (state == 0 && data == FRAME_START_WRITE) {
          state = 1;
        } else if (state == 1) {
          packetCount = data;
          state = 2;
        } else if (state == 2) {
          state = 3;
          packetCount--;
          currentEndpoint = pgm_read_byte(endpoints + data);
          Endpoint_SelectEndpoint(currentEndpoint);
          if (data == REPORT_ID_MIDI || data == REPORT_ID_GAMEPAD ||
              data == REPORT_ID_CONTROL) {
            continue;
          }
          Endpoint_Write_8(data);
        } else if (state == 3) {
          packetCount--;
          Endpoint_Write_8(data);
          if (packetCount == 0) {
            Endpoint_ClearIN();
            state = 0;
            if (currentEndpoint) { checkEndpoint = true; }
            break;
          }
        }
      } while (--count);
      // Save new pointer position
      USARTtoUSB_ReadPtr = tmp & 0xFF;
    } else {
      USB_USBTask();
      if (checkEndpoint) {
        uint8_t prev = Endpoint_GetCurrentEndpoint();
        Endpoint_SelectEndpoint(currentEndpoint);
        if (Endpoint_IsINReady()) {
          uint8_t done = FRAME_DONE;
          writeData(&done, 1);
          checkEndpoint = false;
        }
        Endpoint_SelectEndpoint(prev);
      }
    }
  }
}
void EVENT_USB_Device_ConfigurationChanged(void) {
  // Setup necessary endpoints
  Endpoint_ConfigureEndpoint(XINPUT_EPADDR_IN, EP_TYPE_INTERRUPT, HID_EPSIZE,
                             1);
  Endpoint_ConfigureEndpoint(HID_EPADDR_IN, EP_TYPE_INTERRUPT, HID_EPSIZE, 1);
  Endpoint_ConfigureEndpoint(MIDI_EPADDR_IN, EP_TYPE_INTERRUPT, HID_EPSIZE, 1);
}
void processHIDWriteFeatureReportControl(uint8_t cmd, uint8_t len) {
  Endpoint_ClearSETUP();
  uint8_t header = FRAME_START_FEATURE_WRITE;
  writeData(&header, 1);
  writeData(&len, 1);
  writeData(&cmd, 1);
  uint8_t d;
  while (len) {
    if (Endpoint_IsOUTReceived()) {
      while (len && Endpoint_BytesInEndpoint()) {
        d = Endpoint_Read_8();
        writeData(&d, 1);
        len--;
      }
      Endpoint_ClearOUT();
    }
  }

  if (cmd == COMMAND_WRITE_SUBTYPE) {
    eeprom_update_byte(&config.deviceType, d);
  }
  if (cmd == COMMAND_REBOOT || cmd == COMMAND_JUMP_BOOTLOADER) {
    jmpToBootloader = cmd == COMMAND_REBOOT ? 0 : JUMP;
    reboot();
  }
  Endpoint_ClearStatusStage();
}
void processHIDReadFeatureReport(uint8_t cmd, uint8_t report, const void* request) {
  Endpoint_ClearSETUP();
  uint8_t header = FRAME_START_FEATURE_READ;
  writeData(&header, 1);
  writeData(&cmd, 1);
}

void EVENT_USB_Device_ControlRequest(void) { deviceControlRequest(); }
