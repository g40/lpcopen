/*
 * @brief Startup file for SPIFI programmer example
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2012
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include <string.h>

#include "flash_config.h"

/** @defgroup EXAMPLES_MISC_17XX40XX_SPIFI_PROGRAMMER LPC17xx/40xx SPIFI Flash programmer example
 * @ingroup EXAMPLES_MISC_17XX40XX
 * <b>Example description</b><br>
 * This example demonstrates programming of the SPI flash using the SPIFI interface.
 * The program can be run in two ways, the first and the default is having #USE_SPIFI_LIB defined
 * so that the code makes use of the functions provided by the spifi_*.lib. The second one is
 * having #USE_SPIFI_LIB not defined which will make direct calls to the boot ROM function pointers
 * and will not use any library calls.<br>
 * By default the program writes 256 bytes of data starting from hex 00 to
 * hex FF to #SPIFI_WRITE_SECTOR_OFFSET<br>
 *
 * UART needs to be setup prior to running the example as the example produces the output
 * to the UART console. If LED1 is ON then the programming is successful, if LED2 is ON then
 * the programming has failed.<br>
 *
 * <b>Special connection requirements</b><br>
 * There are no special connection requirements for this example.<br>
 *
 * <b>Build procedures:</b><br>
 * @ref LPCOPEN_17XX40XX_BUILDPROCS_KEIL<br>
 * @ref LPCOPEN_17XX40XX_BUILDPROCS_IAR<br>
 * @ref LPCOPEN_17XX40XX_BUILDPROCS_XPRESSO<br>
 *
 * <b>Supported boards and board setup:</b><br>
 * @ref LPCOPEN_17XX40XX_BOARD_EA4088<br>
 *
 * <b>Submitting LPCOpen issues:</b><br>
 * @ref LPCOPEN_COMMUNITY
 * @{
 */

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

static uint8_t data_buffer[PROG_SIZE];
static SPIFIobj spifi_obj;

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* Verify the spifi data */
static int verify_spifi_data(const uint8_t *buff, int size)
{
	int i;
	for (i = 0; i < size; i++) {
		if (*buff++ == (uint8_t) i) {
			break;
		}
	}
	return i == size ? 0 : i;
}

/* Initializes the buffer from 00 to FF */
static void prepare_write_data(uint8_t *buff, int size)
{
	int i;
	for (i = 0; i < size; i++) {
		*buff++ = (uint8_t) i;
	}
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/**
 * @brief	Main entry point
 * @return	Nothing
 */
int main(void)
{
	SPIFIobj *obj = &spifi_obj;
	uint32_t spifi_clk_mhz;
	SPIFIopers opers;
	int ret;

	spifi_rom_init();

#ifndef USE_SPIFI_LIB
	/* Decrease the CPU Clock for accessing ROM */
	Chip_Clock_SetCPUClockDiv(2);
#endif

	/* Initialize the board & LEDs for error indication */
	Board_Init();

	DEBUGSTR("SPIFI Demo\r\n");

	/* Initialize SPIFI interface */
	Board_SPIFI_Init();

	/* Enable SPIFI Clocking */
	Chip_SPIFI_Init();

	/* Since this code runs from SPIFI no special initialization required here */
	prepare_write_data(data_buffer, sizeof(data_buffer));

	spifi_clk_mhz = Chip_Clock_GetSPIFIClockRate() / 1000000;

	/* Typical time tCS is 20 ns min, we give 200 ns to be on safer side */
	ret = spifi_init(obj, spifi_clk_mhz / 5, S_RCVCLK | S_FULLCLK, spifi_clk_mhz);
	if (ret) {
		DEBUGOUT("Error 0x%x: Initializing SPIFI interface!\r\n", ret);
		Board_LED_Set(1, 1);
		goto end_prog;
	}

	/* Prepare the operations structure */
	memset(&opers, 0, sizeof(SPIFIopers));
	opers.dest = (char *) SPIFI_WRITE_SECTOR_OFFSET;
	opers.length = sizeof(data_buffer);
	opers.scratch = NULL;
	opers.protect = 0;
	opers.options = S_VERIFY_PROG;

	/* NOTE: All interrupts must be disabled before calling program as
	 * any triggered interrupts might attempt to run a code from SPIFI area
	 */
	ret = spifi_program(obj, (char *) data_buffer, &opers);
	if (ret) {
		DEBUGOUT("Error 0x%x: Programming of data buffer to SPIFI Failed!\r\n", ret);
		Board_LED_Set(1, 1);
		goto end_prog;
	}
	DEBUGSTR("SPIFI Programming successful!\r\n");

	if (verify_spifi_data((uint8_t *) SPIFI_WRITE_SECTOR_ADDRESS, sizeof(data_buffer))) {
		DEBUGSTR("Error: Verifying the SPIFI data\r\n");
		Board_LED_Set(1, 1);
		goto end_prog;
	}

	Board_LED_Set(0, 1);
	DEBUGSTR("SPIFI Data verified!\r\n");

end_prog:
	while (1) {
		__WFI();
	}
}

/**
 * @}
 */
