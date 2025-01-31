//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wtype-limits"
//#pragma GCC diagnostic ignored "-Wunused-variable"
#include <Arduino.h>

#ifdef __AVR__
  #include <avr/wdt.h>
#endif

#include "AR488.h"
#include "serial.h"
#include "controller.h"
#include "commands.h"
#include "gpib.h"
#include "macros.h"

#ifdef ESP32
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#endif

/***** FWVER "AR488 GPIB controller, ver. 0.49.14, 02/03/2021" *****/

/*
  Arduino IEEE-488 implementation by John Chajecki

  Inspired by the original work of Emanuele Girlando, licensed under a Creative
  Commons Attribution-NonCommercial-NoDerivatives 4.0 International License.
  Any code in common with the original work is reproduced here with the explicit
  permission of Emanuele Girlando, who has kindly reviewed and tested this code.

  Thanks also to Luke Mester for comparison testing against the Prologix interface.
  AR488 is Licenced under the GNU Public licence.

  Thanks to 'maxwell3e10' on the EEVblog forum for suggesting additional auto mode
  settings and the macro feature.

  Thanks to 'artag' on the EEVblog forum for providing code for the 32u4.

	For information regarding the GPIB firmware by Emanualle Girlando see:
	http://egirland.blogspot.com/2014/03/arduino-uno-as-usb-to-gpib-controller.html
*/


/*
   Pin mapping between the Arduino pins and the GPIB connector.
   NOTE:
   GPIB pins 10 and 18-24 are connected to GND
   GPIB pin 12 should be connected to the cable shield (might be n/c)
   Pin mapping follows the layout originally used by Emanuelle Girlando, but adds
   the SRQ line (GPIB 10) on pin 2 and the REN line (GPIB 17) on pin 13. The program
   should therefore be compatible with the original interface design but for full
   functionality will need the remaining two pins to be connected.
   For further information about the AR488 see the documentation.
*/


/**************************/
/***** CONFIGURATION  *****/
/** SEE AR488_Config.h  ***/
/**************************/


/****** Global variables with volatile values related to controller state *****/
Controller *controller=NULL;
GPIB *gpib=NULL;


/******  Arduino standard SETUP procedure *****/
void setup() {
#ifdef ESP32
  // Disable the brownout detector...
	WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif
  // Disable the watchdog (needed to prevent WDT reset loop)
#ifdef __AVR__
  wdt_disable();
#endif

	controller = new Controller();
	gpib = new GPIB(*controller);

	controller->serialstream->println("Setup");

  // Turn off internal LED (set OUPTUT/LOW)
#ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
#endif

  // Initialise parse buffer
  controller->flushPbuf();
  // Initialise
	controller->serialstream->println("Config init...");
  controller->initConfig();
	controller->serialstream->println("Config init done");
#if defined(USE_MACROS)
  // Run startup macro
	if (isMacro(0))
			execMacro(0, *controller);
#endif

	if (controller->verbose()) {
		controller->cmdstream->println(F("AR488 ready."));
		controller->showPrompt();
	}
	controller->serialstream->println("Setup done");
}
/****** End of Arduino standard SETUP procedure *****/


/***** ARDUINO MAIN LOOP *****/
void loop() {

/*** Macros ***/
/*
 * Run the startup macro if enabled
 */
#ifdef USE_MACROS
  // Run user macro if flagged
  if (controller->runMacro > 0) {
			execMacro(controller->runMacro, *controller);
    controller->runMacro = 0;
  }
#endif

/*** Pin Hooks ***/
/*
 * Not all boards support interrupts or have PCINTs. In this
 * case, use in-loop checking to detect when SRQ and ATN have
 * been signalled
 */
#ifndef USE_INTERRUPTS
  gpib->setATN(digitalRead(ATN)==LOW ? true : false);
  gpib->setSRQ(digitalRead(SRQ)==LOW ? true : false);
#endif

/*** Process the buffer ***/
/* Each received char is passed through parser until an un-escaped
 * CR is encountered. If we have a command then parse and execute.
 * If the line is data (inclding direct instrument commands) then
 * send it to the instrument.
 * NOTE: parseInput() sets lnRdy in serialEvent, readBreak or in the
 * above loop
 * lnRdy=1: process command;
 * lnRdy=2: send data to Gpib
 * lnRdy=3: break
 * lnRdy=4: macro edition in progress, add data to current macro
 */

  // lnRdy=1: received a command so execute it...
  if (controller->lnRdy == 1) {
    controller->execCmd();
  }
#if defined(USE_MACROS)
  else if (controller->lnRdy == 4) {
    controller->appendToMacro();
  }
#endif

  // Controller mode:
  if (controller->config.cmode == 2) {
    // lnRdy=2: received data - send it to the instrument...
    if (controller->lnRdy == 2) {
      controller->sendToInstrument();
			// Auto-read data from GPIB bus following any command
      if (controller->config.amode == 1) {
        gpib->gpibReceiveData();
      }
      // Auto-receive data from GPIB bus following a query command
      if (controller->config.amode == 2 && gpib->isQuery) {
        gpib->gpibReceiveData();
        gpib->isQuery = false;
      }
			controller->showPrompt();
    }

    // Check status of SRQ and SPOLL if asserted
    if (gpib->isSRQ() && controller->isSrqa) {
			controller->spoll_h(NULL);
      gpib->clearSRQ();
    }

    // Continuous auto-receive data from GPIB bus
    if (controller->config.amode == 3 && controller->aRead) gpib->gpibReceiveData();
  }

  // Device mode:
  else if (controller->config.cmode == 1) {
    if (controller->isTO) {
			if (controller->lnRdy == 2) {
				controller->sendToInstrument();
				controller->showPrompt();
			}
    } else if (controller->isRO) {
      gpib->lonMode();
    } else {
			if (gpib->isATN()) gpib->attnRequired();
      if (controller->lnRdy == 2) {
				controller->sendToInstrument();
				controller->showPrompt();
			}
    }
  }

  // Reset line ready flag
//  lnRdy = 0;

  // IDN query ?
  if (controller->sendIdn) {
    if (controller->config.idn==1) controller->cmdstream->println(controller->config.sname);
    if (controller->config.idn==2) {
				controller->cmdstream->print(controller->config.sname);
				controller->cmdstream->print("-");
				controller->cmdstream->println(controller->config.serial);
		}
    controller->sendIdn = false;
  }

	// look for incoming data on a stream, and make it the current IO stream
	controller->selectStream();
  // Check serial buffer
  controller->serialIn_h();

  delayMicroseconds(5);
}
/***** END MAIN LOOP *****/


#ifdef DEBUG1
/***** Prints charaters as hex bytes *****/
void printHex(char *buffr, int dsize) {
  for (int i = 0; i < dsize; i++) {
    dbSerial->print(buffr[i], HEX); dbSerial->print(" ");
  }
  dbSerial->println();
}
#endif
