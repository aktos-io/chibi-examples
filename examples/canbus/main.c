#include "hal.h"
#include "ch.h"
#include "math.h"


/*
 * edit and enable CAN interfaces in halconf.h and mcuconf.h accordingly
 */
 
struct can_instance {
  CANDriver     *canp;
  uint32_t      led;
};

static const struct can_instance can1 = {&CAND1, GPIOD_LED5};
static const struct can_instance can2 = {&CAND2, GPIOD_LED3};

/*
 * Internal loopback mode, 500KBaud, automatic wakeup, automatic recover
 * from abort mode.
 * See section 22.7.7 on the STM32 reference manual.
 */
static const CANConfig cancfg = {
  CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_MCR_TXFP,
  CAN_BTR_LBKM | CAN_BTR_SJW(0) | CAN_BTR_TS2(1) |
  CAN_BTR_TS1(8) | CAN_BTR_BRP(6)
};

/*
 * Receiver thread.
 */
static THD_WORKING_AREA(can_rx1_wa, 256);
static THD_WORKING_AREA(can_rx2_wa, 256);
static THD_FUNCTION(can_rx, p) {
  struct can_instance *cip = p;
  event_listener_t el;
  CANRxFrame rxmsg;

  (void)p;
  chRegSetThreadName("receiver");
  chEvtRegister(&cip->canp->rxfull_event, &el, 0);
  while(!chThdShouldTerminateX()) {
    if (chEvtWaitAnyTimeout(ALL_EVENTS, MS2ST(100)) == 0)
      continue;
    while (canReceive(cip->canp, CAN_ANY_MAILBOX,
                      &rxmsg, TIME_IMMEDIATE) == MSG_OK) {
      /* Process message.*/
      palTogglePad(GPIOD, cip->led);
    }
  }
  chEvtUnregister(&CAND1.rxfull_event, &el);
}

/*
 * Transmitter thread.
 */
static THD_WORKING_AREA(can_tx_wa, 256);
static THD_FUNCTION(can_tx, p) {
  CANTxFrame txmsg;

  (void)p;
  chRegSetThreadName("transmitter");
  txmsg.IDE = CAN_IDE_EXT;
  txmsg.EID = 0x01234567;
  txmsg.RTR = CAN_RTR_DATA;
  txmsg.DLC = 8;
  txmsg.data32[0] = 0x55AA55AA;
  txmsg.data32[1] = 0x00FF00FF;

  while (!chThdShouldTerminateX()) {
    canTransmit(&CAND1, CAN_ANY_MAILBOX, &txmsg, MS2ST(100));
    chThdSleepMilliseconds(100);
    canTransmit(&CAND2, CAN_ANY_MAILBOX, &txmsg, MS2ST(100));
    chThdSleepMilliseconds(500);
  }
}


/*
 * Application entry point.
 */
int main(void) {

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /*
   * Activates the CAN drivers 1 and 2.
   */
  canStart(&CAND1, &cancfg);
  canStart(&CAND2, &cancfg);

  /*
   * Starting the transmitter and receiver threads.
   */
  chThdCreateStatic(can_rx1_wa, sizeof(can_rx1_wa), NORMALPRIO + 7,
                    can_rx, (void *)&can1);
  chThdCreateStatic(can_rx2_wa, sizeof(can_rx2_wa), NORMALPRIO + 7,
                    can_rx, (void *)&can2);
  chThdCreateStatic(can_tx_wa, sizeof(can_tx_wa), NORMALPRIO + 7,
                    can_tx, NULL);

  /*
   * Normal main() thread activity, in this demo it does nothing.
   */
  while (true) {
    chThdSleepMilliseconds(500);
  }
  return 0;
}