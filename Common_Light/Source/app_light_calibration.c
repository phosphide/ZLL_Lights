/*****************************************************************************
 *
 * MODULE:             JN-AN-1171
 *
 * COMPONENT:          app_light_calibration.c
 *
 * DESCRIPTION:        Does calculations and configuration for light
 *                     calibration
 *
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

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <jendefs.h>
#include <AppHardwareApi.h>
#include "PDM.h"
#include "PDM_IDs.h"
#include "os.h"
#include "app_zcl_light_task.h"
#include "app_light_calibration.h"
#include "app_temp_sensor.h"
#include "DriverBulb.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/* Default calibration value for gamma. 1024 = 1.0. 2867 corresponds to 2.8,
 * a reasonable value for most LEDs. */
#define DEFAULT_GAMMA			2867
/* Default calibration value for brightness. 1024 = 1.0. */
#define DEFAULT_BRIGHTNESS		1024

/* Size of UART TX buffer in number of bytes */
#define TX_BUF_SIZE				32
/* Size of UART RX buffer in number of bytes */
#define RX_BUF_SIZE				32

/* Maximum size of configuration line, in number of characters */
#define MAX_LINE_SIZE			40

/* Convert preprocessor definition x to string literal. Both of these are
 * necessary. */
#define STRINGIFY(x)			#x
#define TOSTRING(x)				STRINGIFY(x)

#ifndef BOARD_VERSION
/* Board version string */
#define BOARD_VERSION			("MultiLight " TOSTRING(VARIANT) " built on " __DATE__ " " __TIME__)
#endif

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

typedef struct
{
	uint16 u16Gamma;
	uint16 u16Brightness;
} tsLC_ChannelCalibration;

typedef enum
{
  CHANNEL_GAMMA,
  CHANNEL_BRIGHTNESS
} teChannelSetting;

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

PRIVATE uint32 antilog(uint32 y);
PRIVATE void vLC_ProcessCommand(char *pcCommand);
PRIVATE void vLC_WriteChannelStatusToUART(uint8 u8Channel, teChannelSetting teSetting);
PRIVATE void vLC_WriteStringToUART(const char *pcStr);
PRIVATE void vLC_WriteUnsignedIntegerToUART(unsigned int uValue);
PRIVATE uint32_t u32LC_StringToUnsignedInteger(const char *pcString, char **pcEndPtr);

/****************************************************************************/
/*          Exported Variables                                              */
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

#include "log_table.h"
PRIVATE tsLC_ChannelCalibration atsLC_Calibration[NUM_CHANNELS];
PRIVATE uint8 au8TxBuf[TX_BUF_SIZE];
PRIVATE uint8 au8RxBuf[RX_BUF_SIZE];
PRIVATE char acCurrentLine[MAX_LINE_SIZE + 1]; // + 1 for null
PRIVATE unsigned int uCurrentLineSize;
PRIVATE uint32 u32NewComputedWhiteMode;

#ifdef VARIANT_MINI
/* Map of bulbs to JN5168 Timer channels. */
PRIVATE const uint8 au8ChannelMap[NUM_BULBS * 3] = {
		4,   255, 255,  /* W1 */
		3,   2,   0     /* R1, G1, B1 */
};
#else
/* Map of bulbs to PCA9685 channels. */
PRIVATE const uint8 au8ChannelMap[NUM_BULBS * 3] = {
		7,   255, 255,  /* W1 */
		6,   255, 255,  /* W2 */
		5,   255, 255,  /* W3 */
		4,   3,   2,    /* R1, G1, B1 */
		1,   0,   11,   /* R2, G2, B2 */
		10,  9,   8     /* R3, G3, B3 */
};
#endif

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************
 * NAME: vLC_InitSerialInterface
 *
 * DESCRIPTION:
 * Initialises and configures UART
 ****************************************************************************/
PUBLIC void vLC_InitSerialInterface(void)
{
	/* Use DIO6/DIO7. This must be called before UartEnable, and it is the only
	 * UART-related function which can be called before UartEnable.*/
	vAHI_UartSetLocation(E_AHI_UART_0, FALSE);
	/* Enable UART */
	bAHI_UartEnable(E_AHI_UART_0, au8TxBuf, sizeof(au8TxBuf), au8RxBuf, sizeof(au8RxBuf));

	/* 38400 baud, 8N1 */
	vAHI_UartSetBaudRate(E_AHI_UART_0, E_AHI_UART_RATE_38400);
	vAHI_UartSetControl(E_AHI_UART_0,
			E_AHI_UART_EVEN_PARITY,
			E_AHI_UART_PARITY_DISABLE,
			E_AHI_UART_WORD_LEN_8,
			E_AHI_UART_1_STOP_BIT,
			E_AHI_UART_RTS_LOW);
	/* Don't use RTS/CTS */
	vAHI_UartSetRTSCTS(E_AHI_UART_0, FALSE);
	vAHI_UartSetAutoFlowCtrl(E_AHI_UART_0, E_AHI_UART_FIFO_ARTS_LEVEL_8, FALSE, FALSE, FALSE);
	/* Interrupt on RX */
	vAHI_UartSetInterrupt(E_AHI_UART_0, FALSE, FALSE, FALSE, TRUE, E_AHI_UART_FIFO_LEVEL_1);

	/* Set new computed white mode to existing computed white mode, so that
	 * the "i" command returns the correct mode. */
	u32NewComputedWhiteMode = u32ComputedWhiteMode;
}

/****************************************************************************
 * NAME: APP_isrUart
 *
 * DESCRIPTION:
 * ISR for UART0
 ****************************************************************************/
OS_ISR(APP_isrUart)
{
	while (u16AHI_UartReadRxFifoLevel(E_AHI_UART_0) > 0)
	{
		uint8 nextByte = u8AHI_UartReadData(E_AHI_UART_0);
		/* Accept both carriage return or newline characters as command end.
		 * If both are sent, that will be interpreted as a blank line and
		 * ignored. */
		if ((nextByte == '\n') || (nextByte == '\r'))
		{
			/* Finish current line and process it */
			acCurrentLine[uCurrentLineSize] = '\0';
			if (uCurrentLineSize > 0)
			{
				vLC_ProcessCommand(acCurrentLine);
			}
			/* Begin next line */
			uCurrentLineSize = 0;
		}
		else if (uCurrentLineSize < MAX_LINE_SIZE)
		{
			/* Add to current line */
			acCurrentLine[uCurrentLineSize++] = (char)nextByte;
		}
	}
}

/****************************************************************************
 * NAME: u8LC_GetChannel
 *
 * DESCRIPTION:
 * Get channel number from bulb number and colour component. This will
 * basically look up the appropriate value from au8ChannelMap.
 ****************************************************************************/
PUBLIC uint8 u8LC_GetChannel(uint8 u8Bulb, teColour eColour)
{
	return au8ChannelMap[u8Bulb * 3 + eColour];
}

/****************************************************************************
 * NAME: vLC_LoadCalibrationFromNVM
 *
 * DESCRIPTION:
 * Load calibration values from EEPROM
 ****************************************************************************/
PUBLIC void vLC_LoadCalibrationFromNVM(void)
{
	PDM_teStatus eStatus;
	uint16 u16ByteRead;

	eStatus = PDM_eReadDataFromRecord(PDM_ID_APP_LIGHT_CALIB,
				&atsLC_Calibration,
	            sizeof(atsLC_Calibration), &u16ByteRead);

	if ((eStatus != PDM_E_STATUS_OK) || (u16ByteRead != sizeof(atsLC_Calibration)))
	{
		/* Failed to load calibration data from PDM; load defaults. */
		unsigned int i;
		for (i = 0; i < NUM_CHANNELS; i++)
		{
			atsLC_Calibration[i].u16Gamma = DEFAULT_GAMMA;
			atsLC_Calibration[i].u16Brightness = DEFAULT_BRIGHTNESS;
		}
	}
}

/****************************************************************************
 * NAME: vLC_SaveCalibrationToNVM
 *
 * DESCRIPTION:
 * Save calibration values to EEPROM
 ****************************************************************************/
PUBLIC void vLC_SaveCalibrationToNVM(void)
{
	PDM_eSaveRecordData(PDM_ID_APP_LIGHT_CALIB, &atsLC_Calibration, sizeof(atsLC_Calibration));
	PDM_eSaveRecordData(PDM_ID_APP_COMPUTE_WHITE, &u32NewComputedWhiteMode, sizeof(u32NewComputedWhiteMode));
}

/****************************************************************************
 * NAME: u32LC_AdjustIntensity
 *
 * DESCRIPTION:
 * Adjusts intensity based on current calibration values. u8Intensity is
 * expected to be between 1 and 255 (inclusive). This will return a value
 * between 1 and 4095 (inclusive).
 ****************************************************************************/
PUBLIC uint32 u32LC_AdjustIntensity(uint8 u8Intensity, uint8 u8ChannelNum)
{
	uint32 x;
	uint32 y;
	uint32 u32Gamma = atsLC_Calibration[u8ChannelNum].u16Gamma;
	uint32 u32Brightness = atsLC_Calibration[u8ChannelNum].u16Brightness;

	if (u8Intensity == 0)
		return 0;
	if (u8Intensity > 254)
		u8Intensity = 254;
	y = log_table_short[u8Intensity];
	y = y * u32Gamma;
	y = (y >> 10) + ((y & 512) >> 9); /* round */
	x = antilog(y);
	x = x * u32Brightness;
	x = (x >> 10) + ((x & 512) >> 9); /* round */
	if (x < 1) x = 1;
	if (x > 4095) x = 4095;
	return x;
}

/****************************************************************************/
/***        Local    Functions                                            ***/
/****************************************************************************/

/****************************************************************************
 * NAME:	antilog
 *
 * DESCRIPTION:
 *			Inverse log i.e. exponentiate. This will return the index i such
 *			that log_table_long(i) is close to y.
 *			Output values will be in the range 0 to 4095, where 0 represents
 *			0 intensity and 4095 represents maximum intensity.
 ****************************************************************************/
PRIVATE uint32 antilog(uint32 y)
{
	uint32 left_index;
	uint32 right_index;
	uint32 diff_left;
	uint32 diff_right;

    if (y > log_table_long[1])
        return 0;
    /* Binary search through log_table_long */
    left_index = 1;
    right_index = 4095;
    while ((left_index + 1) != right_index)
    {
    	uint32 i = (right_index + left_index) >> 1;
        if (log_table_long[i] < y)
            right_index = i;
        else
            left_index = i;
    }
    diff_left = log_table_long[left_index] - y;
    diff_right = y - log_table_long[right_index];
    if (diff_left < diff_right)
        return left_index;
    else
        return right_index;
}

/****************************************************************************
 * NAME:	vLC_ProcessCommand
 *
 * DESCRIPTION:
 *			Process and execute calibration/configuration command
 ****************************************************************************/
PRIVATE void vLC_ProcessCommand(char *pcCommand)
{
	uint32 u32ChannelMask = 0; /* bit n set means that channel is selected */
	uint32 u32Parameter = 0;
	char *pcCommandNext = pcCommand; /* pointer to start of command */
	bool bFirst;
	unsigned int i;
	int16 i16Temperature;

	if (strlen(pcCommand) < 1)
	{
		vLC_WriteStringToUART("Command too short\r\n");
		return;
	}

	switch(pcCommand[0])
	{
	case 'g':
	case 'b':
		/* Set gamma or brightness */
		/* Format of command is [g or b] <channel mask> <value> */
		u32ChannelMask = u32LC_StringToUnsignedInteger(&(pcCommand[1]), &pcCommandNext);
		u32Parameter = u32LC_StringToUnsignedInteger(pcCommandNext, NULL);
		bFirst = true;
		for (i = 0; i < NUM_CHANNELS; i++)
		{
			if ((u32ChannelMask >> i) & 1)
			{
				if (bFirst)
				{
					bFirst = false;
				}
				else
				{
					vLC_WriteStringToUART(",");
				}
				if (pcCommand[0] == 'g')
				{
					atsLC_Calibration[i].u16Gamma = u32Parameter;
					vLC_WriteChannelStatusToUART(i, CHANNEL_GAMMA);
				}
				else if (pcCommand[0] == 'b')
				{
					atsLC_Calibration[i].u16Brightness = u32Parameter;
					vLC_WriteChannelStatusToUART(i, CHANNEL_BRIGHTNESS);
				}
			}
		}
		vLC_WriteStringToUART("\r\n");
		/* Refresh current PWM values, to account for new calibration
		 * parameters. */
		for (i = 0; i < NUM_BULBS; i++)
		{
			DriverBulb_vOutput((uint8)i);
		}
		break;

	case 'n':
		/* Get raw channel names */
		/* Output will be a series of <raw channel number>=<name> entries,
		 * separated by ",". */
		bFirst = true;
		/* List white channels */
		for (i = 0; i < NUM_MONO_LIGHTS; i++)
		{
			if (bFirst)
			{
				bFirst = false;
			}
			else
			{
				vLC_WriteStringToUART(",");
			}
			vLC_WriteUnsignedIntegerToUART(u8LC_GetChannel(i, BULB_WHITE));
			vLC_WriteStringToUART("=White ");
			vLC_WriteUnsignedIntegerToUART(i + 1);
		}
		/* List red/green/blue channels */
		for (i = 0; i < NUM_RGB_LIGHTS; i++)
		{
			if (bFirst)
			{
				bFirst = false;
			}
			else
			{
				vLC_WriteStringToUART(",");
			}
			vLC_WriteUnsignedIntegerToUART(u8LC_GetChannel(NUM_MONO_LIGHTS + i, BULB_RED));
			vLC_WriteStringToUART("=Red ");
			vLC_WriteUnsignedIntegerToUART(i + 1);
			vLC_WriteStringToUART(",");
			vLC_WriteUnsignedIntegerToUART(u8LC_GetChannel(NUM_MONO_LIGHTS + i, BULB_GREEN));
			vLC_WriteStringToUART("=Green ");
			vLC_WriteUnsignedIntegerToUART(i + 1);
			vLC_WriteStringToUART(",");
			vLC_WriteUnsignedIntegerToUART(u8LC_GetChannel(NUM_MONO_LIGHTS + i, BULB_BLUE));
			vLC_WriteStringToUART("=Blue ");
			vLC_WriteUnsignedIntegerToUART(i + 1);
		}
		vLC_WriteStringToUART("\r\n");
		break;

	case 'i':
		/* Get information about all channels */
		bFirst = true;
		for (i = 0; i < NUM_CHANNELS; i++)
		{
			if (bFirst)
			{
				bFirst = false;
			}
			else
			{
				vLC_WriteStringToUART(",");
			}
			vLC_WriteChannelStatusToUART(i, CHANNEL_GAMMA);
			vLC_WriteStringToUART(",");
			vLC_WriteChannelStatusToUART(i, CHANNEL_BRIGHTNESS);
		}
		vLC_WriteStringToUART(",ComputedWhiteMode=");
		vLC_WriteUnsignedIntegerToUART((unsigned int)u32NewComputedWhiteMode);
		vLC_WriteStringToUART("\r\n");
		break;

	case 'w':
		/* Set computed white mode */
		/* We don't set u32ComputedWhiteMode because several
		 * commissioning-related functions refer to it. So it's not safe to
		 * update u32ComputedWhiteMode without restarting. So we just update
		 * u32NewComputedWhiteMode, save it to NVM, so the new value will
		 * be used on the next restart. */
		u32NewComputedWhiteMode = u32LC_StringToUnsignedInteger(&(pcCommand[1]), NULL);
		vLC_WriteStringToUART("ComputedWhiteMode=");
		vLC_WriteUnsignedIntegerToUART((unsigned int)u32NewComputedWhiteMode);
		vLC_WriteStringToUART("\r\n");
		break;

	case 's':
		/* Save settings to non-volatile memory */
		vLC_WriteStringToUART("saving\r\n");
		vLC_SaveCalibrationToNVM();
		break;

	case 'r':
		/* Reset board */
		vLC_WriteStringToUART("resetting\r\n");
		vAHI_SwReset();
		break;

	case 'v':
		/* Get board version */
		vLC_WriteStringToUART(BOARD_VERSION);
		vLC_WriteStringToUART("\r\n");
		break;

	case 't':
		/* Get board temperature */
		i16Temperature = i16TS_GetTemperature();
		if (i16Temperature < 0)
		{
			vLC_WriteStringToUART("-");
			i16Temperature = -i16Temperature;
		}
		vLC_WriteUnsignedIntegerToUART((uint16)i16Temperature);
		vLC_WriteStringToUART("\r\n");
		break;

	default:
		vLC_WriteStringToUART("Unknown command\r\n");
		break;
	}

}

/****************************************************************************
 * NAME:	vLC_WriteChannelStatusToUART
 *
 * DESCRIPTION:
 *			Write channel status to UART. The channel status will be
 *			reported as <raw channel number>:<setting>=<value> e.g.
 *			"0:gamma=1024", which means that channel 0 has a gamma setting
 *			of 1024.
 ****************************************************************************/
PRIVATE void vLC_WriteChannelStatusToUART(uint8 u8Channel, teChannelSetting teSetting)
{
	vLC_WriteUnsignedIntegerToUART(u8Channel);
	if (teSetting == CHANNEL_GAMMA)
	{
		vLC_WriteStringToUART(":gamma=");
		vLC_WriteUnsignedIntegerToUART(atsLC_Calibration[u8Channel].u16Gamma);
	}
	else if (teSetting == CHANNEL_BRIGHTNESS)
	{
		vLC_WriteStringToUART(":brightness=");
		vLC_WriteUnsignedIntegerToUART(atsLC_Calibration[u8Channel].u16Brightness);
	}
}

/****************************************************************************
 * NAME:	vLC_WriteStringToUART
 *
 * DESCRIPTION:
 *			Write string to UART
 ****************************************************************************/
PRIVATE void vLC_WriteStringToUART(const char *pcStr)
{
	unsigned int i;

	for (i = 0; i < strlen(pcStr); i++)
	{
		while (u16AHI_UartReadTxFifoLevel(E_AHI_UART_0) == TX_BUF_SIZE)
		{
			/* wait */
		}
		vAHI_UartWriteData(E_AHI_UART_0, (uint8)pcStr[i]);
	}
}

/****************************************************************************
 * NAME:	vLC_WriteUnsignedIntegerToUART
 *
 * DESCRIPTION:
 *			Convert number iValue into null-terminated string, then write
 *			that string to the UART. The number will be in base-10
 *			representation.
 *
 *			This is used instead of printf to reduce RAM use.
 ****************************************************************************/
PRIVATE void vLC_WriteUnsignedIntegerToUART(unsigned int uValue)
{
	unsigned int uIndex = 0;
	unsigned int i;
	unsigned int uRemainder;
	char cTemp;
	char acString[16];

	if (uValue == 0)
	{
		acString[0] = '0';
		acString[1] = '\0';
		vLC_WriteStringToUART(acString);
		return;
	}

	while (uValue != 0)
	{
		uRemainder = uValue % 10;
		uValue = uValue / 10;
		acString[uIndex++] = (char)('0' + uRemainder);
	}
	/* Reverse string */
	for (i = 0; i < (uIndex >> 1); i++)
	{
		cTemp = acString[i];
		acString[i] = acString[uIndex - 1 - i];
		acString[uIndex - 1 - i] = cTemp;
	}
	/* Null-terminate */
	acString[uIndex++] = '\0';
	vLC_WriteStringToUART(acString);
}

/****************************************************************************
 * NAME:	u32LC_StringToUnsignedInteger
 *
 * DESCRIPTION:
 *			Convert null-terminated string into unsigned integer, assuming
 *			number is in base-10 representation. This will return 0 on error
 *			(e.g. no number there). This will skip leading whitespace.
 *			If pcEndPtr is not NULL, then pcEndPtr will be written with a
 *			pointer to the first character past the end of the number.
 *
 *			This is used instead of sscanf or strtol to reduce RAM use.
 ****************************************************************************/
PRIVATE uint32_t u32LC_StringToUnsignedInteger(const char *pcString, char **pcEndPtr)
{
	uint32_t u32Value = 0;

	/* Skip whitespace */
	while ((*pcString == ' ') || (*pcString == '\r') || (*pcString == '\n') || (*pcString == '\t'))
	{
		pcString++;
	}

	while ((*pcString >= '0') && (*pcString <= '9'))
	{
		u32Value *= 10;
		u32Value += (uint32)(*pcString - '0');
		pcString++;
	}

	if (pcEndPtr)
	{
		*pcEndPtr = (char *)pcString;
	}
	return u32Value;
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
