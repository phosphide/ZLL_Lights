/*
 * uart.h
 *
 *  Created on: 27 Apr 2020
 *      Author: phosphide
 */

#ifndef UART_H_
#define UART_H_

#include "colour_light.h"
#include "zps_gen.h"
#include "AppHardwareApi.h"

void UART_init();
void UART_send_state();

#endif /* UART_H_ */
