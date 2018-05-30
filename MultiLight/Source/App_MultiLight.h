/*****************************************************************************
 *
 * MODULE:             JN-AN-1171
 *
 * COMPONENT:          App_Light_ColorLight.h
 *
 * DESCRIPTION:        ZLL Demo: Colored Light - Interface
 *
 ****************************************************************************
 *
 * This software is owned by NXP B.V. and/or its supplier and is protected
 * under applicable copyright laws. All rights are reserved. We grant You,
 * and any third parties, a license to use this software solely and
 * exclusively on NXP products [NXP Microcontrollers such as JN5168, JN5164,
 * JN5161, JN5148, JN5142, JN5139].
 * You, and any third parties must reproduce the copyright and warranty notice
 * and any other legend of ownership on each copy or partial copy of the
 * software.
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
 * Copyright NXP B.V. 2012. All rights reserved
 *
 ***************************************************************************/

#ifndef APP_COLOR_LIGHT_H
#define APP_COLOR_LIGHT_H

#include "colour_light.h"
#include "dimmable_light.h"
#include "commission_endpoint.h"

/****************************************************************************/
/***        Constants                                                     ***/
/****************************************************************************/

#ifdef VARIANT_MINI
#define NUM_MONO_LIGHTS		1
#define NUM_RGB_LIGHTS		1
#else
#define NUM_MONO_LIGHTS		3
#define NUM_RGB_LIGHTS		3
#endif

/****************************************************************************/
/***        External Variables                                            ***/
/****************************************************************************/

extern tsZLL_DimmableLightDevice sLightMono[NUM_MONO_LIGHTS];
extern tsZLL_ColourLightDevice sLightRGB[NUM_RGB_LIGHTS];

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

PUBLIC uint8 u8App_GetNumberOfDevices(void);
PUBLIC tsCLD_ZllDeviceRecord *psApp_GetDeviceRecord(uint8 u8RecordNum);
PUBLIC teZCL_Status eApp_ZLL_RegisterEndpoint(tfpZCL_ZCLCallBackFunction fptr,
                                       tsZLL_CommissionEndpoint* psCommissionEndpoint);
PUBLIC bool_t bEndPointToNum(uint8 u8Endpoint, bool_t* bIsRGB, uint8* u8Num);
PUBLIC void vApp_eCLD_ColourControl_GetRGB(uint8 u8Endpoint,uint8* pu8Red,uint8* pu8Green,uint8* pu8Blue);
PUBLIC void vAPP_ZCL_DeviceSpecific_Init(void);
PUBLIC void vStartEffect(uint8 u8Endpoint, uint8 u8Effect);
PUBLIC void vIdEffectTick(void);

PUBLIC void vRGBLight_SetLevels(uint8 u8Bulb, bool_t bOn, uint8 u8Level, uint8 u8Red,
                                uint8 u8Green, uint8 u8Blue);
PUBLIC void vSetBulbState(uint8 u8Bulb, bool bOn, uint8 u8Level);
PUBLIC void APP_vHandleIdentify(uint8 u8Endpoint);
PUBLIC void APP_vHandleIdentifyAll(void);
PUBLIC void vCreateInterpolationPoints( void);

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

#endif /* APP_COLOR_LIGHT_H */
