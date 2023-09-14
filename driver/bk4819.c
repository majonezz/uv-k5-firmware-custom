/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include "bk4819.h"
#include "bsp/dp32g030/gpio.h"
#include "bsp/dp32g030/portcon.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/systick.h"

static const uint16_t FSK_RogerTable[7] = {0xF1A2, 0x7446, 0x61A4, 0x6544, 0x4E8A, 0xE044, 0xEA84};

static uint16_t gBK4819_GpioOutState;

bool gRxIdleMode;

__inline uint16_t scale_freq(const uint16_t freq)
{
//	return (uint16_t)(freq * 10.32444);   // argh - floating point
//	return ((uint32_t)freq * 1032444u) / 100000u;
	return ((uint32_t)freq * 1353245u) >> 17;
}

void BK4819_Init(void)
{
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);

	BK4819_WriteRegister(BK4819_REG_00, 0x8000);
	BK4819_WriteRegister(BK4819_REG_00, 0x0000);
	BK4819_WriteRegister(BK4819_REG_37, 0x1D0F);
	BK4819_WriteRegister(BK4819_REG_36, 0x0022);
	BK4819_SetAGC(0);
	BK4819_WriteRegister(BK4819_REG_19, 0x1041);
	BK4819_WriteRegister(BK4819_REG_7D, 0xE940);
	BK4819_WriteRegister(BK4819_REG_48, 0xB3A8);
	BK4819_WriteRegister(BK4819_REG_09, 0x006F);
	BK4819_WriteRegister(BK4819_REG_09, 0x106B);
	BK4819_WriteRegister(BK4819_REG_09, 0x2067);
	BK4819_WriteRegister(BK4819_REG_09, 0x3062);
	BK4819_WriteRegister(BK4819_REG_09, 0x4050);
	BK4819_WriteRegister(BK4819_REG_09, 0x5047);
	BK4819_WriteRegister(BK4819_REG_09, 0x603A);
	BK4819_WriteRegister(BK4819_REG_09, 0x702C);
	BK4819_WriteRegister(BK4819_REG_09, 0x8041);
	BK4819_WriteRegister(BK4819_REG_09, 0x9037);
	BK4819_WriteRegister(BK4819_REG_09, 0xA025);
	BK4819_WriteRegister(BK4819_REG_09, 0xB017);
	BK4819_WriteRegister(BK4819_REG_09, 0xC0E4);
	BK4819_WriteRegister(BK4819_REG_09, 0xD0CB);
	BK4819_WriteRegister(BK4819_REG_09, 0xE0B5);
	BK4819_WriteRegister(BK4819_REG_09, 0xF09F);
	BK4819_WriteRegister(BK4819_REG_1F, 0x5454);
	BK4819_WriteRegister(BK4819_REG_3E, 0xA037);
	gBK4819_GpioOutState = 0x9000;
	BK4819_WriteRegister(BK4819_REG_33, 0x9000);
	BK4819_WriteRegister(BK4819_REG_3F, 0);
}

static uint16_t BK4819_ReadU16(void)
{
	unsigned int i;
	uint16_t     Value;

	PORTCON_PORTC_IE = (PORTCON_PORTC_IE & ~PORTCON_PORTC_IE_C2_MASK) | PORTCON_PORTC_IE_C2_BITS_ENABLE;
	GPIOC->DIR = (GPIOC->DIR & ~GPIO_DIR_2_MASK) | GPIO_DIR_2_BITS_INPUT;
	SYSTICK_DelayUs(1);

	Value = 0;
	for (i = 0; i < 16; i++)
	{
		Value <<= 1;
		Value |= GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1);
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1);
	}
	PORTCON_PORTC_IE = (PORTCON_PORTC_IE & ~PORTCON_PORTC_IE_C2_MASK) | PORTCON_PORTC_IE_C2_BITS_DISABLE;
	GPIOC->DIR = (GPIOC->DIR & ~GPIO_DIR_2_MASK) | GPIO_DIR_2_BITS_OUTPUT;

	return Value;
}

uint16_t BK4819_ReadRegister(BK4819_REGISTER_t Register)
{
	uint16_t Value;

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);

	SYSTICK_DelayUs(1);

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	BK4819_WriteU8(Register | 0x80);
	Value = BK4819_ReadU16();
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);

	SYSTICK_DelayUs(1);

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);

	return Value;
}

void BK4819_WriteRegister(BK4819_REGISTER_t Register, uint16_t Data)
{
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);

	SYSTICK_DelayUs(1);

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	BK4819_WriteU8(Register);

	SYSTICK_DelayUs(1);

	BK4819_WriteU16(Data);

	SYSTICK_DelayUs(1);

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);

	SYSTICK_DelayUs(1);

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
}

void BK4819_WriteU8(uint8_t Data)
{
	unsigned int i;

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	for (i = 0; i < 8; i++)
	{
		if ((Data & 0x80) == 0)
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
		else
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);

		SYSTICK_DelayUs(1);
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1);

		Data <<= 1;

		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1);
	}
}

void BK4819_WriteU16(uint16_t Data)
{
	unsigned int i;

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	for (i = 0; i < 16; i++)
	{
		if ((Data & 0x8000) == 0)
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
		else
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);

		SYSTICK_DelayUs(1);
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);

		Data <<= 1;

		SYSTICK_DelayUs(1);
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1);
	}
}

void BK4819_SetAGC(uint8_t Value)
{
	if (Value == 0)
	{
		BK4819_WriteRegister(BK4819_REG_13, 0x03BE);
		BK4819_WriteRegister(BK4819_REG_12, 0x037B);
		BK4819_WriteRegister(BK4819_REG_11, 0x027B);
		BK4819_WriteRegister(BK4819_REG_10, 0x007A);
		BK4819_WriteRegister(BK4819_REG_14, 0x0019);
		BK4819_WriteRegister(BK4819_REG_49, 0x2A38);
		BK4819_WriteRegister(BK4819_REG_7B, 0x8420);
	}
	else
	if (Value == 1)
	{
		unsigned int i;
		BK4819_WriteRegister(BK4819_REG_13, 0x03BE);
		BK4819_WriteRegister(BK4819_REG_12, 0x037C);
		BK4819_WriteRegister(BK4819_REG_11, 0x027B);
		BK4819_WriteRegister(BK4819_REG_10, 0x007A);
		BK4819_WriteRegister(BK4819_REG_14, 0x0018);
		BK4819_WriteRegister(BK4819_REG_49, 0x2A38);
		BK4819_WriteRegister(BK4819_REG_7B, 0x318C);
		BK4819_WriteRegister(BK4819_REG_7C, 0x595E);
		BK4819_WriteRegister(BK4819_REG_20, 0x8DEF);
		for (i = 0; i < 8; i++)
			// Bug? The bit 0x2000 below overwrites the (i << 13)
			BK4819_WriteRegister(BK4819_REG_06, ((i << 13) | 0x2500u) + 0x036u);
	}
}

void BK4819_ToggleGpioOut(BK4819_GPIO_PIN_t Pin, bool bSet)
{
	if (bSet)
		gBK4819_GpioOutState |=  (0x40u >> Pin);
	else
		gBK4819_GpioOutState &= ~(0x40u >> Pin);

	BK4819_WriteRegister(BK4819_REG_33, gBK4819_GpioOutState);
}

void BK4819_SetCDCSSCodeWord(uint32_t CodeWord)
{
	// REG_51 <15>  0                                 1 = Enable TxCTCSS/CDCSS           0 = Disable
	// REG_51 <14>  0                                 1 = GPIO0Input for CDCSS           0 = Normal Mode.(for BK4819v3)
	// REG_51 <13>  0                                 1 = Transmit negative CDCSS code   0 = Transmit positive CDCSScode
	// REG_51 <12>  0 CTCSS/CDCSS mode selection      1 = CTCSS                          0 = CDCSS
	// REG_51 <11>  0 CDCSS 24/23bit selection        1 = 24bit                          0 = 23bit
	// REG_51 <10>  0 1050HzDetectionMode             1 = 1050/4 Detect Enable, CTC1 should be set to 1050/4 Hz
	// REG_51 <9>   0 Auto CDCSS Bw Mode              1 = Disable                        0 = Enable.
	// REG_51 <8>   0 Auto CTCSS Bw Mode              0 = Enable                         1 = Disable
	// REG_51 <6:0> 0 CTCSS/CDCSS Tx Gain1 Tuning     0 = min                            127 = max

	// Enable CDCSS
	// Transmit positive CDCSS code
	// CDCSS Mode
	// CDCSS 23bit
	// Enable Auto CDCSS Bw Mode
	// Enable Auto CTCSS Bw Mode
	// CTCSS/CDCSS Tx Gain1 Tuning = 51
	//
	BK4819_WriteRegister(BK4819_REG_51,
			  BK4819_REG_51_ENABLE_CxCSS
			| BK4819_REG_51_GPIO6_PIN2_NORMAL
			| BK4819_REG_51_TX_CDCSS_POSITIVE
			| BK4819_REG_51_MODE_CDCSS
			| BK4819_REG_51_CDCSS_23_BIT
			| BK4819_REG_51_1050HZ_NO_DETECTION
			| BK4819_REG_51_AUTO_CDCSS_BW_ENABLE
			| BK4819_REG_51_AUTO_CTCSS_BW_ENABLE
			| (51u << BK4819_REG_51_SHIFT_CxCSS_TX_GAIN1));

	// REG_07 <15:0>
	//
	// When <13> = 0 for CTC1
	// <12:0> = CTC1 frequency control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 1 for CTC2 (Tail 55Hz Rx detection)
	// <12:0> = CTC2 (should below 100Hz) frequency control word =
	//                          25391 / freq(Hz) for XTAL 13M/26M or
	//                          25000 / freq(Hz) for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 2 for CDCSS 134.4Hz
	// <12:0> = CDCSS baud rate frequency (134.4Hz) control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC1 | 2775u);

	// REG_08 <15:0> <15> = 1 for CDCSS high 12bit
	//               <15> = 0 for CDCSS low  12bit
	// <11:0> = CDCSShigh/low 12bit code
	//
	BK4819_WriteRegister(BK4819_REG_08, (0u << 15) | ((CodeWord >>  0) & 0x0FFF)); // LS 12-bits
	BK4819_WriteRegister(BK4819_REG_08, (1u << 15) | ((CodeWord >> 12) & 0x0FFF)); // MS 12-bits
}

void BK4819_SetCTCSSFrequency(uint32_t FreqControlWord)
{
	// REG_51 <15>  0                                 1 = Enable TxCTCSS/CDCSS           0 = Disable
	// REG_51 <14>  0                                 1 = GPIO0Input for CDCSS           0 = Normal Mode.(for BK4819v3)
	// REG_51 <13>  0                                 1 = Transmit negative CDCSS code   0 = Transmit positive CDCSScode
	// REG_51 <12>  0 CTCSS/CDCSS mode selection      1 = CTCSS                          0 = CDCSS
	// REG_51 <11>  0 CDCSS 24/23bit selection        1 = 24bit                          0 = 23bit
	// REG_51 <10>  0 1050HzDetectionMode             1 = 1050/4 Detect Enable, CTC1 should be set to 1050/4 Hz
	// REG_51 <9>   0 Auto CDCSS Bw Mode              1 = Disable                        0 = Enable.
	// REG_51 <8>   0 Auto CTCSS Bw Mode              0 = Enable                         1 = Disable
	// REG_51 <6:0> 0 CTCSS/CDCSS Tx Gain1 Tuning     0 = min                            127 = max

	uint16_t Config;
	if (FreqControlWord == 2625)
	{	// Enables 1050Hz detection mode
		// Enable TxCTCSS
		// CTCSS Mode
		// 1050/4 Detect Enable
		// Enable Auto CDCSS Bw Mode
		// Enable Auto CTCSS Bw Mode
		// CTCSS/CDCSS Tx Gain1 Tuning = 74
		//
		Config = 0x944A;   // 1 0 0 1 0 1 0 0 0 1001010
	}
	else
	{	// Enable TxCTCSS
		// CTCSS Mode
		// Enable Auto CDCSS Bw Mode
		// Enable Auto CTCSS Bw Mode
		// CTCSS/CDCSS Tx Gain1 Tuning = 74
		//
		Config = 0x904A;   // 1 0 0 1 0 0 0 0 0 1001010
	}
	BK4819_WriteRegister(BK4819_REG_51, Config);
	
	// REG_07 <15:0>
	//
	// When <13> = 0 for CTC1
	// <12:0> = CTC1 frequency control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 1 for CTC2 (Tail RX detection)
	// <12:0> = CTC2 (should below 100Hz) frequency control word =
	//                          25391 / freq(Hz) for XTAL 13M/26M or
	//                          25000 / freq(Hz) for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 2 for CDCSS 134.4Hz
	// <12:0> = CDCSS baud rate frequency (134.4Hz) control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC1 | (((FreqControlWord * 2064888u) + 500000u) / 1000000u));   // with rounding
}

// freq_10Hz is CTCSS Hz * 10 
void BK4819_SetTailDetection(const uint32_t freq_10Hz)
{
	// REG_07 <15:0>
	//
	// When <13> = 0 for CTC1
	// <12:0> = CTC1 frequency control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 1 for CTC2 (Tail RX detection)
	// <12:0> = CTC2 (should below 100Hz) frequency control word =
	//                          25391 / freq(Hz) for XTAL 13M/26M or
	//                          25000 / freq(Hz) for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 2 for CDCSS 134.4Hz
	// <12:0> = CDCSS baud rate frequency (134.4Hz) control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC2 | ((253910 + (freq_10Hz / 2)) / freq_10Hz));  // with rounding
}

void BK4819_EnableVox(uint16_t VoxEnableThreshold, uint16_t VoxDisableThreshold)
{
	//VOX Algorithm
	//if (voxamp>VoxEnableThreshold)                VOX = 1;
	//else
	//if (voxamp<VoxDisableThreshold) (After Delay) VOX = 0;

	const uint16_t REG_31_Value = BK4819_ReadRegister(BK4819_REG_31);

	// 0xA000 is undocumented?
	BK4819_WriteRegister(BK4819_REG_46, 0xA000 | (VoxEnableThreshold & 0x07FF));

	// 0x1800 is undocumented?
	BK4819_WriteRegister(BK4819_REG_79, 0x1800 | (VoxDisableThreshold & 0x07FF));

	// Bottom 12 bits are undocumented, 15:12 vox disable delay *128ms
	BK4819_WriteRegister(BK4819_REG_7A, 0x289A); // vox disable delay = 128*5 = 640ms

	// Enable VOX
	BK4819_WriteRegister(BK4819_REG_31, REG_31_Value | 4u); // bit 2 - VOX Enable
}

void BK4819_SetFilterBandwidth(BK4819_FilterBandwidth_t Bandwidth)
{
	if (Bandwidth == BK4819_FILTER_BW_WIDE)
		BK4819_WriteRegister(BK4819_REG_43, 0x3028);
	else
	if (Bandwidth == BK4819_FILTER_BW_NARROW)
		BK4819_WriteRegister(BK4819_REG_43, 0x4048);
		//BK4819_WriteRegister(BK4819_REG_43, 0x790C);	// fastest squelch, https://github.com/fagci/uv-k5-firmware-fagci-mod
}

void BK4819_SetupPowerAmplifier(uint16_t Bias, uint32_t Frequency)
{
	uint8_t Gain;

	if (Bias > 255)
		Bias = 255;

	if (Frequency < 28000000)
	{
		// Gain 1 = 1
		// Gain 2 = 0
		Gain = 0x08U;
	}
	else
	{
		// Gain 1 = 4
		// Gain 2 = 2
		Gain = 0x22U;
	}

	// Enable PACTLoutput
	BK4819_WriteRegister(BK4819_REG_36, (Bias << 8) | 0x80U | Gain);
}

void BK4819_SetFrequency(uint32_t Frequency)
{
	BK4819_WriteRegister(BK4819_REG_38, (Frequency >>  0) & 0xFFFF);
	BK4819_WriteRegister(BK4819_REG_39, (Frequency >> 16) & 0xFFFF);
}

void BK4819_SetupSquelch(uint8_t SquelchOpenRSSIThresh, uint8_t SquelchCloseRSSIThresh, uint8_t SquelchOpenNoiseThresh, uint8_t SquelchCloseNoiseThresh, uint8_t SquelchCloseGlitchThresh, uint8_t SquelchOpenGlitchThresh)
{
	BK4819_WriteRegister(BK4819_REG_70, 0);

	#if 1
		BK4819_WriteRegister(BK4819_REG_4D, 0xA000 | SquelchCloseGlitchThresh);
	#else
		// fastest squelch, https://github.com/fagci/uv-k5-firmware-fagci-mod this doesn't work
		BK4819_WriteRegister(BK4819_REG_4D, 0b01000000 | SquelchCloseGlitchThresh);
	#endif
	
	// 0x6f = 0110 1111 meaning the default sql delays from the datasheet are used (101 and 111)
	BK4819_WriteRegister(BK4819_REG_4E, 0x6F00 | SquelchOpenGlitchThresh);
	BK4819_WriteRegister(BK4819_REG_4F, (SquelchCloseNoiseThresh << 8) | SquelchOpenNoiseThresh);
	BK4819_WriteRegister(BK4819_REG_78, (SquelchOpenRSSIThresh   << 8) | SquelchCloseRSSIThresh);

	BK4819_SetAF(BK4819_AF_MUTE);

	BK4819_RX_TurnOn();
}

void BK4819_SetAF(BK4819_AF_Type_t AF)
{
	// AF Output Inverse Mode = Inverse
	// Undocumented bits 0x2040
	BK4819_WriteRegister(BK4819_REG_47, 0x6040 | (AF << 8));
}

void BK4819_RX_TurnOn(void)
{
	// DSP Voltage Setting = 1
	// ANA LDO = 2.7v
	// VCO LDO = 2.7v
	// RF LDO = 2.7v
	// PLL LDO = 2.7v
	// ANA LDO bypass
	// VCO LDO bypass
	// RF LDO bypass
	// PLL LDO bypass
	// Reserved bit is 1 instead of 0
	// Enable DSP
	// Enable XTAL
	// Enable Band Gap
	BK4819_WriteRegister(BK4819_REG_37, 0x1F0F);

	// Turn off everything
	BK4819_WriteRegister(BK4819_REG_30, 0);

	// Enable VCO Calibration
	// Enable RX Link
	// Enable AF DAC
	// Enable PLL/VCO
	// Disable PA Gain
	// Disable MIC ADC
	// Disable TX DSP
	// Enable RX DSP
	BK4819_WriteRegister(BK4819_REG_30, 0xBFF1);
}

void BK4819_PickRXFilterPathBasedOnFrequency(uint32_t Frequency)
{
	if (Frequency < 28000000)
	{
		BK4819_ToggleGpioOut(BK4819_GPIO2_PIN30, true);
		BK4819_ToggleGpioOut(BK4819_GPIO3_PIN31, false);
	}
	else
	if (Frequency == 0xFFFFFFFF)
	{
		BK4819_ToggleGpioOut(BK4819_GPIO2_PIN30, false);
		BK4819_ToggleGpioOut(BK4819_GPIO3_PIN31, false);
	}
	else
	{
		BK4819_ToggleGpioOut(BK4819_GPIO2_PIN30, false);
		BK4819_ToggleGpioOut(BK4819_GPIO3_PIN31, true);
	}
}

void BK4819_DisableScramble(void)
{
	const uint16_t Value = BK4819_ReadRegister(BK4819_REG_31);
	BK4819_WriteRegister(BK4819_REG_31, Value & 0xFFFD);
}

void BK4819_EnableScramble(uint8_t Type)
{
	const uint16_t Value = BK4819_ReadRegister(BK4819_REG_31);
	BK4819_WriteRegister(BK4819_REG_31, Value | 2u);
	BK4819_WriteRegister(BK4819_REG_71, 0x68DC + (Type * 1032));
}

void BK4819_DisableVox(void)
{
	const uint16_t Value = BK4819_ReadRegister(BK4819_REG_31);
	BK4819_WriteRegister(BK4819_REG_31, Value & 0xFFFB);
}

void BK4819_DisableDTMF(void)
{
	BK4819_WriteRegister(BK4819_REG_24, 0);
}

void BK4819_EnableDTMF(void)
{
	BK4819_WriteRegister(BK4819_REG_21, 0x06D8);
	BK4819_WriteRegister(BK4819_REG_24,
		  (1U << BK4819_REG_24_SHIFT_UNKNOWN_15)
		| (24 << BK4819_REG_24_SHIFT_THRESHOLD)
		| (1U << BK4819_REG_24_SHIFT_UNKNOWN_6)
		| BK4819_REG_24_ENABLE
		| BK4819_REG_24_SELECT_DTMF
		| (14U << BK4819_REG_24_SHIFT_MAX_SYMBOLS));
}

void BK4819_PlayTone(uint16_t Frequency, bool bTuningGainSwitch)
{
	uint16_t ToneConfig;

	BK4819_EnterTxMute();
	BK4819_SetAF(BK4819_AF_BEEP);

	if (bTuningGainSwitch == 0)
		ToneConfig = BK4819_REG_70_ENABLE_TONE1 | (96U << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN);
	else
		ToneConfig = BK4819_REG_70_ENABLE_TONE1 | (28U << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN);
	BK4819_WriteRegister(BK4819_REG_70, ToneConfig);

	BK4819_WriteRegister(BK4819_REG_30, 0);
	BK4819_WriteRegister(BK4819_REG_30, BK4819_REG_30_ENABLE_AF_DAC | BK4819_REG_30_ENABLE_DISC_MODE | BK4819_REG_30_ENABLE_TX_DSP);

	BK4819_WriteRegister(BK4819_REG_71, scale_freq(Frequency));
}

void BK4819_EnterTxMute(void)
{
	BK4819_WriteRegister(BK4819_REG_50, 0xBB20);
}

void BK4819_ExitTxMute(void)
{
	BK4819_WriteRegister(BK4819_REG_50, 0x3B20);
}

void BK4819_Sleep(void)
{
	BK4819_WriteRegister(BK4819_REG_30, 0);
	BK4819_WriteRegister(BK4819_REG_37, 0x1D00);
}

void BK4819_TurnsOffTones_TurnsOnRX(void)
{
	BK4819_WriteRegister(BK4819_REG_70, 0);
	BK4819_SetAF(BK4819_AF_MUTE);

	BK4819_ExitTxMute();

	BK4819_WriteRegister(BK4819_REG_30, 0);
	BK4819_WriteRegister(BK4819_REG_30,
			0
			| BK4819_REG_30_ENABLE_VCO_CALIB
			| BK4819_REG_30_ENABLE_RX_LINK
			| BK4819_REG_30_ENABLE_AF_DAC
			| BK4819_REG_30_ENABLE_DISC_MODE
			| BK4819_REG_30_ENABLE_PLL_VCO
			| BK4819_REG_30_ENABLE_RX_DSP);
}

#ifdef ENABLE_AIRCOPY
	void BK4819_SetupAircopy(void)
	{
		BK4819_WriteRegister(BK4819_REG_70, 0x00E0);    // Enable Tone2, tuning gain 48
		BK4819_WriteRegister(BK4819_REG_72, 0x3065);    // Tone2 baudrate 1200
		BK4819_WriteRegister(BK4819_REG_58, 0x00C1);    // FSK Enable, FSK 1.2K RX Bandwidth, Preamble 0xAA or 0x55, RX Gain 0, RX Mode
		                                                // (FSK1.2K, FSK2.4K Rx and NOAA SAME Rx), TX Mode FSK 1.2K and FSK 2.4K Tx
		BK4819_WriteRegister(BK4819_REG_5C, 0x5665);    // Enable CRC among other things we don't know yet
		BK4819_WriteRegister(BK4819_REG_5D, 0x4700);    // FSK Data Length 72 Bytes (0xabcd + 2 byte length + 64 byte payload + 2 byte CRC + 0xdcba)
	}
#endif

void BK4819_ResetFSK(void)
{
	BK4819_WriteRegister(BK4819_REG_3F, 0x0000);        // Disable interrupts
	BK4819_WriteRegister(BK4819_REG_59, 0x0068);        // Sync length 4 bytes, 7 byte preamble

	SYSTEM_DelayMs(30);

	BK4819_Idle();
}

void BK4819_Idle(void)
{
	BK4819_WriteRegister(BK4819_REG_30, 0x0000);
}

void BK4819_ExitBypass(void)
{
	BK4819_SetAF(BK4819_AF_MUTE);
	BK4819_WriteRegister(BK4819_REG_7E, 0x302E);
}

void BK4819_PrepareTransmit(void)
{
	BK4819_ExitBypass();
	BK4819_ExitTxMute();
	BK4819_TxOn_Beep();
}

void BK4819_TxOn_Beep(void)
{
	BK4819_WriteRegister(BK4819_REG_37, 0x1D0F);
	BK4819_WriteRegister(BK4819_REG_52, 0x028F);
	BK4819_WriteRegister(BK4819_REG_30, 0x0000);
	BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
}

void BK4819_ExitSubAu(void)
{
	// REG_51 <15>  0                                 1 = Enable TxCTCSS/CDCSS           0 = Disable
	// REG_51 <14>  0                                 1 = GPIO0Input for CDCSS           0 = Normal Mode.(for BK4819v3)
	// REG_51 <13>  0                                 1 = Transmit negative CDCSS code   0 = Transmit positive CDCSScode
	// REG_51 <12>  0 CTCSS/CDCSS mode selection      1 = CTCSS                          0 = CDCSS
	// REG_51 <11>  0 CDCSS 24/23bit selection        1 = 24bit                          0 = 23bit
	// REG_51 <10>  0 1050HzDetectionMode             1 = 1050/4 Detect Enable, CTC1 should be set to 1050/4 Hz
	// REG_51 <9>   0 Auto CDCSS Bw Mode              1 = Disable                        0 = Enable.
	// REG_51 <8>   0 Auto CTCSS Bw Mode              0 = Enable                         1 = Disable
	// REG_51 <6:0> 0 CTCSS/CDCSS Tx Gain1 Tuning     0 = min                            127 = max
	//
	BK4819_WriteRegister(BK4819_REG_51, 0x0000);
}

void BK4819_Conditional_RX_TurnOn_and_GPIO6_Enable(void)
{
	if (gRxIdleMode)
	{
		BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2, true);
		BK4819_RX_TurnOn();
	}
}

void BK4819_EnterDTMF_TX(bool bLocalLoopback)
{
	BK4819_EnableDTMF();
	BK4819_EnterTxMute();
	BK4819_SetAF(bLocalLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);
	BK4819_WriteRegister(BK4819_REG_70,
		0
		| BK4819_REG_70_MASK_ENABLE_TONE1
		| (83 << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN)
		| BK4819_REG_70_MASK_ENABLE_TONE2
		| (83 << BK4819_REG_70_SHIFT_TONE2_TUNING_GAIN));

	BK4819_EnableTXLink();
}

void BK4819_ExitDTMF_TX(bool bKeep)
{
	BK4819_EnterTxMute();
	BK4819_SetAF(BK4819_AF_MUTE);
	BK4819_WriteRegister(BK4819_REG_70, 0x0000);
	BK4819_DisableDTMF();
	BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
	if (!bKeep)
		BK4819_ExitTxMute();
}

void BK4819_EnableTXLink(void)
{
	BK4819_WriteRegister(BK4819_REG_30,
		0
		| BK4819_REG_30_ENABLE_VCO_CALIB
		| BK4819_REG_30_ENABLE_UNKNOWN
		| BK4819_REG_30_DISABLE_RX_LINK
		| BK4819_REG_30_ENABLE_AF_DAC
		| BK4819_REG_30_ENABLE_DISC_MODE
		| BK4819_REG_30_ENABLE_PLL_VCO
		| BK4819_REG_30_ENABLE_PA_GAIN
		| BK4819_REG_30_DISABLE_MIC_ADC
		| BK4819_REG_30_ENABLE_TX_DSP
		| BK4819_REG_30_DISABLE_RX_DSP);
}

void BK4819_PlayDTMF(char Code)
{
	switch (Code)
	{
		case '0':
			BK4819_WriteRegister(BK4819_REG_71, 0x25F3);
			BK4819_WriteRegister(BK4819_REG_72, 0x35E1);
			break;
		case '1':
			BK4819_WriteRegister(BK4819_REG_71, 0x1C1C);
			BK4819_WriteRegister(BK4819_REG_72, 0x30C2);
			break;
		case '2':
			BK4819_WriteRegister(BK4819_REG_71, 0x1C1C);
			BK4819_WriteRegister(BK4819_REG_72, 0x35E1);
			break;
		case '3':
			BK4819_WriteRegister(BK4819_REG_71, 0x1C1C);
			BK4819_WriteRegister(BK4819_REG_72, 0x3B91);
			break;
		case '4':
			BK4819_WriteRegister(BK4819_REG_71, 0x1F0E);
			BK4819_WriteRegister(BK4819_REG_72, 0x30C2);
			break;
		case '5':
			BK4819_WriteRegister(BK4819_REG_71, 0x1F0E);
			BK4819_WriteRegister(BK4819_REG_72, 0x35E1);
			break;
		case '6':
			BK4819_WriteRegister(BK4819_REG_71, 0x1F0E);
			BK4819_WriteRegister(BK4819_REG_72, 0x3B91);
			break;
		case '7':
			BK4819_WriteRegister(BK4819_REG_71, 0x225C);
			BK4819_WriteRegister(BK4819_REG_72, 0x30C2);
			break;
		case '8':
			BK4819_WriteRegister(BK4819_REG_71, 0x225c);
			BK4819_WriteRegister(BK4819_REG_72, 0x35E1);
			break;
		case '9':
			BK4819_WriteRegister(BK4819_REG_71, 0x225C);
			BK4819_WriteRegister(BK4819_REG_72, 0x3B91);
			break;
		case 'A':
			BK4819_WriteRegister(BK4819_REG_71, 0x1C1C);
			BK4819_WriteRegister(BK4819_REG_72, 0x41DC);
			break;
		case 'B':
			BK4819_WriteRegister(BK4819_REG_71, 0x1F0E);
			BK4819_WriteRegister(BK4819_REG_72, 0x41DC);
			break;
		case 'C':
			BK4819_WriteRegister(BK4819_REG_71, 0x225C);
			BK4819_WriteRegister(BK4819_REG_72, 0x41DC);
			break;
		case 'D':
			BK4819_WriteRegister(BK4819_REG_71, 0x25F3);
			BK4819_WriteRegister(BK4819_REG_72, 0x41DC);
			break;
		case '*':
			BK4819_WriteRegister(BK4819_REG_71, 0x25F3);
			BK4819_WriteRegister(BK4819_REG_72, 0x30C2);
			break;
		case '#':
			BK4819_WriteRegister(BK4819_REG_71, 0x25F3);
			BK4819_WriteRegister(BK4819_REG_72, 0x3B91);
			break;
	}
}

void BK4819_PlayDTMFString(const char *pString, bool bDelayFirst, uint16_t FirstCodePersistTime, uint16_t HashCodePersistTime, uint16_t CodePersistTime, uint16_t CodeInternalTime)
{
	unsigned int i;
	for (i = 0; pString[i]; i++)
	{
		uint16_t Delay;
		BK4819_PlayDTMF(pString[i]);
		BK4819_ExitTxMute();
		if (bDelayFirst && i == 0)
			Delay = FirstCodePersistTime;
		else
		if (pString[i] == '*' || pString[i] == '#')
			Delay = HashCodePersistTime;
		else
			Delay = CodePersistTime;
		SYSTEM_DelayMs(Delay);
		BK4819_EnterTxMute();
		SYSTEM_DelayMs(CodeInternalTime);
	}
}

void BK4819_TransmitTone(bool bLocalLoopback, uint32_t Frequency)
{
	BK4819_EnterTxMute();

	BK4819_WriteRegister(BK4819_REG_70, 0 | BK4819_REG_70_MASK_ENABLE_TONE1 | (96U << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));
	BK4819_WriteRegister(BK4819_REG_71, scale_freq(Frequency));

	BK4819_SetAF(bLocalLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);

	BK4819_EnableTXLink();

	SYSTEM_DelayMs(50);
	
	BK4819_ExitTxMute();
}

void BK4819_GenTail(uint8_t Tail)
{
	// REG_52 <15>    0 Enable 120/180/240 degree shift CTCSS or 134.4Hz Tail when CDCSS mode
	//                0 = Normal
	//                1 = Enable
	// REG_52 <14:13> 0 CTCSS tail mode selection (only valid when REG_52 <15> = 1)
	//                00 = for 134.4Hz CTCSS Tail when CDCSS mode
	//                01 = CTCSS0 120° phase shift
	//                10 = CTCSS0 180° phase shift
	//                11 = CTCSS0 240° phase shift
	// REG_52 <12>    0 CTCSSDetectionThreshold Mode
	//                1 = ~0.1%
	//                0 =  0.1 Hz
	// REG_52 <11:6>  0x0A CTCSS found detect threshold
	// REG_52 <5:0>   0x0F CTCSS lost  detect threshold

	// REG_07 <15:0>
	//
	// When <13> = 0 for CTC1
	// <12:0> = CTC1 frequency control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 1 for CTC2 (Tail 55Hz Rx detection)
	// <12:0> = CTC2 (should below 100Hz) frequency control word =
	//                          25391 / freq(Hz) for XTAL 13M/26M or
	//                          25000 / freq(Hz) for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 2 for CDCSS 134.4Hz
	// <12:0> = CDCSS baud rate frequency (134.4Hz) control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz)*20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M

	switch (Tail)
	{
		case 0: // 134.4Hz CTCSS Tail
			BK4819_WriteRegister(BK4819_REG_52, 0x828F);   // 1 00 0 001010 001111
			break;
		case 1: // 120° phase shift
			BK4819_WriteRegister(BK4819_REG_52, 0xA28F);   // 1 01 0 001010 001111
			break;
		case 2: // 180° phase shift
			BK4819_WriteRegister(BK4819_REG_52, 0xC28F);   // 1 10 0 001010 001111
			break;
		case 3: // 240° phase shift
			BK4819_WriteRegister(BK4819_REG_52, 0xE28F);   // 1 11 0 001010 001111
			break;
		case 4: // 55Hz tone freq
			BK4819_WriteRegister(BK4819_REG_07, 0x046f);   // 0 00 0 010001 101111
			break;
	}
}

void BK4819_EnableCDCSS(void)
{
	BK4819_GenTail(0);     // CTC134
	BK4819_WriteRegister(BK4819_REG_51, 0x804A);
}

void BK4819_EnableCTCSS(void)
{
	#ifdef ENABLE_CTCSS_TAIL_PHASE_SHIFT
		//BK4819_GenTail(1);     // 120° phase shift
		BK4819_GenTail(2);       // 180° phase shift
		//BK4819_GenTail(3);     // 240° phase shift
	#else
		BK4819_GenTail(4);       // 55Hz tone freq
	#endif
	
	// REG_51 <15>  0
	//              1 = Enable TxCTCSS/CDCSS
	//              0 = Disable
	// REG_51 <14>  0
	//              1 = GPIO0Input for CDCSS
	//              0 = Normal Mode (for BK4819 v3)
	// REG_51 <13>  0
	//              1 = Transmit negative CDCSS code
	//              0 = Transmit positive CDCSS code
	// REG_51 <12>  0 CTCSS/CDCSS mode selection
	//              1 = CTCSS
	//              0 = CDCSS
	// REG_51 <11>  0 CDCSS 24/23bit selection
	//              1 = 24bit
	//              0 = 23bit
	// REG_51 <10>  0 1050HzDetectionMode
	//              1 = 1050/4 Detect Enable, CTC1 should be set to 1050/4 Hz
	// REG_51 <9>   0 Auto CDCSS Bw Mode
	//              1 = Disable
	//              0 = Enable
	// REG_51 <8>   0 Auto CTCSS Bw Mode
	//              0 = Enable
	//              1 = Disable
	// REG_51 <6:0> 0 CTCSS/CDCSS Tx Gain1 Tuning
	//              0   = min
	//              127 = max

	BK4819_WriteRegister(BK4819_REG_51, 0x904A); // 1 0 0 1 0 0 0 0 0 1001010
}

uint16_t BK4819_GetRSSI(void)
{
	return BK4819_ReadRegister(BK4819_REG_67) & 0x01FF;
}

bool BK4819_GetFrequencyScanResult(uint32_t *pFrequency)
{
	const uint16_t High     = BK4819_ReadRegister(BK4819_REG_0D);
	const bool     Finished = (High & 0x8000) == 0;
	if (Finished)
	{
		const uint16_t Low = BK4819_ReadRegister(BK4819_REG_0E);
		*pFrequency = (uint32_t)((High & 0x7FF) << 16) | Low;
	}
	return Finished;
}

BK4819_CssScanResult_t BK4819_GetCxCSSScanResult(uint32_t *pCdcssFreq, uint16_t *pCtcssFreq)
{
	uint16_t Low;
	uint16_t High = BK4819_ReadRegister(BK4819_REG_69);

	if ((High & 0x8000) == 0)
	{
		Low         = BK4819_ReadRegister(BK4819_REG_6A);
		*pCdcssFreq = ((High & 0xFFF) << 12) | (Low & 0xFFF);
		return BK4819_CSS_RESULT_CDCSS;
	}

	Low = BK4819_ReadRegister(BK4819_REG_68);

	if ((Low & 0x8000) == 0)
	{
		*pCtcssFreq = ((Low & 0x1FFF) * 4843) / 10000;
		return BK4819_CSS_RESULT_CTCSS;
	}

	return BK4819_CSS_RESULT_NOT_FOUND;
}

void BK4819_DisableFrequencyScan(void)
{
	BK4819_WriteRegister(BK4819_REG_32, 0x0244);
}

void BK4819_EnableFrequencyScan(void)
{
	BK4819_WriteRegister(BK4819_REG_32, 0x0245);
}

void BK4819_SetScanFrequency(uint32_t Frequency)
{
	BK4819_SetFrequency(Frequency);

	// REG_51 <15>  0                                 1 = Enable TxCTCSS/CDCSS           0 = Disable
	// REG_51 <14>  0                                 1 = GPIO0 Input for CDCSS          0 = Normal Mode.(for BK4819v3)
	// REG_51 <13>  0                                 1 = Transmit negative CDCSS code   0 = Transmit positive CDCSScode
	// REG_51 <12>  0 CTCSS/CDCSS mode selection      1 = CTCSS                          0 = CDCSS
	// REG_51 <11>  0 CDCSS 24/23bit selection        1 = 24bit                          0 = 23bit
	// REG_51 <10>  0 1050HzDetectionMode             1 = 1050/4 Detect Enable, CTC1 should be set to 1050/4 Hz
	// REG_51 <9>   0 Auto CDCSS Bw Mode              1 = Disable                        0 = Enable.
	// REG_51 <8>   0 Auto CTCSS Bw Mode              0 = Enable                         1 = Disable
	// REG_51 <6:0> 0 CTCSS/CDCSS Tx Gain1 Tuning     0 = min                            127 = max

	BK4819_WriteRegister(BK4819_REG_51, 0
		| BK4819_REG_51_DISABLE_CxCSS
		| BK4819_REG_51_GPIO6_PIN2_NORMAL
		| BK4819_REG_51_TX_CDCSS_POSITIVE
		| BK4819_REG_51_MODE_CDCSS
		| BK4819_REG_51_CDCSS_23_BIT
		| BK4819_REG_51_1050HZ_NO_DETECTION
		| BK4819_REG_51_AUTO_CDCSS_BW_DISABLE
		| BK4819_REG_51_AUTO_CTCSS_BW_DISABLE);

	BK4819_RX_TurnOn();
}

void BK4819_Disable(void)
{
	BK4819_WriteRegister(BK4819_REG_30, 0);
}

void BK4819_StopScan(void)
{
	BK4819_DisableFrequencyScan();
	BK4819_Disable();
}

uint8_t BK4819_GetDTMF_5TONE_Code(void)
{
	return (BK4819_ReadRegister(BK4819_REG_0B) >> 8) & 0x0F;
}

uint8_t BK4819_GetCDCSSCodeType(void)
{
	return (BK4819_ReadRegister(BK4819_REG_0C) >> 14) & 3;
}

uint8_t BK4819_GetCTCType(void)
{
	return (BK4819_ReadRegister(BK4819_REG_0C) >> 10) & 3;
}

void BK4819_SendFSKData(uint16_t *pData)
{
	unsigned int i;
	uint8_t Timeout = 200;

	SYSTEM_DelayMs(20);

	BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_3F_FSK_TX_FINISHED);
	BK4819_WriteRegister(BK4819_REG_59, 0x8068);
	BK4819_WriteRegister(BK4819_REG_59, 0x0068);

	for (i = 0; i < 36; i++)
		BK4819_WriteRegister(BK4819_REG_5F, pData[i]);

	SYSTEM_DelayMs(20);

	BK4819_WriteRegister(BK4819_REG_59, 0x2868);

	while (Timeout-- && (BK4819_ReadRegister(BK4819_REG_0C) & 1u) == 0)
		SYSTEM_DelayMs(5);

	BK4819_WriteRegister(BK4819_REG_02, 0);

	SYSTEM_DelayMs(20);

	BK4819_ResetFSK();
}

void BK4819_PrepareFSKReceive(void)
{
	BK4819_ResetFSK();
	BK4819_WriteRegister(BK4819_REG_02, 0);
	BK4819_WriteRegister(BK4819_REG_3F, 0);
	BK4819_RX_TurnOn();
	BK4819_WriteRegister(BK4819_REG_3F, 0 | BK4819_REG_3F_FSK_RX_FINISHED | BK4819_REG_3F_FSK_FIFO_ALMOST_FULL);

	// Clear RX FIFO
	// FSK Preamble Length 7 bytes
	// FSK SyncLength Selection
	BK4819_WriteRegister(BK4819_REG_59, 0x4068);

	// Enable FSK Scramble
	// Enable FSK RX
	// FSK Preamble Length 7 bytes
	// FSK SyncLength Selection
	BK4819_WriteRegister(BK4819_REG_59, 0x3068);
}

void BK4819_PlayRoger(void)
{
	BK4819_EnterTxMute();

	BK4819_SetAF(BK4819_AF_MUTE);
	BK4819_WriteRegister(BK4819_REG_70, 0xE000);

	BK4819_EnableTXLink();

	SYSTEM_DelayMs(50);

	BK4819_WriteRegister(BK4819_REG_71, 0x142A);
	BK4819_ExitTxMute();

	SYSTEM_DelayMs(80);

	BK4819_EnterTxMute();
	BK4819_WriteRegister(BK4819_REG_71, 0x1C3B);
	BK4819_ExitTxMute();

	SYSTEM_DelayMs(80);

	BK4819_EnterTxMute();

	BK4819_WriteRegister(BK4819_REG_70, 0x0000);
	BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
}

void BK4819_PlayRogerMDC(void)
{
	unsigned int i;

	BK4819_SetAF(BK4819_AF_MUTE);
	BK4819_WriteRegister(BK4819_REG_58, 0x37C3);   // FSK Enable, RX Bandwidth FFSK1200/1800, 0xAA or 0x55 Preamble, 11 RX Gain,
	                                               // 101 RX Mode, FFSK1200/1800 TX
	BK4819_WriteRegister(BK4819_REG_72, 0x3065);   // Set Tone2 to 1200Hz
	BK4819_WriteRegister(BK4819_REG_70, 0x00E0);   // Enable Tone2 and Set Tone2 Gain
	BK4819_WriteRegister(BK4819_REG_5D, 0x0D00);   // Set FSK data length to 13 bytes
	BK4819_WriteRegister(BK4819_REG_59, 0x8068);   // 4 byte sync length, 6 byte preamble, clear TX FIFO
	BK4819_WriteRegister(BK4819_REG_59, 0x0068);   // Same, but clear TX FIFO is now unset (clearing done)
	BK4819_WriteRegister(BK4819_REG_5A, 0x5555);   // First two sync bytes
	BK4819_WriteRegister(BK4819_REG_5B, 0x55AA);   // End of sync bytes. Total 4 bytes: 555555aa
	BK4819_WriteRegister(BK4819_REG_5C, 0xAA30);   // Disable CRC

	// Send the data from the roger table
	for (i = 0; i < 7; i++)
		BK4819_WriteRegister(BK4819_REG_5F, FSK_RogerTable[i]);

	SYSTEM_DelayMs(20);

	// 4 sync bytes, 6 byte preamble, Enable FSK TX
	BK4819_WriteRegister(BK4819_REG_59, 0x0868);

	SYSTEM_DelayMs(180);

	// Stop FSK TX, reset Tone2, disable FSK
	BK4819_WriteRegister(BK4819_REG_59, 0x0068);
	BK4819_WriteRegister(BK4819_REG_70, 0x0000);
	BK4819_WriteRegister(BK4819_REG_58, 0x0000);
}

void BK4819_Enable_AfDac_DiscMode_TxDsp(void)
{
	BK4819_WriteRegister(BK4819_REG_30, 0x0000);
	BK4819_WriteRegister(BK4819_REG_30, 0x0302);
}

void BK4819_GetVoxAmp(uint16_t *pResult)
{
	*pResult = BK4819_ReadRegister(BK4819_REG_64) & 0x7FFF;
}

void BK4819_SetScrambleFrequencyControlWord(uint32_t Frequency)
{
	BK4819_WriteRegister(BK4819_REG_71, scale_freq(Frequency));
}

void BK4819_PlayDTMFEx(bool bLocalLoopback, char Code)
{
	BK4819_EnableDTMF();
	BK4819_EnterTxMute();

	BK4819_SetAF(bLocalLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);
	BK4819_WriteRegister(BK4819_REG_70, 0xD3D3);

	BK4819_EnableTXLink();

	SYSTEM_DelayMs(50);

	BK4819_PlayDTMF(Code);

	BK4819_ExitTxMute();
}
