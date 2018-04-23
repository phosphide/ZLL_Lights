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
#include "app_light_calibration.h"
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

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef struct
{
	uint16 u16Gamma;
	uint16 u16Brightness;
}tsLC_ChannelCalibration;

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

PRIVATE uint32 antilog(uint32 y);
PRIVATE void vLC_ProcessCommand(char *pcCommand);
PRIVATE void vLC_WriteStringToUART(const char *pcStr);
PRIVATE void vLC_UnsignedIntegerToString(unsigned int uValue, char *pcString);
PRIVATE uint16_t u16LC_StringToUnsignedInteger(const char *pcString, char **pcEndPtr);

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

/* Map of bulbs to PCA9685 channels. */
PRIVATE const uint8 au8ChannelMap[NUM_BULBS * 3] = {
		7,   255, 255,  /* W1 */
		6,   255, 255,  /* W2 */
		5,   255, 255,  /* W3 */
		4,   3,   2,    /* R1, G1, B1 */
		1,   0,   11,   /* R2, G2, B2 */
		10,  9,   8     /* R3, G3, B3 */
};

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************
 * NAME: vLC_InitSerialInterface
 *
 * DESCRIPTION:
 * Callback that is invoked whenever UART0 receives a byte
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
	char *pcCommandNext = pcCommand; /* pointer to start of command */
	bool bValid = false; /* whether channel specifier is valid */
	unsigned int i;
	uint16 u16Parameter;
	char acNumberBuffer[16];

	/* Commands take the form: <channel specifier> <command>, so the minimum
	 * length is 2 characters. */
	if (strlen(pcCommand) >= 2)
	{
		char c = pcCommand[0];
		unsigned int n;

		if ((c >= '0') && (c <= '9'))
		{
			/* Channel specifier is just a number. Interpret that number
			 * as the raw channel number. */
			n = u16LC_StringToUnsignedInteger(pcCommand, &pcCommandNext);
			u32ChannelMask = 1UL << n;
			bValid = true;
		}
		else if (c == '*')
		{
			/* Channel specifier is '*', so configure all channels at once. */
			u32ChannelMask = 0xffffffff;
			pcCommandNext = pcCommand + 1;
			bValid = true;
		}
		else if ((c == 'r') || (c == 'g') || (c == 'b') || (c == 'w') || (c == 'a'))
		{
			/* Channel specifier is something like "r1", "r*" or "w2". We'll
			 * need to look at what comes after the first letter. */
			/* 'r' is red, 'g' is green, 'b' is blue, 'w' is white, 'a' is all
			 * RGB at once. */
			char c2 = pcCommand[1]; /* c2 = what comes after first letter */
			uint8 u8Bulb = 0;
			uint8 u8NumBulbs = 1;

			if ((c2 >= '0') && (c2 <= '9'))
			{
				/* Channel specifier is something like "r1", "r2" or "b1".
				 * We need to read the number n after the first letter. */
				n = u16LC_StringToUnsignedInteger(&(pcCommand[1]), &pcCommandNext);
				/* Note that (non raw) channel indices start at 1.
				 * u16LC_StringToUnsignedInteger will return 0 on error, so
				 * if n == 0 then the number is invalid. */
				if (n > 0)
				{
					/* Convert to bulb number u8Bulb */
					if (c == 'w')
					{
						u8Bulb = (uint8)(n - 1);
						if (n <= NUM_MONO_LIGHTS)
						{
							bValid = true;
						}
					}
					else
					{
						u8Bulb = (uint8)(n + NUM_MONO_LIGHTS - 1);
						if (n <= NUM_RGB_LIGHTS)
						{
							bValid = true;
						}
					}

				}
			}
			else if (c2 == '*')
			{
				/* Channel specifier is something like "r*" or "b*" i.e.
				 * configure all red channels at once, or all blue
				 * channels at once (respectively). */
				pcCommandNext = pcCommand + 2;
				if (c == 'w')
				{
					u8Bulb = 0;
					u8NumBulbs = NUM_MONO_LIGHTS;
				}
				else
				{
					u8Bulb = NUM_MONO_LIGHTS;
					u8NumBulbs = NUM_RGB_LIGHTS;
				}
				bValid = true;
			}

			/* Mark which channels to configure */
			for (i = 0; i < u8NumBulbs; i++)
			{
				if (c == 'a')
				{
					/* All components in the group */
					u32ChannelMask |= 1UL << u8LC_GetChannel((uint8)(u8Bulb + i), BULB_RED);
					u32ChannelMask |= 1UL << u8LC_GetChannel((uint8)(u8Bulb + i), BULB_GREEN);
					u32ChannelMask |= 1UL << u8LC_GetChannel((uint8)(u8Bulb + i), BULB_BLUE);
				}
				else if (c == 'r')
				{
					u32ChannelMask |= 1UL << u8LC_GetChannel((uint8)(u8Bulb + i), BULB_RED);
				}
				else if (c == 'g')
				{
					u32ChannelMask |= 1UL << u8LC_GetChannel((uint8)(u8Bulb + i), BULB_GREEN);
				}
				else if (c == 'b')
				{
					u32ChannelMask |= 1UL << u8LC_GetChannel((uint8)(u8Bulb + i), BULB_BLUE);
				}
				else if (c == 'w')
				{
					u32ChannelMask |= 1UL << u8LC_GetChannel((uint8)(u8Bulb + i), BULB_WHITE);
				}
			}
		}
	}
	else
	{
		vLC_WriteStringToUART("Command too short\r\n");
		return;
	}

	if (bValid)
	{
		/* If execution flows to here, that means the channel specifier was
		 * valid. */
		char cCommand;

		/* Skip whitespace */
		while (*pcCommandNext == ' ')
		{
			pcCommandNext++;
		}
		cCommand = *pcCommandNext;
		if ((cCommand == 'g') || (cCommand == 'b'))
		{
			/* Change gamma or brightness correction */
			pcCommandNext++;
			u16Parameter = u16LC_StringToUnsignedInteger(pcCommandNext, NULL);
			if (u16Parameter != 0)
			{
				for (i = 0; i < NUM_CHANNELS; i++)
				{
					if ((u32ChannelMask >> i) & 1)
					{
						vLC_WriteStringToUART("channel ");
						vLC_UnsignedIntegerToString(i, acNumberBuffer);
						vLC_WriteStringToUART(acNumberBuffer);
						if (cCommand == 'g')
						{
							vLC_WriteStringToUART(" gamma = ");
							atsLC_Calibration[i].u16Gamma = u16Parameter;
						}
						else if (cCommand == 'b')
						{
							vLC_WriteStringToUART(" brightness = ");
							atsLC_Calibration[i].u16Brightness = u16Parameter;
						}
						vLC_UnsignedIntegerToString(u16Parameter, acNumberBuffer);
						vLC_WriteStringToUART(acNumberBuffer);
						vLC_WriteStringToUART("\r\n");
					}
				}
				/* Refresh current PWM values, to account for new calibration
				 * parameters. */
				for (i = 0; i < (NUM_MONO_LIGHTS + NUM_RGB_LIGHTS); i++)
				{
					DriverBulb_vOutput((uint8)i);
				}
				/* Save calibration, so that it will persist across reset */
				vLC_SaveCalibrationToNVM();
			}
		}
		else
		{
			vLC_WriteStringToUART("Invalid command\r\n");
		}
	}
	else
	{
		vLC_WriteStringToUART("Invalid channel specifier\r\n");
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
 * NAME:	vLC_UnsignedIntegerToString
 *
 * DESCRIPTION:
 *			Convert number iValue into null-terminated string, writing to
 *			pcString. The number will be in base-10 representation.
 *
 *			This is used instead of printf to reduce RAM use.
 ****************************************************************************/
PRIVATE void vLC_UnsignedIntegerToString(unsigned int uValue, char *pcString)
{
	unsigned int uIndex = 0;
	unsigned int i;
	unsigned int uRemainder;
	char cTemp;

	if (uValue == 0)
	{
		pcString[0] = '0';
		pcString[1] = '\0';
		return;
	}

	while (uValue != 0)
	{
		uRemainder = uValue % 10;
		uValue = uValue / 10;
		pcString[uIndex++] = (char)('0' + uRemainder);
	}
	/* Reverse string */
	for (i = 0; i < (uIndex >> 1); i++)
	{
		cTemp = pcString[i];
		pcString[i] = pcString[uIndex - 1 - i];
		pcString[uIndex - 1 - i] = cTemp;
	}
	/* Null-terminate */
	pcString[uIndex++] = '\0';
}

/****************************************************************************
 * NAME:	u16LC_StringToUnsignedInteger
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
PRIVATE uint16_t u16LC_StringToUnsignedInteger(const char *pcString, char **pcEndPtr)
{
	uint16_t u16Value = 0;

	/* Skip leading whitespace */
	while ((*pcString == ' ') || (*pcString == '\r') || (*pcString == '\n') || (*pcString == '\t'))
	{
		pcString++;
	}

	while ((*pcString >= '0') && (*pcString <= '9'))
	{
		u16Value *= 10;
		u16Value += (uint16)(*pcString - '0');
		pcString++;
	}

	if (pcEndPtr)
	{
		*pcEndPtr = (char *)pcString;
	}
	return u16Value;
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
