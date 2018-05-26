/*****************************************************************************
 *
 * MODULE:             JN-AN-1171
 *
 * COMPONENT:          app_temp_sensor.h
 *
 * DESCRIPTION:        Interface for app_temp_sensor.c
 *
 ****************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/

#ifndef APP_TEMP_SENSOR_H
#define APP_TEMP_SENSOR_H

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <stdint.h>
#include <jendefs.h>

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/* Board temperature limit in degrees Celsius. If the temperature exceeds
 * this, all lights will be shut off to prevent stuff from burning. */
#define TEMPERATURE_OVERHEAT_CUTOFF			85
/* After the board overheats, once the temperature drops below this threshold,
 * normal light operation will resume. This number is in degrees Celsius. */
#define TEMPERATURE_RESTORE_THRESHOLD		75

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

PUBLIC void vTS_InitTempSensor(void);
PUBLIC int16 i16TS_GetTemperature(void);

/****************************************************************************/
/***        External Variables                                            ***/
/****************************************************************************/

extern volatile bool_t bOverheat;

#endif /* APP_TEMP_SENSOR_H */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
