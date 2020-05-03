#include "uart.h"

#include "dbg.h"

uint8_t tx_buffer[81];
uint8_t rx_buffer[81];

const uint8_t digits[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
const uint8_t hex_to_dec[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15};

uint8_t msg[] = "RGB0,on=0,level=00,x=0000,y=0000,hue=00,saturation=00,enhancedhue=0000\n";
//               01234567890123456789012345678901234567890123456789012345678901234567890
//               0         1         2         3         4         5         6         7

inline void sprint_x16(uint8_t *str, uint16_t val) {
    str[3] = digits[val & 0x000F];
    val >>= 4;
    str[2] = digits[val & 0x000F];
    val >>= 4;
    str[1] = digits[val & 0x000F];
    val >>= 4;
    str[0] = digits[val & 0x000F];
}

inline void sprint_x8(uint8_t *str, uint8_t val) {
    str[1] = digits[val & 0x0F];
    str[0] = digits[val >> 4];
}
inline void sprint_b(uint8_t *str, bool val) {
    str[0] = val ? '1' : '0';
}
inline void sscan_x16(const uint8_t *str, uint16_t *val) {
    *val = hex_to_dec[str[0]-'0'] << 12 | hex_to_dec[str[1]-'0'] << 8 |
    hex_to_dec[str[2]-'0'] << 4 | hex_to_dec[str[3]-'0'];
}
inline void sscan_x8(const uint8_t *str, uint8_t *val) {
    *val = hex_to_dec[str[0]-'0'] << 4 | hex_to_dec[str[1]-'0'];
}
inline void sscan_b(const uint8_t *str, bool *val) {
    *val = *str == '1' ? 1 : 0;
}

void UART_callback(uint32_t u32Device, uint32_t u32ItemBitmap) {
	if (u32ItemBitmap == E_AHI_UART_INT_RXDATA) {
		dbg_vPrintfImplNoneVerbose("UART_callback E_AHI_UART_INT_RXDATA\n");
		while (u16AHI_UartReadRxFifoLevel(E_AHI_UART_1)) {
			dbg_vPrintfImplNoneVerbose("UART: %c\n", u8AHI_UartReadData(E_AHI_UART_1));
		}

	} else {
		dbg_vPrintfImplNoneVerbose("UART_callback %d\n", u32ItemBitmap);
	}
}

extern tsZLL_ColourLightDevice sLight;
void os_vUART1_ISR() {
	dbg_vPrintfImplNoneVerbose("os_vUART1_ISR\n");
	while (u16AHI_UartReadRxFifoLevel(E_AHI_UART_1)) {
		uint8_t byte = u8AHI_UartReadData(E_AHI_UART_1);
		dbg_vPrintfImplNoneVerbose("UART: %c\n", byte);
		const uint8_t step = 10;
		if (byte == 'D') {
			if (sLight.sLevelControlServerCluster.u8CurrentLevel >= step) {
				sLight.sLevelControlServerCluster.u8CurrentLevel -= step;
//				eCLD_ColourControlUpdate(LIGHT_COLORLIGHT_LIGHT_00_ENDPOINT);
			}

//			dbg_vPrintfImplNoneVerbose("sLevelControlServerCluster.u8CurrentLevel: %d\n", sLight.sLevelControlServerCluster.u8CurrentLevel);
//			dbg_vPrintfImplNoneVerbose("sColourControlServerCluster.u16CurrentX: %d\n", sLight.sColourControlServerCluster.u16CurrentX);
//			dbg_vPrintfImplNoneVerbose("sColourControlServerCluster.u16CurrentY: %d\n", sLight.sColourControlServerCluster.u16CurrentY);
//			dbg_vPrintfImplNoneVerbose("sColourControlServerCluster.u8CurrentHue: %d\n", sLight.sColourControlServerCluster.u8CurrentHue);
//			dbg_vPrintfImplNoneVerbose("sColourControlServerCluster.u8CurrentSaturation: %d\n", sLight.sColourControlServerCluster.u8CurrentSaturation);
//			dbg_vPrintfImplNoneVerbose("sColourControlServerCluster.u16EnhancedCurrentHue: %d\n", sLight.sColourControlServerCluster.u16EnhancedCurrentHue);

			// nope: u16Primary1X, u16Primary1Y, u16Primary2X, u16Primary2Y, u16Primary3X, u16Primary3Y,
			//       u8Primary1Intensity, u8Primary2Intensity, u8Primary3Intensity

		} else if (byte == 'U') {
			if (sLight.sLevelControlServerCluster.u8CurrentLevel <= 254-step) {
				sLight.sLevelControlServerCluster.u8CurrentLevel += step;
//				eCLD_ColourControlUpdate(LIGHT_COLORLIGHT_LIGHT_00_ENDPOINT);

			}
		}
	}
}

void UART_init() {
	bAHI_UartEnable(E_AHI_UART_1, tx_buffer, 81, rx_buffer, 81);
	vAHI_UartSetBaudRate(E_AHI_UART_1, E_AHI_UART_RATE_115200);
	vAHI_UartSetControl(E_AHI_UART_1, FALSE, FALSE, E_AHI_UART_WORD_LEN_8, TRUE, FALSE);

	u16AHI_UartBlockWriteData(E_AHI_UART_1, (uint8_t*)"INIT\n", 5);

	vAHI_UartSetInterrupt(E_AHI_UART_1, FALSE, FALSE, FALSE, TRUE, E_AHI_UART_FIFO_LEVEL_1);
	vAHI_Uart1RegisterCallback(UART_callback);
}

uint16_t serialize_fill(const tsZLL_ColourLightDevice *light, uint8_t *buffer) {
    sprint_b(buffer+8, light->sOnOffServerCluster.bOnOff);
    sprint_x8(buffer+16, light->sLevelControlServerCluster.u8CurrentLevel);
    sprint_x16(buffer+21, light->sColourControlServerCluster.u16CurrentX);
    sprint_x16(buffer+28, light->sColourControlServerCluster.u16CurrentY);
    sprint_x8(buffer+37, light->sColourControlServerCluster.u8CurrentHue);
    sprint_x8(buffer+51, light->sColourControlServerCluster.u8CurrentSaturation);
    sprint_x16(buffer+66, light->sColourControlServerCluster.u16EnhancedCurrentHue);
    return sizeof(msg)-1;
}

void UART_send_state(const tsZLL_ColourLightDevice *light) {
	u16AHI_UartBlockWriteData(E_AHI_UART_1, msg, serialize_fill(light, msg));
}
