/* $Id$ */
//
// ������ HF Dream Receiver (�� ������� �����)
// ����� ���� ����������� mgs2001@mail.ru
// UA1ARN
//

#include "hardware.h"	/* ��������� �� ���������� ������� ������ � ������� */
#include "formats.h"	/* sprintf() replacement */
#include <ctype.h>
#include <string.h>

//#define WITHTEST_H7	CPUSTYLE_STM32H7XX

#if WITHUSESDCARD

#include "board.h"
#include "sdcard.h"
#include "spifuncs.h"
#include "ff.h"	
#include "diskio.h"		/* FatFs lower layer API */

#include "display.h"	/* ���������� ������� ��������� ������� ������� */
#include "keyboard.h"	
#include "audio.h"	



/* Card Status Register*/

#define SD_STAT_OUT_OF_RANGE       0x80000000  /*The command's argument was out of the allowed range for this card.*/
#define SD_STAT_ADDRESS_ERROR      0x40000000  /*A misaligned address which did not match the block length was used in the command. */
#define SD_STAT_BLOCK_LEN_ERROR    0x20000000  /*The transferred block length is not allowed for this card, or the number
                                         of transferred bytes does not match the block length. */
#define SD_STAT_ERASE_SEQ_ERROR    0x10000000  /*An error in the sequence of erase commands occurred. */
#define SD_STAT_ERASE_PARAM        0x08000000  /*An invalid selection of write-blocks for erase occurred. */
#define SD_STAT_WP_VIOLATION       0x04000000  /*Set when the host attempts to write to a protected block or to the
                                         temporary or permanent write protected card. */
#define SD_STAT_CARD_IS_LOCKED     0x02000000  /*When set, signals that the card is locked by the host */
#define SD_STAT_LOCK_UNLOCK_FAILED 0x01000000  /*Set when a sequence or password error has been detected in
                                         lock/unlock card command. */
#define SD_STAT_COM_CRC_ERROR      0x00800000  /*The CRC check of the previous command failed.*/
#define SD_STAT_ILLEGAL_COMMAND    0x00400000  /*Command not legal for the card state. */
#define SD_STAT_CARD_ECC_FAILED    0x00200000  /*Card internal ECC was applied but failed to correct the data.*/
#define SD_STAT_CC_ERROR           0x00100000  /*Internal card controller error.*/
#define SD_STAT_ERROR              0x00080000  /*A general or an unknown error occurred during the operation.*/
#define SD_STAT_CSD_OVERWRITE      0x00010000  /*Can be either one of the following errors:
                                          - The read only section of the CSD does not match the card content.
                                          - An attempt to reverse the copy (set as original) or permanent WP
                                            (unprotected) bits was made. */
#define SD_STAT_WP_ERASE_SKIP      0x00008000  /*Set when only partial address space was erased due to existing
                                         write protected blocks or the temporary or permanent write
                                         protected card was erased. */
#define SD_STAT_CARD_ECC_DISABLED  0x00004000  /*The command has been executed without using the internal ECC.*/
#define SD_STAT_ERASE_RESET        0x00002000  /*An erase sequence was cleared before executing because an out of
                                         erase sequence command was received. */
#define SD_STAT_CURRENT_STATE      0x00001E00  /*The state of the card when receiving the command. If the command
                                         execution causes a state change, it will be visible to the host in the
                                         response to the next command. The four bits are interpreted as a
                                         binary coded number between 0 and 15.*/
#define SD_STAT_CURRENT_STATE_IDLE  0x00000000  
#define SD_STAT_CURRENT_STATE_READY 0x00000200
#define SD_STAT_CURRENT_STATE_IDENT 0x00000400
#define SD_STAT_CURRENT_STATE_STDBY 0x00000600
#define SD_STAT_CURRENT_STATE_TRAN  0x00000800
#define SD_STAT_CURRENT_STATE_DATA  0x00000A00
#define SD_STAT_CURRENT_STATE_RECV  0x00000C00
#define SD_STAT_CURRENT_STATE_PROG  0x00000E00
#define SD_STAT_CURRENT_STATE_DIS   0x00001000

#define SD_STAT_READY_FOR_DATA      0x00000100 /*Corresponds to buffer empty signaling on the bus. */
#define SD_STAT_APP_CMD             0x00000020 /*The card will expect ACMD, or an indication that the command has
                                         been interpreted as ACMD. */
#define SD_STAT_AKE_SEQ_ERROR       0x0000008  /*Error in the sequence of the authentication process*/


// this variable will be used to track the current block length
// this allows the block length to be set only when needed
// unsigned long _BlockLength = 0;

// error/success codes
#define MMC_SUCCESS           0x00
#define MMC_BLOCK_SET_ERROR   0x01
#define MMC_RESPONSE_ERROR    0x02
#define MMC_DATA_TOKEN_ERROR  0x03
#define MMC_INIT_ERROR        0x04
#define MMC_CRC_ERROR         0x10
#define MMC_WRITE_ERROR       0x11
#define MMC_OTHER_ERROR       0x12
#define MMC_TIMEOUT_ERROR     0xFF



// ������� ��������� �������� ���� �� ������ �� �������
static unsigned long array_get_bit(const uint8_t * data, unsigned total, unsigned leftbit)
{
	const unsigned bitfromstart = (total - 1) - leftbit;
	const unsigned byteindex = bitfromstart / 8;
	const unsigned bitindex = 7 - bitfromstart % 8;
	return (data [byteindex] >> bitindex) & 0x01;

}

// ������� ��������� �������� �������� ���� �� �������
static unsigned long array_get_bits(const uint8_t * data, unsigned total, unsigned leftbit, unsigned width)
{
	unsigned long v = 0;
	while (width --)
		v = v * 2 + array_get_bit(data, total, leftbit --);
	return v;
}

// ������������� ������������ ������ - ���������� ������� SD CARD
// ����� � 0 - ������� ������, � 1 - �����.
static void sd_power_initialize(void)
{
	HARDWARE_SDIOPOWER_INITIALIZE();
}

static void sd_sense_initialize(void)
{
	HARDWARE_SDIOSENSE_INITIALIZE();
}

static void sd_power_enable(
	uint_fast8_t enable	// ���� ���������� ���������� ������� SD �����
	)
{
	board_set_sdcardpoweron(enable);
	board_update();
}

static void sd_power_cycle(void)
{
	HARDWARE_SDIO_HANGOFF();
	sd_power_enable(0);	// off
	local_delay_ms(200);
	sd_power_enable(1);	// and ON
	local_delay_ms(50);
	HARDWARE_SDIO_INITIALIZE();
}

void sdcardhw_initialize(void)
{
	//debug_printf_P(PSTR("sd_initialize: power cycle\n"));
	sd_sense_initialize();
	sd_power_initialize();
	//sd_power_cycle();
	
	//debug_printf_P(PSTR("sd_initialize: done.\n"));
	return;
}

static uint_fast64_t sdhost_sdcard_parse_CSD(const uint8_t * bv)
{
	uint_fast64_t MMC_CardSize = 0;
	uint_fast32_t mmc_C_SIZE = 0;
	// ��������� ���������������� ����� (Code length is 128 bits = 16 bytes)
	//
	const uint_fast8_t csdv = array_get_bits(bv, 128, 127, 2);	//  [127:126] - csd structure version
	debug_printf_P("CSD version=0x%02x\n", csdv);
	debug_printf_P("CSD TRAN_SPEED = %d (0x%02x)\n", array_get_bits(bv, 128, 103, 8), array_get_bits(bv, 128, 103, 8));	// [103:96] 

	switch (csdv)
	{	
	case 1:
		// CSD version 2.0
		// ������ ������ ����� ������ �� ��������� ������ 2
		mmc_C_SIZE = array_get_bits(bv, 128, 69, 22);	// [69:48] This parameter is used to calculate the user data area capacity in the SD memory card (not include the protected area).

		//debug_printf_P("mmc_C_SIZE = %lu\n", (unsigned long) mmc_C_SIZE);
		MMC_CardSize = (uint_fast64_t) (mmc_C_SIZE + 1) * 512 * 1024;
		break;

	case 0:
		// CSD version 1.0
		{
			uint_fast64_t im;      // mask
			unsigned short j;      // index
			// CSD version 1.0
			unsigned char mmc_READ_BL_LEN = 0,  // Read block length
				mmc_C_SIZE_MULT = 0;

			mmc_READ_BL_LEN = array_get_bits(bv, 128, 83, 4); // [83:80] bv [5] & 0x0F;		// lower 4 bits of CCC and
			mmc_C_SIZE = array_get_bits(bv, 128, 73, 12);	// [73:62]
			mmc_C_SIZE_MULT = array_get_bits(bv, 128, 49, 3);	// [49:47]
			// bits 73:62  C_Size (12 bit for v1)
			MMC_CardSize = (mmc_C_SIZE + 1);

			// power function with base 2 is better with a loop
			// i = (pow(2,mmc_C_SIZE_MULT+2)+0.5);
			for (im = 2, j = mmc_C_SIZE_MULT + 2; j > 1; j --)
				im <<= 1;

			MMC_CardSize *= im;

			// power function with base 2 is better with a loop
			//i = (pow(2,mmc_READ_BL_LEN)+0.5);
			for (im = 2, j = mmc_READ_BL_LEN; j > 1; j --)
				im <<= 1;

			MMC_CardSize *= im;
		}
		break;
	}
	return MMC_CardSize;
}

#if WITHSDHCHW	/* Hardware SD HOST CONTROLLER */

//void hardware_sdhost_setspeed(unsigned long ticksfreq);


/** 
 * @brief SDIO Commands  Index
 */
#define SD_CMD_GO_IDLE_STATE                       ((uint8_t)0)
#define SD_CMD_SEND_OP_COND                        ((uint8_t)1)
#define SD_CMD_ALL_SEND_CID                        ((uint8_t)2)
#define SD_CMD_SET_REL_ADDR                        ((uint8_t)3) /*!< SDIO_SEND_REL_ADDR for SD Card */
#define SD_CMD_SET_DSR                             ((uint8_t)4)
//#define SD_CMD_SDIO_SEN_OP_COND                    ((uint8_t)5)
#define SD_CMD_HS_SWITCH                           ((uint8_t)6)
#define SD_CMD_SEL_DESEL_CARD                      ((uint8_t)7)
//#define SD_CMD_HS_SEND_EXT_CSD                     ((uint8_t)8)
#define SD_CMD_SEND_CSD                            ((uint8_t)9)
#define SD_CMD_SEND_CID                            ((uint8_t)10)
#define SD_CMD_READ_DAT_UNTIL_STOP                 ((uint8_t)11) /*!< SD Card doesn't support it */
#define SD_CMD_STOP_TRANSMISSION                   ((uint8_t)12)
#define SD_CMD_SEND_STATUS                         ((uint8_t)13)
#define SD_CMD_HS_BUSTEST_READ                     ((uint8_t)14)
#define SD_CMD_GO_INACTIVE_STATE                   ((uint8_t)15)
#define SD_CMD_SET_BLOCKLEN                        ((uint8_t)16)
#define SD_CMD_READ_SINGLE_BLOCK                   ((uint8_t)17)
#define SD_CMD_READ_MULT_BLOCK                     ((uint8_t)18)
#define SD_CMD_HS_BUSTEST_WRITE                    ((uint8_t)19)
#define SD_CMD_WRITE_DAT_UNTIL_STOP                ((uint8_t)20) /*!< SD Card doesn't support it */
#define SD_CMD_SET_BLOCK_COUNT                     ((uint8_t)23) /*!< SD Card doesn't support it */
#define SD_CMD_WRITE_SINGLE_BLOCK                  ((uint8_t)24)
#define SD_CMD_WRITE_MULT_BLOCK                    ((uint8_t)25)
#define SD_CMD_PROG_CID                            ((uint8_t)26) /*!< reserved for manufacturers */
#define SD_CMD_PROG_CSD                            ((uint8_t)27)
#define SD_CMD_SET_WRITE_PROT                      ((uint8_t)28)
#define SD_CMD_CLR_WRITE_PROT                      ((uint8_t)29)
#define SD_CMD_SEND_WRITE_PROT                     ((uint8_t)30)
#define SD_CMD_SD_ERASE_GRP_START                  ((uint8_t)32) /*!< To set the address of the first write
                                                                  block to be erased. (For SD card only) */
#define SD_CMD_SD_ERASE_GRP_END                    ((uint8_t)33) /*!< To set the address of the last write block of the
                                                                  continuous range to be erased. (For SD card only) */
#define SD_CMD_ERASE_GRP_START                     ((uint8_t)35) /*!< To set the address of the first write block to be erased.
                                                                  (For MMC card only spec 3.31) */

#define SD_CMD_ERASE_GRP_END                       ((uint8_t)36) /*!< To set the address of the last write block of the
                                                                  continuous range to be erased. (For MMC card only spec 3.31) */

#define SD_CMD_ERASE                               ((uint8_t)38)
#define SD_CMD_FAST_IO                             ((uint8_t)39) /*!< SD Card doesn't support it */
#define SD_CMD_GO_IRQ_STATE                        ((uint8_t)40) /*!< SD Card doesn't support it */
#define SD_CMD_LOCK_UNLOCK                         ((uint8_t)42)
#define SD_CMD_APP_CMD                             ((uint8_t)55)
#define SD_CMD_GEN_CMD                             ((uint8_t)56)
#define SD_CMD_NO_CMD                              ((uint8_t)64)

/** 
 * @brief Following commands are SD Card Specific commands.
 *        SDIO_APP_CMD should be sent before sending these commands.
 */
#define SD_CMD_APP_SD_SET_BUSWIDTH                 ((uint8_t)6)  /*!< For SD Card only */
#define SD_CMD_SD_APP_STATUS                       ((uint8_t)13) /*!< For SD Card only */
#define SD_CMD_SD_APP_SEND_NUM_WRITE_BLOCKS        ((uint8_t)22) /*!< For SD Card only */
#define SD_CMD_SD_APP_OP_COND                      ((uint8_t)41) /*!< For SD Card only */
#define SD_CMD_SD_APP_SET_CLR_CARD_DETECT          ((uint8_t)42) /*!< For SD Card only */
#define SD_CMD_SD_APP_SEND_SCR                     ((uint8_t)51) /*!< For SD Card only */
#define SD_CMD_SDIO_RW_DIRECT                      ((uint8_t)52) /*!< For SD I/O Card only */
#define SD_CMD_SDIO_RW_EXTENDED                    ((uint8_t)53) /*!< For SD I/O Card only */

#define SD_CMD_SD_APP_SET_NWB_PREERASED        ((uint8_t)23) /*!< For SD Card only */

/** 
 * @brief Following commands are SD Card Specific security commands.
 *        SDIO_APP_CMD should be sent before sending these commands.
 */
#define SD_CMD_SD_APP_GET_MKB                      ((uint8_t)43) /*!< For SD Card only */
#define SD_CMD_SD_APP_GET_MID                      ((uint8_t)44) /*!< For SD Card only */
#define SD_CMD_SD_APP_SET_CER_RN1                  ((uint8_t)45) /*!< For SD Card only */
#define SD_CMD_SD_APP_GET_CER_RN2                  ((uint8_t)46) /*!< For SD Card only */
#define SD_CMD_SD_APP_SET_CER_RES2                 ((uint8_t)47) /*!< For SD Card only */
#define SD_CMD_SD_APP_GET_CER_RES1                 ((uint8_t)48) /*!< For SD Card only */
#define SD_CMD_SD_APP_SECURE_READ_MULTIPLE_BLOCK   ((uint8_t)18) /*!< For SD Card only */
#define SD_CMD_SD_APP_SECURE_WRITE_MULTIPLE_BLOCK  ((uint8_t)25) /*!< For SD Card only */
#define SD_CMD_SD_APP_SECURE_ERASE                 ((uint8_t)38) /*!< For SD Card only */
#define SD_CMD_SD_APP_CHANGE_SECURE_AREA           ((uint8_t)49) /*!< For SD Card only */
#define SD_CMD_SD_APP_SECURE_WRITE_MKB             ((uint8_t)48) /*!< For SD Card only */
/** 
 * @brief  Following commands are SD Card Specific commands.
 *         SDIO_APP_CMD should be sent before sending these commands.
 */
#define SDIO_SEND_IF_COND               ((uint32_t)0x00000008)

#define SD_VOLTAGE_WINDOW_SD            ((uint32_t)0x80100000)
#define SD_HIGH_CAPACITY                ((uint32_t)0x40000000)
#define SD_STD_CAPACITY                 ((uint32_t)0x00000000)
#define SD_CHECK_PATTERN                ((uint32_t)0x000001AA)

/** 
 * @brief Supported SD Memory Cards
 */
#define SDIO_STD_CAPACITY_SD_CARD_V1_1             ((uint32_t)0x00000000)
#define SDIO_STD_CAPACITY_SD_CARD_V2_0             ((uint32_t)0x00000001)
#define SDIO_HIGH_CAPACITY_SD_CARD                 ((uint32_t)0x00000002)
#define SDIO_MULTIMEDIA_CARD                       ((uint32_t)0x00000003)
#define SDIO_SECURE_DIGITAL_IO_CARD                ((uint32_t)0x00000004)
#define SDIO_HIGH_SPEED_MULTIMEDIA_CARD            ((uint32_t)0x00000005)
#define SDIO_SECURE_DIGITAL_IO_COMBO_CARD          ((uint32_t)0x00000006)
#define SDIO_HIGH_CAPACITY_MMC_CARD                ((uint32_t)0x00000007)

static uint_fast32_t sdhost_sdcard_RCA; /* = 0 */
static uint8_t sdhost_sdcard_CID [16];
static uint8_t sdhost_sdcard_CSD [16];

static uint_fast32_t sdhost_CardType = SDIO_STD_CAPACITY_SD_CARD_V1_1;
static uint32_t sdhost_SDType = SD_STD_CAPACITY;

static uint_fast8_t sdhost_use_cmd23;
static uint_fast8_t sdhost_use_cmd20;

#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)


#define RESPLIMIT 64000U
#define RESPLIMITREAD 640000L

/* Assert the CS signal, active low (CS=0) */
static void SDCARD_CS_LOW(void)
{
	spi_select2(targetsdcard, SPIC_MODE3, SPIC_SPEEDSDCARD);	// MODE0/MODE3 - ������ ����������� ����
}

/* Deassert the CS signal (CS=1) */
static void SDCARD_CS_HIGH(void)
{
	spi_unselect(targetsdcard);
}

// ���������� �������� SPI ��� ���������� ������ �����������
void hardware_sdhost_setspeed(unsigned long ticksfreq)
{
	hardware_spi_master_setfreq(SPIC_SPEEDSDCARD, ticksfreq);
}

void hardware_sdhost_setbuswidth(uint_fast8_t use4bit)
{
}

/* ��������������� ������������ ������� CRC ��� ������ ����.
   CRC �������� � ������� 7 �����.
*/
static  uint_fast8_t crc7b1(uint_fast8_t crc, uint_fast8_t v1)
{
	if ((v1 != 0) != ((crc & 0x80) != 0))
	{
		crc <<= 1;
		crc ^= (0x09 << 1);
	}
	else
	{
		crc <<= 1;
	}

	return crc;
}

/* ��������������� ������������ ������� CRC ��� ������ ����� */
static uint_fast8_t crc7b8(uint_fast8_t crc, uint_fast8_t v8)
{
	crc = crc7b1(crc, v8 & 0x80);
	crc = crc7b1(crc, v8 & 0x40);
	crc = crc7b1(crc, v8 & 0x20);
	crc = crc7b1(crc, v8 & 0x10);
	crc = crc7b1(crc, v8 & 0x08);
	crc = crc7b1(crc, v8 & 0x04);
	crc = crc7b1(crc, v8 & 0x02);
	crc = crc7b1(crc, v8 & 0x01);

	return crc;
}

#elif CPUSTYLE_R7S721

#elif CPUSTYLE_STM32H7XX || CPUSTYLE_STM32F7XX || CPUSTYLE_STM32F4XX


/** 
 * @brief  SDIO Static flags, TimeOut, FIFO Address
 */
#define SDIO_STATIC_FLAGS               ((uint32_t)0x000005FF)
#define SDIO_CMD0TIMEOUT                ((uint32_t)0x00010000)

// SDIO data transfer complete
//	SDIO DMA2	Stream6	Channel 4	
void DMA2_Stream6_IRQHandler(void)
{
	debug_printf_P(PSTR("DMA2_Stream6_IRQHandler() trapped.\n"));
	for (;;)
		;
}

void SDIO_IRQHandler(void)
{
	debug_printf_P(PSTR("SDIO_IRQHandler trapped\n"));
	for (;;)
		;
}

#else
	#error Wrong CPUSTYLE_xxx
#endif


/* DMA ��� ������ � SD CARD  - ��������� ���������� ���������� ������ */
//	RX	SAI2_B	DMA2	Stream6	Channel 4	
static void DMA_SDIO_setparams(
	uintptr_t addr,
	uint32_t length0,
	uint32_t count,
	uint_fast8_t direction	// 0: Peripheral-to-memory, 1: Memory-to-peripherial
	)
{
	//debug_printf_P(PSTR("DMA_SDIO_setparams: addr=%p\n"), addr);
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)

#elif CPUSTYLE_R7S721

	// DMA �� �������� SDHI0

	enum { id = 14 };	// 14: DMAC14

	DMAC14.CHCTRL_n = 1 * (1U << DMAC14_CHCTRL_n_SWRST_SHIFT);		// SWRST
	//DMAC14.CHCTRL_n = 1 * (1U << DMAC14_CHCTRL_n_CLRINTMSK_SHIFT);	// CLRINTMSK
	DMAC14.CHCTRL_n = 1 * (1U << DMAC14_CHCTRL_n_SETINTMSK_SHIFT);	// SETINTMSK - ��������� ���������� �� ����������� DMA

	if (direction != 0)
	{
		// Write to SD CARD
		/* SDHI_0 transmission */
		// DMAC14
		DMAC14.N0SA_n = addr;	// Source address
		DMAC14.N0DA_n = (uint32_t) & SDHI0.SD_BUF0;	// Fixed destination address
		DMAC14.N0TB_n = length0 * count;	// ������ � ������

		// Values from Table 9.4 On-Chip Peripheral Module Requests
		// SDHI0 (transmit data empty)
		const uint_fast8_t mid = 0x30;	
		const uint_fast8_t rid = 0x01;		
		const uint_fast8_t tm = 0;		
		const uint_fast8_t am = 0x02;		
		const uint_fast8_t lvl = 1;		
		const uint_fast8_t reqd = 1;	

		DMAC14.CHCFG_n =
			0 * (1U << 31) |	// DMS	0: Register mode
			0 * (1U << 30) |	// REN	0: Does not continue DMA transfers.
			0 * (1U << 29) |	// RSW	0: Does not invert RSEL automatically after a DMA transaction
			0 * (1U << 28) |	// RSEL	0: Executes the Next0 Register Set
			0 * (1U << 27) |	// SBE	0: Stops the DMA transfer without sweeping the buffer (initial value).
			0 * (1U << 24) |	// DEM	0: Does not mask the DMA transfer end interrupt - ���������� ������ ��� ����� TC
			tm * (1U << 22) |	// TM	0: Single transfer mode - ������ �� Table 9.4
			1 * (1U << 21) |	// DAD	1: Fixed destination address
			0 * (1U << 20) |	// SAD	0: Increment source address
			2 * (1U << 16) |	// DDS	2: 32 bits (Destination Data Size)
			2 * (1U << 12) |	// SDS	2: 32 bits (Source Data Size)
			am * (1U << 8) |	// AM	1: ACK mode: Level mode (active until the transfer request from an on-chip peripheral module
			lvl * (1U << 6) |	// LVL	1: Detects based on the level.
			1 * (1U << 5) |		// HIEN	1: When LVL = 1: Detects a request when the signal is at the High level.
			reqd * (1U << 3) |	// REQD		Request Direction
			(id & 0x07) * (1U << 0) |		// SEL	0: CH0/CH8
			0;

		enum { dmarsshift = (id & 0x01) * 16 };
		DMAC1415.DMARS = (DMAC1415.DMARS & ~ (0x1FFul << dmarsshift)) |
			mid * (1U << (2 + dmarsshift)) |		// MID
			rid * (1U << (0 + dmarsshift)) |		// RID
			0;
	}
	else
	{
		// Read from SD CARD
		/* SDHI_0 reception */
		// DMAC14
		DMAC14.N0SA_n = (uint32_t) & SDHI0.SD_BUF0;		// Fixed source addess
		DMAC14.N0DA_n = addr;	// destination address
		DMAC14.N0TB_n = length0 * count;	// ������ � ������

		// Values from Table 9.4 On-Chip Peripheral Module Requests
		// SDHI0 (transmit data empty)
		const uint_fast8_t mid = 0x30;	
		const uint_fast8_t rid = 0x02;		
		const uint_fast8_t tm = 0;		
		const uint_fast8_t am = 0x02;		
		const uint_fast8_t lvl = 1;		
		const uint_fast8_t reqd = 0;	

		DMAC14.CHCFG_n =
			0 * (1U << 31) |	// DMS	0: Register mode
			0 * (1U << 30) |	// REN	0: Does not continue DMA transfers.
			0 * (1U << 29) |	// RSW	0: Does not invert RSEL automatically after a DMA transaction.
			0 * (1U << 28) |	// RSEL	0: Executes the Next0 Register Set
			0 * (1U << 27) |	// SBE	0: Stops the DMA transfer without sweeping the buffer (initial value).
			0 * (1U << 24) |	// DEM	0: Does not mask the DMA transfer end interrupt - ���������� ������ ��� ����� TC
			tm * (1U << 22) |	// TM	0: Single transfer mode - ������ �� Table 9.4
			0 * (1U << 21) |	// DAD	0: Increment destination address
			1 * (1U << 20) |	// SAD	1: Fixed source address
			2 * (1U << 16) |	// DDS	2: 32 bits (Destination Data Size)
			2 * (1U << 12) |	// SDS	2: 32 bits (Source Data Size)
			am * (1U << 8) |	// AM	1: ACK mode: Level mode (active until the transfer request from an on-chip peripheral module
			lvl * (1U << 6) |	// LVL	1: Detects based on the level.
			1 * (1U << 5) |		// HIEN	1: When LVL = 1: Detects a request when the signal is at the High level.
			reqd * (1U << 3) |	// REQD		Request Direction
			(id & 0x07) * (1U << 0) |		// SEL	0: CH0/CH8
			0;

		enum { dmarsshift = (id & 0x01) * 16 };
		DMAC1415.DMARS = (DMAC1415.DMARS & ~ (0x1FFul << dmarsshift)) |
			mid * (1U << (2 + dmarsshift)) |		// MID
			rid * (1U << (0 + dmarsshift)) |		// RID
			0;
	}


	// ��������� ������ ��� ���� ������� (8..15)
    DMAC815.DCTRL_0_7 = (DMAC815.DCTRL_0_7 & ~ (/*(1U << 1) | */(1U << 0))) |
		//1 * (1U << 1) |		// LVINT	1: Level output
		1 * (1U << 0) |		// PR		1: Round robin mode
		0;

	DMAC14.CHCTRL_n = 1 * (1U << 0);		// SETEN

	if (count > 1)
	{
		while ((SDHI0.SD_INFO2 & (1uL << 13)) == 0)	// SCLKDIVEN
			;
		SDHI0.SD_SIZE = length0;

		while ((SDHI0.SD_INFO2 & (1uL << 13)) == 0)	// SCLKDIVEN
			;
		SDHI0.SD_STOP = 1 * (1U << 8);	// SEC 1: Enables SD_SECCNT

		while ((SDHI0.SD_INFO2 & (1uL << 13)) == 0)	// SCLKDIVEN
			;
		SDHI0.SD_SECCNT = count;
	}
	else
	{
		while ((SDHI0.SD_INFO2 & (1uL << 13)) == 0)	// SCLKDIVEN
			;
		SDHI0.SD_SIZE = length0;

		while ((SDHI0.SD_INFO2 & (1uL << 13)) == 0)	// SCLKDIVEN
			;
		SDHI0.SD_STOP = 0 * (1U << 8);	// SEC 0: Disables SD_SECCNT
	}

	SDHI0.CC_EXT_MODE |= (1uL << 1);	// DMASDRW

#elif CPUSTYLE_STM32H7XX

	// � ���������� ��� ������ � SDIO ������������ ���������� ���� DMA

	SDMMC1->IDMACTRL |= SDMMC_IDMA_IDMAEN;
	SDMMC1->IDMABASE0 = addr;
	//SDMMC1->IDMABSIZE = (SDMMC1->IDMABSIZE & ~ (SDMMC_IDMABSIZE_IDMABNDT)) |
	//	(((length0 * count / 32) << SDMMC_IDMABSIZE_IDMABNDT_Pos) & SDMMC_IDMABSIZE_IDMABNDT_Msk) |
	//0;
	//debug_printf_P(PSTR("SDMMC1->IDMABASE0=%08lx\n"), SDMMC1->IDMABASE0);
	ASSERT((SDMMC1->IDMACTRL & SDMMC_IDMA_IDMAEN_Msk) != 0);
	ASSERT(SDMMC1->IDMABASE0 == addr);


#elif CPUSTYLE_STM32F7XX || CPUSTYLE_STM32F4XX

	/* DMA2	Stream6	Channel 4 */ 
	const uint_fast8_t ch = 4;

	RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;	// ������� DMA2 
	__DSB();

#if CPUSTYLE_STM32F7XX
	DMA2_Stream6->PAR = (uint32_t) & SDMMC1->FIFO;
#else /* CPUSTYLE_STM32F7XX */
	DMA2_Stream6->PAR = (uint32_t) & SDIO->FIFO;
#endif /* CPUSTYLE_STM32F7XX */

	DMA2_Stream6->M0AR = addr;
	DMA2_Stream6->NDTR = (DMA2_Stream6->NDTR & ~ DMA_SxNDT) |
		(1 * DMA_SxNDT_0);		// ������ ������ - ������� ������������. � �������� �� ST ������� 1

	if (direction == 0)
	{
		// Read from SD CARD
		//DMA2_Stream6->NDTR = (DMA2_Stream6->NDTR & ~ DMA_SxNDT) |
		//	(length0 * count / 4 * DMA_SxNDT_0);		// ������ ������ - ������� ������������. � �������� �� ST ������� 1

		DMA2_Stream6->FCR =
			1 * DMA_SxFCR_DMDIS |	// use FIFO mode
			3 * DMA_SxFCR_FTH_0 |	// 11: full FIFO
			0;

		DMA2_Stream6->CR =
			0 * DMA_SxCR_DIR_0 |	// 00: Peripheral-to-memory
			ch * DMA_SxCR_CHSEL_0 | // �����
			1 * DMA_SxCR_PFCTRL |	// 1: The peripheral is the flow controller (required for SDIO/SDMMC connected DMA)
			1 * DMA_SxCR_MBURST_0 |	// 01: INCR4 (incremental burst of 4 beats)
			1 * DMA_SxCR_PBURST_0 |	// 01: INCR4 (incremental burst of 4 beats)
			//0 * DMA_SxCR_DIR_0 |	// 00: Peripheral-to-memory
			1 * DMA_SxCR_MINC |		//��������� ������
			2 * DMA_SxCR_MSIZE_0 | //����� � ������ - 32 bit
			2 * DMA_SxCR_PSIZE_0 | //����� � DR - 32 bit
			//1 * DMA_SxCR_CIRC | //����������� ����� �� ��������� ��� DBM
			2 * DMA_SxCR_PL_0 |		// Priority level - High
			0 * DMA_SxCR_CT |	// M0AR selected
			//0 * DMA_SxCR_DBM |	 // double buffer mode seelcted
			0;
	}
	else
	{
		// Write to SD CARD

		//DMA2_Stream6->NDTR = (DMA2_Stream6->NDTR & ~ DMA_SxNDT) |
		//	(length0 * count * DMA_SxNDT_0);		// ������ ������ - ������� ������������. � �������� �� ST ������� 1

		DMA2_Stream6->FCR =
			1 * DMA_SxFCR_DMDIS |	// use FIFO mode
			1 * DMA_SxFCR_FTH_0 |	// 01: 1/2 full FIFO (changed from read !)
			0;

		DMA2_Stream6->CR =
			1 * DMA_SxCR_DIR_0 |	// 01: Memory-to-peripherial
			ch * DMA_SxCR_CHSEL_0 | // �����
			1 * DMA_SxCR_PFCTRL |	// 1: The peripheral is the flow controller (required for SDIO/SDMMC connected DMA)
			0 * DMA_SxCR_MBURST_0 |	// 00: single transfer (changed from read !)
			1 * DMA_SxCR_PBURST_0 |	// 01: INCR4 (incremental burst of 4 beats)
			//0 * DMA_SxCR_DIR_0 |	// 00: Peripheral-to-memory
			1 * DMA_SxCR_MINC |		//��������� ������
			0 * DMA_SxCR_MSIZE_0 | //����� � ������ - 8 bit (changed from read !)
			2 * DMA_SxCR_PSIZE_0 | //����� � DR - 32 bit
			//1 * DMA_SxCR_CIRC | //����������� ����� �� ��������� ��� DBM
			2 * DMA_SxCR_PL_0 |		// Priority level - High
			0 * DMA_SxCR_CT |	// M0AR selected
			//0 * DMA_SxCR_DBM |	 // double buffer mode seelcted
			0;
	}

	DMA2->HIFCR = (DMA_HIFCR_CTCIF6 /*| DMA_HIFCR_CTEIF6 */);	// Clear TC interrupt flag ��������������� stream
	//DMA2_Stream6->CR |= (DMA_SxCR_TCIE /* | DMA_SxCR_TEIE */);	// ��������� ���������� �� DMA �� TC � TE
	
	DMA2_Stream6->CR |= DMA_SxCR_EN;

#else
	#error Wrong CPUSTYLE_xxx
#endif
}

static uint_fast8_t DMA_sdio_waitdone(void)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)
	return 0;

#elif CPUSTYLE_R7S721

	while ((DMAC14.CHSTAT_n & (1U << 5)) == 0)	// END
		;

	while ((SDHI0.SD_INFO2 & (1uL << 13)) == 0)	// SCLKDIVEN
		;
	SDHI0.CC_EXT_MODE &= ~ (1uL << 1);	// DMASDRW
	return 0;

#elif CPUSTYLE_STM32H7XX
	// � ���������� ��� ������ � SDIO ������������ ���������� ���� DMA
	// check result
	if ((SDMMC1->STA & (SDMMC_STA_DATAEND | SDMMC_STA_DCRCFAIL | SDMMC_STA_DTIMEOUT)) != SDMMC_STA_DATAEND)
		return 1;
	return 0;

#elif CPUSTYLE_STM32F7XX || CPUSTYLE_STM32F4XX

	while ((DMA2->HISR & DMA_HISR_TCIF6) == 0)
	{
		//debug_printf_P(PSTR("DMA_sdio_waitdone: NDTR=%lu\n"), DMA2_Stream6->NDTRDMA2_Stream6->NDTR);
	}

	if ((DMA2->HISR & DMA_HISR_TCIF6) != 0)
		DMA2->HIFCR = DMA_HIFCR_CTCIF6;		// ������� ���� - DMA ������ �������� � ������
	else
	{
		debug_printf_P(PSTR("DMA_sdio_waitdone: force stop, NDTR=%lu\n"), DMA2_Stream6->NDTR);
		// DMA ��� ����� ����������
		// ���� �������� � ��������� ���������
		DMA2_Stream6->CR &= ~ DMA_SxCR_EN;
		while ((DMA2_Stream6->CR &  DMA_SxCR_EN) != 0)
			;
		//DMA2_Stream6->CR |= DMA_SxCR_EN;	// ���������� DMA
	}
	return 0;

#else
	#error Wrong CPUSTYLE_xxx
#endif
}

// ����� � SD CARD �������� ��������. ���������� DMA
static void DMA_sdio_cancel(void)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)

#elif CPUSTYLE_R7S721

	while ((SDHI0.SD_INFO2 & (1uL << 13)) == 0)	// SCLKDIVEN
		;
	SDHI0.CC_EXT_MODE &= ~ (1uL << 1);	// DMASDRW

#elif CPUSTYLE_STM32H7XX
	// � ���������� ��� ������ � SDIO ������������ ���������� ���� DMA
	//SDMMC1->CMD = SDMMC_CMD_CMDSTOP;

	SDMMC1->IDMACTRL &= ~ SDMMC_IDMA_IDMAEN;

#elif CPUSTYLE_STM32F7XX || CPUSTYLE_STM32F4XX

	{
		//debug_printf_P(PSTR("DMA_sdio_cancel: force stop, NDTR=%lu\n"), DMA2_Stream6->NDTR);
		// DMA ��� ����� ����������
		// ���� �������� � ��������� ���������
		DMA2_Stream6->CR &= ~ DMA_SxCR_EN;
		while ((DMA2_Stream6->CR &  DMA_SxCR_EN) != 0)
			;
		//DMA2_Stream6->CR |= DMA_SxCR_EN;	// ���������� DMA
	}

#else
	#error Wrong CPUSTYLE_xxx
#endif
}


#if CPUSTYLE_R7S721

#elif CPUSTYLE_STM32F7XX || CPUSTYLE_STM32H7XX

#elif CPUSTYLE_STM32F4XX
#else
	#error Wrong CPUSTYLE_xxx
#endif




#if CPUSTYLE_STM32H7XX

static volatile int sd_event_xx;
static volatile int sd_event_value;

enum
{

	EV_SD_ERROR = 1,
	EV_SD_READY = 2,
	EV_SD_DATA = 4

};

enum { WAIT_ANY = 0 };
typedef uint_fast8_t events_t;


static void
SetEvents(events_t v)
{
	sd_event_xx = 1;
	sd_event_value = v;
}

static int
WaitEvents(events_t e, int type)
{

	for (;;)
	{
		disableIRQ();
		if (sd_event_xx != 0 /*&& (sd_event_value & e) != 0 */)
		{
			sd_event_xx = 0;
			enableIRQ();
			break;
		}
		enableIRQ();
	}
	return EV_SD_READY;
}


//SDMMC1 Interrupt
void /*__attribute__((interrupt)) */ SDMMC1_IRQHandler(void)
{
   const uint32_t f = SDMMC1->STA & SDMMC1->MASK;
   events_t e = 0;
   if (f & (SDMMC_STA_CCRCFAIL | SDMMC_STA_CTIMEOUT))
   {
      //Command path error
      e |= EV_SD_ERROR;
      //Mask error interrupts, leave flags in STA for the further analisys
      SDMMC1->MASK &= ~(SDMMC_STA_CCRCFAIL | SDMMC_STA_CTIMEOUT);
   }
   if (f & (SDMMC_STA_CMDREND | SDMMC_STA_CMDSENT))
   {
      //We have finished the command execution
      e |= EV_SD_READY;
      SDMMC1->MASK &= ~ (SDMMC_STA_CMDREND | SDMMC_STA_CMDSENT);
      SDMMC1->ICR |= (SDMMC_STA_CMDREND | SDMMC_STA_CMDSENT);
   }
   uint32_t fdata = (f & (SDMMC_STA_DATAEND | SDMMC_STA_DBCKEND | SDMMC_STA_DCRCFAIL | SDMMC_STA_DTIMEOUT));
   if (fdata)
   {
      //We have finished the data send/receive
      e |= EV_SD_DATA;
      SDMMC1->MASK &= ~(SDMMC_STA_DATAEND | SDMMC_STA_DBCKEND | SDMMC_STA_DCRCFAIL | SDMMC_STA_DTIMEOUT);
   }

   SetEvents(e);
}

#define SD_CMD_NO_RESP (SDMMC_CMD_WAITRESP_0 * 0)
#define SD_CMD_SHORT_RESP (SDMMC_CMD_WAITRESP_0 * 1)
#define SD_CMD_LONG_RESP (SDMMC_CMD_WAITRESP_0 * 3)

#define SD_OK        0
#define SD_ERROR     1

static uint32_t SDSendCommand(uint32_t cmd, uint32_t arg, uint32_t rsp)
{
   //debug_printf_P(PSTR("CMD%d ARG=%08x\n"), cmd, arg);
   SDMMC1->ARG = arg;
   //Clear interrupt flags, unmask interrupts according to cmd response
   SDMMC1->ICR = (SDMMC_ICR_CMDRENDC | SDMMC_ICR_CMDSENTC | SDMMC_ICR_CCRCFAILC | SDMMC_ICR_CTIMEOUTC);
   if (rsp == SD_CMD_NO_RESP)
   {
      //No response wait for CMDSENT interrupt
      SDMMC1->MASK |= (SDMMC_MASK_CMDSENTIE | SDMMC_MASK_CCRCFAILIE | SDMMC_MASK_CTIMEOUTIE);
   }
   else 
   {
      //Short or long response wait for CMDREND interrupt
      SDMMC1->MASK |= (SDMMC_MASK_CMDRENDIE | SDMMC_MASK_CCRCFAILIE | SDMMC_MASK_CTIMEOUTIE);
   }
   SDMMC1->CMD = SDMMC_CMD_CPSMEN | rsp | cmd;
   if (WaitEvents(EV_SD_READY | EV_SD_ERROR, WAIT_ANY) != EV_SD_READY)
   { 
      //debug_printf_P("E1(cmd=%d arg=%x STA=%x)\n",cmd,arg,SDMMC1->STA);
      return SD_ERROR; 
   }
   //ASSERT(WaitEvents(EV_SD_READY | EV_SD_ERROR, WAIT_ANY) == EV_SD_READY);
   //Check for the correct response (long response and OCR response will result in 0x3F value in RESPCMD)
   if (rsp == SD_CMD_SHORT_RESP && SDMMC1->RESPCMD != (cmd & SDMMC_CMD_CMDINDEX))
   {
      //debug_printf_P("E2(cmd=%d arg=%x STA=%x)\n",cmd,arg,SDMMC1->STA);
      return SD_ERROR;
   }
   //ASSERT(!(rsp == SD_CMD_SHORT_RESP && SDMMC1->RESPCMD != cmd));
   //debug_printf_P("OK");
   return SD_OK;
}

#endif /* CPUSTYLE_STM32H7XX */


// �������� ��������� ������ data path state machine
static uint_fast8_t sdhost_dpsm_wait(uint_fast8_t txmode)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)
	return 0;

#elif CPUSTYLE_R7S721

	return 0;

#elif CPUSTYLE_STM32H7XX

	WaitEvents(EV_SD_DATA, WAIT_ANY);
	return 0;

#elif CPUSTYLE_STM32F7XX

	unsigned w;
	w = 0;
	const uint_fast32_t errmask = (SDMMC_STA_DCRCFAIL | SDMMC_STA_RXOVERR | SDMMC_STA_TXUNDERR | SDMMC_STA_DTIMEOUT);
	for (;; ++ w)
	{
		const uint_fast32_t sta = SDMMC1->STA;
		if ((sta & errmask) != 0)
			break;
		if ((sta & SDMMC_STA_DBCKEND) != 0)
			return 0;
	}
	debug_printf_P(PSTR("sdhost_dpsm_wait error, STA=%08lX, w=%u\n"), SDMMC1->STA, w);
	//SDMMC1->ICR = SDMMC1->STA & errmask;
	return 1;

#elif CPUSTYLE_STM32F4XX

	for (;;)
	{
		const uint_fast32_t sta = SDIO->STA;
		if ((sta & (SDIO_STA_DCRCFAIL | SDIO_STA_RXOVERR | SDIO_STA_TXUNDERR | SDIO_STA_DTIMEOUT /*| SDIO_STA_STBITERR */)) != 0)
			break;
		if ((sta & SDIO_STA_DBCKEND) != 0)
			return 0;
	}
	debug_printf_P(PSTR("sdhost_dpsm_wait error, STA=%08lX\n"), SDIO->STA);
	return 1;

#else
	#error Wrong CPUSTYLE_xxx
	return 1;
#endif
}


static void sdhost_dpsm_wait_fifo_empty(void)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)

#elif CPUSTYLE_R7S721

#elif CPUSTYLE_STM32H7XX
	// � ���������� ��� ������ � SDIO ������������ ���������� ���� DMA
	// TODO: ����� ������ �������� ��������� FIFO
	for (;;)
	{
		//const uint_fast32_t fifocnt = SDMMC1->FIFOCNT;
		//if ((fifocnt & SDMMC_FIFOCNT_FIFOCOUNT) == 0)
			break;
	}

#elif CPUSTYLE_STM32F7XX

	for (;;)
	{
		const uint_fast32_t fifocnt = SDMMC1->FIFOCNT;
		if ((fifocnt & SDMMC_FIFOCNT_FIFOCOUNT) == 0)
			break;
	}

#elif CPUSTYLE_STM32F4XX

	for (;;)
	{
		const uint_fast32_t fifocnt = SDIO->FIFOCNT;
		if ((fifocnt & SDIO_FIFOCNT_FIFOCOUNT) == 0)
			break;
	}
#else
	#error Wrong CPUSTYLE_xxx
#endif
}

// ���������� � ������ data path state machine
static void sdhost_dpsm_prepare(uintptr_t addr, uint_fast8_t txmode, uint_fast32_t len, uint_fast32_t lenpower)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)

#elif CPUSTYLE_R7S721


#elif CPUSTYLE_STM32H7XX


	//Clean DPSM interrupt flags and enable DPSM interrupts
	SDMMC1->ICR = (SDMMC_ICR_DBCKENDC | SDMMC_ICR_DATAENDC | SDMMC_ICR_DCRCFAILC | SDMMC_ICR_DTIMEOUTC);
	SDMMC1->MASK |= (SDMMC_MASK_DATAENDIE | SDMMC_MASK_DCRCFAILIE | SDMMC_MASK_DTIMEOUTIE);

	SDMMC1->DLEN = (SDMMC1->DLEN & ~ (SDMMC_DLEN_DATALENGTH_Msk)) |
		((len << SDMMC_DLEN_DATALENGTH_Pos) & SDMMC_DLEN_DATALENGTH_Msk) |
		0;
	SDMMC1->DTIMER = 0x03FFFFFF;

	SDMMC1->DCTRL =
		//SDMMC_DCTRL_DTEN_Msk |		// Data transfer enabled bit
		! txmode * SDMMC_DCTRL_DTDIR_Msk |		// 1: From card to controller.
		((lenpower * SDMMC_DCTRL_DBLOCKSIZE_0) & SDMMC_DCTRL_DBLOCKSIZE_Msk) |	// 9: 512 bytes, 3: 8 bytes
		0;

#elif CPUSTYLE_STM32F7XX

	SDMMC1->DTIMER = 0x03FFFFFF;

	SDMMC1->DLEN = (len & SDMMC_DLEN_DATALENGTH);

	SDMMC1->DCTRL =
		1 * SDMMC_DCTRL_DTEN_Msk |		// Data transfer enabled bit
		! txmode * SDMMC_DCTRL_DTDIR_Msk |		// 1: From card to controller.
		0 * SDMMC_DCTRL_DTMODE_Msk |		// 0: Block data transfer
		1 * SDMMC_DCTRL_DMAEN_Msk |
		lenpower * SDMMC_DCTRL_DBLOCKSIZE_0 |	// 9: 512 bytes, 3: 8 bytes
		//0 * SDMMC_DCTRL_RWSTART |
		//0 * SDMMC_DCTRL_RWSTOP |
		//1 * SDMMC_DCTRL_RWMOD |			// 1: Read Wait control using SDMMC_CK
		//0 * SDMMC_DCTRL_SDMMCEN |		// If this bit is set, the DPSM performs an SD I/O-card-specific operation.
		0;

#elif CPUSTYLE_STM32F4XX

	SDIO->DTIMER = 0x03FFFFFF;

	SDIO->DLEN = (len & SDIO_DLEN_DATALENGTH);

	SDIO->DCTRL =
		1 * SDIO_DCTRL_DTEN |		// Data transfer enabled bit
		! txmode * SDIO_DCTRL_DTDIR |		// 1: From card to controller.
		0 * SDIO_DCTRL_DTMODE |		// 0: Block data transfer
		1 * SDIO_DCTRL_DMAEN |
		lenpower * SDIO_DCTRL_DBLOCKSIZE_0 |	// 9: 512 bytes, 3: 8 bytes
		//0 * SDIO_DCTRL_RWSTART |
		//0 * SDIO_DCTRL_RWSTOP |
		//1 * SDIO_DCTRL_RWMOD |			// 1: Read Wait control using SDIO_CK
		//0 * SDIO_DCTRL_SDIOEN |		// If this bit is set, the DPSM performs an SD I/O-card-specific operation.
		0;

#else
	#error Wrong CPUSTYLE_xxx
#endif
}

// CPUSTYLE_R7S721 SD_CMD bits
// 
// SD_CMD[15:14]	MD7:MD6		Multiple Block Transfer Mode (enabled at multiple block transfer)
// SD_CMD[13]		MD5			Single/Multiple Block Transfer (enabled when the command with data is handled)
// SD_CMD[12]		MD4			Write/Read Mode (enabled when the command with data is handled)
// SD_CMD[11]		MD3			Data Mode (Command Type)
// SD_CMD[10..8]	MD2:MD0		Mode/Response Type
// SD_CMD[7..6]		C1:C0		00: CMD, 01: ACMD
// SD_CMD[5..0]		CF45:CF40	Command Index
#define R7S721_SD_CMD_ACMD_bm	(1U << 6)

static uint_fast16_t encode_cmd(uint_fast8_t cmd)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)
	return (cmd & 0x3f) | 0x40;

#elif CPUSTYLE_R7S721

	switch (cmd)
	{
	case 5:					// CMD5 SD_CMD_SDIO_SEN_OP_COND
		return 0x0705;

	case SD_CMD_HS_SWITCH:	// CMD6
		return 0x1C06;

	case 8:					// CMD8 SD_CMD_HS_SEND_EXT_CSD
		return 0x0408;

	case SD_CMD_SDIO_RW_DIRECT:	// CMD52
		return 0x0434;

	case SD_CMD_SDIO_RW_EXTENDED:	// command have four versions in depends of mode
		return cmd;

	default:
		break;
	}
#elif CPUSTYLE_STM32H7XX
	switch (cmd)
	{
	case SD_CMD_WRITE_SINGLE_BLOCK:
	case SD_CMD_WRITE_MULT_BLOCK:
	case SD_CMD_READ_SINGLE_BLOCK:
	case SD_CMD_READ_MULT_BLOCK:
		return cmd | SDMMC_CMD_CMDTRANS;
	case SD_CMD_STOP_TRANSMISSION:
		return cmd | SDMMC_CMD_CMDSTOP;
	default:
		break;
	}

#endif
	return cmd;
}

static uint_fast16_t encode_appcmd(uint_fast8_t cmd)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)
	return (cmd & 0x3f) | 0x40;

#elif CPUSTYLE_R7S721
	switch (cmd)
	{
	case SD_CMD_APP_SD_SET_BUSWIDTH:		// ACMD6
		return 6 | R7S721_SD_CMD_ACMD_bm;	// 0x0046

	case SD_CMD_SD_APP_STATUS:				// ACMD13
		return 13 | R7S721_SD_CMD_ACMD_bm;	// 0x004D

	case SD_CMD_SD_APP_SEND_NUM_WRITE_BLOCKS:// ACMD22
		return 22 | R7S721_SD_CMD_ACMD_bm;

	case 23:								// ACMD23
		return 23 | R7S721_SD_CMD_ACMD_bm;	// 0x0057

	case SD_CMD_SD_APP_OP_COND:				// ACMD41
		return 0x0069;

	case SD_CMD_SD_APP_SET_CLR_CARD_DETECT:	// ACMD42
		return 0x006A;

	case SD_CMD_SD_APP_SEND_SCR:			// ACMD51
		return 0x0073;

	default:
		return (cmd & 0x3f) | R7S721_SD_CMD_ACMD_bm;
	}
#elif CPUSTYLE_STM32H7XX
	switch (cmd)
	{
	case SD_CMD_SD_APP_SEND_SCR:
	case SD_CMD_SD_APP_STATUS:
		return cmd | SDMMC_CMD_CMDTRANS;
	default:
		break;
	}
#endif
	return cmd;
}

// ��������� �� ���������� �������, �� ������������ responce
static void sdhost_no_resp(uint_fast16_t cmd, uint_fast32_t arg)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)
	uint_fast8_t crc = 0x00;

	crc = crc7b8(crc, cmd);		
	crc = crc7b8(crc, arg >> 24);	
	crc = crc7b8(crc, arg >> 16);	
	crc = crc7b8(crc, arg >> 8);	
	crc = crc7b8(crc, arg >> 0);	

	SDCARD_CS_LOW();
	spi_progval8_p1(targetsdcard, 0xff);	// dummy byte
	spi_progval8_p2(targetsdcard, cmd);	// command
	spi_progval8_p2(targetsdcard, arg >> 24);
	spi_progval8_p2(targetsdcard, arg >> 16);
	spi_progval8_p2(targetsdcard, arg >> 8);
	spi_progval8_p2(targetsdcard, arg >> 0);
	spi_progval8_p2(targetsdcard, crc | 0x01);		// CRC and end transmit bit

	spi_complete(targetsdcard);
	SDCARD_CS_HIGH();

#elif CPUSTYLE_R7S721

	while ((SDHI0.SD_INFO2 & (1uL << 14)) != 0)	// CBSY
		; //debug_printf_P(PSTR("sdhost_no_resp: CBSY\n"));

	SDHI0.SD_INFO1_MASK = 0xFFFE;
	SDHI0.SD_INFO2_MASK = 0x7F80;

	// Issue command
	SDHI0.SD_ARG0 = arg >> 0;
	SDHI0.SD_ARG1 = arg >> 16;
	SDHI0.SD_CMD = cmd;				// ���������� ������� ���������� ����� ������ � ���� �������

#elif CPUSTYLE_STM32H7XX

	SDMMC1->ARG = arg;
	SDMMC1->CMD = 
		cmd |
		0 * SDMMC_CMD_WAITRESP_0 |	// 0: no response, 1: short response, 3: long response
		0 * SDMMC_CMD_WAITINT |
		0 * SDMMC_CMD_WAITPEND |
		1 * SDMMC_CMD_CPSMEN |
		//0 * SDMMC_CMD_SDIOSUSPEND |
		0;

#elif CPUSTYLE_STM32F7XX

	SDMMC1->ARG = arg;
	SDMMC1->CMD = 
		cmd |
		0 * SDMMC_CMD_WAITRESP_0 |	// 0: no response, 1: short response, 3: long response
		0 * SDMMC_CMD_WAITINT |
		0 * SDMMC_CMD_WAITPEND |
		1 * SDMMC_CMD_CPSMEN |
		//0 * SDMMC_CMD_SDIOSUSPEND |
		0;

#elif CPUSTYLE_STM32F4XX

	SDIO->ARG = arg;
	SDIO->CMD = 
		(SDIO_CMD_CMDINDEX & cmd) |
		0 * SDIO_CMD_WAITRESP_0 |	// 0: no response, 1: short response, 3: long response
		0 * SDIO_CMD_WAITINT |
		0 * SDIO_CMD_WAITPEND |
		1 * SDIO_CMD_CPSMEN |
		0 * SDIO_CMD_SDIOSUSPEND |
		0;

#else
	#error Wrong CPUSTYLE_xxx
#endif
}

// ��������� �� ���������� �������, ������������ short responce
static void sdhost_short_resp2(uint_fast16_t cmd, uint_fast32_t arg, uint_fast8_t dtransfer, uint_fast8_t nocrc)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)

	uint_fast8_t crc = 0x00;

	crc = crc7b8(crc, cmd);		
	crc = crc7b8(crc, arg >> 24);	
	crc = crc7b8(crc, arg >> 16);	
	crc = crc7b8(crc, arg >> 8);	
	crc = crc7b8(crc, arg >> 0);	

	SDCARD_CS_LOW();
	spi_progval8_p1(targetsdcard, 0xff);	// dummy byte
	spi_progval8_p2(targetsdcard, cmd);	// command
	spi_progval8_p2(targetsdcard, arg >> 24);
	spi_progval8_p2(targetsdcard, arg >> 16);
	spi_progval8_p2(targetsdcard, arg >> 8);
	spi_progval8_p2(targetsdcard, arg >> 0);
	spi_progval8_p2(targetsdcard, crc | 0x01);		// CRC and end transmit bit

	spi_complete(targetsdcard);
	SDCARD_CS_HIGH();

#elif CPUSTYLE_R7S721

	while ((SDHI0.SD_INFO2 & (1uL << 14)) != 0)	// CBSY
		; //debug_printf_P(PSTR("sdhost_short_resp2: CBSY\n"));

	SDHI0.SD_INFO1_MASK = 0xFFFE;
	SDHI0.SD_INFO2_MASK = 0x7F80;

	// Issue command
	SDHI0.SD_ARG0 = arg >> 0;
	SDHI0.SD_ARG1 = arg >> 16;
	SDHI0.SD_CMD = cmd;				// ���������� ������� ���������� ����� ������ � ���� �������

#elif CPUSTYLE_STM32H7XX

	SDMMC1->ARG = arg;
	SDMMC1->CMD = 
		cmd |
		(nocrc ? 2 : 1) * SDMMC_CMD_WAITRESP_0 |	// 0: no response, 1: short response, 2: short response no CRC, 3: long response
		0 * SDMMC_CMD_WAITINT |
		0 * SDMMC_CMD_WAITPEND |
		1 * SDMMC_CMD_CPSMEN |
		//0 * SDMMC_CMD_SDIOSUSPEND |
		0;

#elif CPUSTYLE_STM32F7XX

	SDMMC1->ARG = arg;
	SDMMC1->CMD = 
		cmd |
		1 * SDMMC_CMD_WAITRESP_0 |	// 0: no response, 1: short response, 3: long response
		0 * SDMMC_CMD_WAITINT |
		0 * SDMMC_CMD_WAITPEND |
		1 * SDMMC_CMD_CPSMEN |
		//0 * SDMMC_CMD_SDIOSUSPEND |
		0;

#elif CPUSTYLE_STM32F4XX

	SDIO->ARG = arg;
	SDIO->CMD = 
		(SDIO_CMD_CMDINDEX & cmd) |
		1 * SDIO_CMD_WAITRESP_0 |	// 0: no response, 1: short response, 3: long response
		0 * SDIO_CMD_WAITINT |
		0 * SDIO_CMD_WAITPEND |
		1 * SDIO_CMD_CPSMEN |
		0 * SDIO_CMD_SDIOSUSPEND |
		0;

#else
	#error Wrong CPUSTYLE_xxx
#endif
}

// ������� ��� ��������� ������
static void sdhost_short_resp(uint_fast16_t cmd, uint_fast32_t arg)
{
	sdhost_short_resp2(cmd, arg, 0, 0);
}

// ��������� �� ���������� �������, ������������ long responce
static void sdhost_long_resp(uint_fast16_t cmd, uint_fast32_t arg)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)

	uint_fast8_t crc = 0x00;

	crc = crc7b8(crc, cmd);		
	crc = crc7b8(crc, arg >> 24);	
	crc = crc7b8(crc, arg >> 16);	
	crc = crc7b8(crc, arg >> 8);	
	crc = crc7b8(crc, arg >> 0);	

	SDCARD_CS_LOW();
	spi_progval8_p1(targetsdcard, 0xff);	// dummy byte
	spi_progval8_p2(targetsdcard, cmd);	// command
	spi_progval8_p2(targetsdcard, arg >> 24);
	spi_progval8_p2(targetsdcard, arg >> 16);
	spi_progval8_p2(targetsdcard, arg >> 8);
	spi_progval8_p2(targetsdcard, arg >> 0);
	spi_progval8_p2(targetsdcard, crc | 0x01);		// CRC and end transmit bit

	spi_complete(targetsdcard);
	SDCARD_CS_HIGH();

#elif CPUSTYLE_R7S721

	while ((SDHI0.SD_INFO2 & (1uL << 14)) != 0)	// CBSY
		; //debug_printf_P(PSTR("sdhost_long_resp: CBSY\n"));

	SDHI0.SD_INFO1_MASK = 0xFFFE;
	SDHI0.SD_INFO2_MASK = 0x7F80;

	// Issue command
	SDHI0.SD_ARG0 = arg >> 0;
	SDHI0.SD_ARG1 = arg >> 16;
	SDHI0.SD_CMD = cmd;				// ���������� ������� ���������� ����� ������ � ���� �������

#elif CPUSTYLE_STM32H7XX

	SDMMC1->ARG = arg;
	SDMMC1->CMD = 
		cmd |
		3 * SDMMC_CMD_WAITRESP_0 |	// 0: no response, 1: short response, 3: long response
		0 * SDMMC_CMD_WAITINT |
		0 * SDMMC_CMD_WAITPEND |
		1 * SDMMC_CMD_CPSMEN |
		//0 * SDMMC_CMD_SDIOSUSPEND |
		0;

#elif CPUSTYLE_STM32F7XX

	SDMMC1->ARG = arg;
	SDMMC1->CMD = 
		cmd |
		3 * SDMMC_CMD_WAITRESP_0 |	// 0: no response, 1: short response, 3: long response
		0 * SDMMC_CMD_WAITINT |
		0 * SDMMC_CMD_WAITPEND |
		1 * SDMMC_CMD_CPSMEN |
		//0 * SDMMC_CMD_SDIOSUSPEND |
		0;

#elif CPUSTYLE_STM32F4XX

	SDIO->ARG = arg;
	SDIO->CMD = 
		(SDIO_CMD_CMDINDEX & cmd) |
		3 * SDIO_CMD_WAITRESP_0 |	// 0: no response, 1: short response, 3: long response
		0 * SDIO_CMD_WAITINT |
		0 * SDIO_CMD_WAITPEND |
		1 * SDIO_CMD_CPSMEN |
		0 * SDIO_CMD_SDIOSUSPEND |
		0;

#else
	#error Wrong CPUSTYLE_xxx
#endif
}

// �������� ���������� ������ � ����� �������
static uint_fast8_t sdhost_verify_resp(uint_fast16_t cmd)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)
	return 0;

#elif CPUSTYLE_R7S721

	return 0;

#elif CPUSTYLE_STM32F7XX || CPUSTYLE_STM32H7XX

	if ((SDMMC1->RESPCMD & SDMMC_RESPCMD_RESPCMD) != (cmd & SDMMC_CMD_CMDINDEX))
	{
		//debug_printf_P(PSTR("sdhost_verify_resp error, RESPCMD=%02lX, expeted %02lX\n"), SDMMC1->RESPCMD, cmd);
		return 1;
	}
	return 0;

#elif CPUSTYLE_STM32F4XX

	if ((SDIO->RESPCMD & SDIO_RESPCMD_RESPCMD) != (cmd & SDIO_CMD_CMDINDEX))
	{
		//debug_printf_P(PSTR("sdhost_verify_resp error, RESPCMD=%02lX, expeted %02lX\n"), SDIO->RESPCMD, cmd);
		return 1;
	}
	return 0;

#else
	#error Wrong CPUSTYLE_xxx
	return 1;
#endif
}
// ��������� ��������� ���������� ������� (��� ������ ������� � ��� ��������� ������) � ������� ��� ������
static uint_fast8_t sdhost_get_none_resp(void)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)
	SDCARD_CS_LOW();
	spi_read_byte(targetsdcard, 0xff);
	spi_read_byte(targetsdcard, 0xff);
	spi_read_byte(targetsdcard, 0xff);
	spi_read_byte(targetsdcard, 0xff);
	spi_read_byte(targetsdcard, 0xff);
	SDCARD_CS_HIGH();
	return 0;

#elif CPUSTYLE_R7S721

	uint_fast8_t ec = 0;

	while ((SDHI0.SD_INFO1 & (1U << 0)) == 0)	// INFO0
		;
	ec = (SDHI0.SD_ERR_STS2 & 0x007F) != 0;

	SDHI0.SD_INFO1 = (uint16_t) ~ (1U << 0);	// INFO0=0
	//SDHI0.SD_INFO1 = (uint16_t) ~ (1U << 2);	// INFO2=0

	//debug_printf_P(PSTR("Aftrer command w=%lu %04X %04X %04X\n"), w, SDHI0.SD_CMD, SDHI0.SD_ARG0, SDHI0.SD_ARG1);
	//debug_printf_P(PSTR("SD_INFO1=%04X, SD_INFO2=%04X\n"), SDHI0.SD_INFO1, SDHI0.SD_INFO2);	// SD_INFO1=0600, SD_INFO2=2080
	//debug_printf_P(PSTR("SD_ERR_STS1=%04X, SD_ERR_STS2=%04X\n"), SDHI0.SD_ERR_STS1, SDHI0.SD_ERR_STS2);	// SD_ERR_STS1=0000, SD_ERR_STS2=2000
	//debug_printf_P(PSTR("SD_SIZE=%04X\n"), SDHI0.SD_SIZE);
	
	return ec;

#elif CPUSTYLE_STM32F7XX || CPUSTYLE_STM32H7XX

	uint_fast8_t ec = 0;
	while (ec == 0)
	{
		const uint_fast32_t sta = SDMMC1->STA;
		if ((sta & SDMMC_STA_CTIMEOUT) != 0)
			ec = 1;
		if ((sta & SDMMC_STA_CMDSENT) != 0)
			break;
#if defined (SDMMC_STA_DTIMEOUT)
		if ((sta & SDMMC_STA_DTIMEOUT) != 0)
			ec = 1;
#endif /* defined (SDMMC_STA_DTIMEOUT) */
#if CPUSTYLE_STM32H7XX
		if ((sta & SDMMC_STA_BUSYD0END) != 0)
			break;
#endif /* CPUSTYLE_STM32H7XX */
	}
	
	SDMMC1->ICR = SDMMC_ICR_CMDSENTC;

	// ���� ���� ������ CRC ��� ����� ������ - �������� �
	if ((SDMMC1->STA & SDMMC_STA_CCRCFAIL) != 0)
		SDMMC1->ICR = SDMMC_ICR_CCRCFAILC;
	// ���� ���� ������ �������� ��� ����� ������ - �������� �
	if ((SDMMC1->STA & SDMMC_STA_CTIMEOUT) != 0)
		SDMMC1->ICR = SDMMC_ICR_CTIMEOUTC;

	SDMMC1->ICR = SDIO_STATIC_FLAGS;
	return 0;

#elif CPUSTYLE_STM32F4XX

	uint_fast8_t ec = 0;
	while (ec == 0)
	{
		const uint_fast32_t sta = SDIO->STA;
		if ((sta & SDIO_STA_CTIMEOUT) != 0)
			ec = 1;
		if ((sta & SDIO_STA_CMDSENT) != 0)
			break;
	}
	
	SDIO->ICR = SDIO_ICR_CMDSENTC;

	// ���� ���� ������ CRC ��� ����� ������ - �������� �
	if ((SDIO->STA & SDIO_STA_CCRCFAIL) != 0)
		SDIO->ICR = SDIO_ICR_CCRCFAILC;
	// ���� ���� ������ �������� ��� ����� ������ - �������� �
	if ((SDIO->STA & SDIO_STA_CTIMEOUT) != 0)
		SDIO->ICR = SDIO_ICR_CTIMEOUTC;

	SDIO->ICR = SDIO_STATIC_FLAGS;
	return 0;

#else
	#error Wrong CPUSTYLE_xxx
	return 1;
#endif
}

// ��������� ��������� ���������� ������� (��� ������ �������) � ������� ��� ������
static uint_fast8_t sdhost_get_resp(void)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)

	unsigned long i;
	uint_fast8_t response;

	SDCARD_CS_LOW();
	for (i = 0; i <= RESPLIMIT; ++ i)
	{
		response = spi_read_byte(targetsdcard, 0xff);
		if ((response & 0x80) != 0)
			continue;
		return 0;
	}
	SDCARD_CS_HIGH();
	return 1;

#elif CPUSTYLE_R7S721

	uint_fast8_t ec = 0;

	while ((SDHI0.SD_INFO1 & (1U << 0)) == 0)	// INFO0
		;
	ec = (SDHI0.SD_ERR_STS2 & 0x007F) != 0;

	SDHI0.SD_INFO1 = (uint16_t) ~ (1U << 0);	// INFO0=0
	//SDHI0.SD_INFO1 = (uint16_t) ~ (1U << 2);	// INFO2=0

	//debug_printf_P(PSTR("Aftrer command w=%lu %04X %04X %04X\n"), w, SDHI0.SD_CMD, SDHI0.SD_ARG0, SDHI0.SD_ARG1);
	//debug_printf_P(PSTR("SD_INFO1=%04X, SD_INFO2=%04X\n"), SDHI0.SD_INFO1, SDHI0.SD_INFO2);	// SD_INFO1=0600, SD_INFO2=2080
	//debug_printf_P(PSTR("SD_ERR_STS1=%04X, SD_ERR_STS2=%04X\n"), SDHI0.SD_ERR_STS1, SDHI0.SD_ERR_STS2);	// SD_ERR_STS1=0000, SD_ERR_STS2=2000
	//debug_printf_P(PSTR("SD_SIZE=%04X\n"), SDHI0.SD_SIZE);
	
	return ec;

#elif CPUSTYLE_STM32F7XX || CPUSTYLE_STM32H7XX

	uint_fast8_t ec = 0;
	while (ec == 0)
	{
		const uint_fast32_t sta = SDMMC1->STA;
		if ((sta & SDMMC_STA_CTIMEOUT) != 0)
			ec = 1;
		if ((sta & SDMMC_STA_CCRCFAIL) != 0)
			ec = 1;
		if ((sta & SDMMC_STA_CMDREND) != 0)
			break;
#if defined (SDMMC_STA_DTIMEOUT)
		if ((sta & SDMMC_STA_DTIMEOUT) != 0)
			ec = 1;
#endif /* defined (SDMMC_STA_DTIMEOUT) */
#if CPUSTYLE_STM32H7XX
		if ((sta & SDMMC_STA_BUSYD0END) != 0)
			break;
#endif /* CPUSTYLE_STM32H7XX */
	}
	if (ec != 0)
	{
		debug_printf_P(PSTR("sdhost_get_resp error, STA=%08lX, DCOUNT=%08lX\n"), SDMMC1->STA, SDMMC1->DCOUNT & SDMMC_DCOUNT_DATACOUNT);
	}
	SDMMC1->ICR = SDMMC_ICR_CMDRENDC;
	// ���� ���� ������ CRC ��� ����� ������ - �������� �
	if ((SDMMC1->STA & SDMMC_STA_CCRCFAIL) != 0)
		SDMMC1->ICR = SDMMC_ICR_CCRCFAILC;
	// ���� ���� ������ �������� ��� ����� ������ - �������� �
	if ((SDMMC1->STA & SDMMC_STA_CTIMEOUT) != 0)
		SDMMC1->ICR = SDMMC_ICR_CTIMEOUTC;
#if defined (SDMMC_STA_DTIMEOUT)
	if ((SDMMC1->STA & SDMMC_STA_DTIMEOUT) != 0)
		SDMMC1->ICR = SDMMC_ICR_DTIMEOUTC;
#endif /* defined (SDMMC_STA_DTIMEOUT) */

	SDMMC1->ICR = SDIO_STATIC_FLAGS;
	return ec;

#elif CPUSTYLE_STM32F4XX

	uint_fast8_t ec = 0;
	while (ec == 0)
	{
		const uint_fast32_t sta = SDIO->STA;
		if ((sta & SDIO_STA_CTIMEOUT) != 0)
			ec = 1;
		if ((sta & SDIO_STA_CCRCFAIL) != 0)
			ec = 1;
		if ((sta & SDIO_STA_CMDREND) != 0)
			break;
	}
	if (ec != 0)
	{
			//debug_printf_P(PSTR("sdhost_get_resp error, STA=%08lX\n"), SDIO->STA);
	}

	SDIO->ICR = SDIO_ICR_CMDRENDC;
	// ���� ���� ������ CRC ��� ����� ������ - �������� �
	if ((SDIO->STA & SDIO_STA_CCRCFAIL) != 0)
		SDIO->ICR = SDIO_ICR_CCRCFAILC;
	// ���� ���� ������ �������� ��� ����� ������ - �������� �
	if ((SDIO->STA & SDIO_STA_CTIMEOUT) != 0)
		SDIO->ICR = SDIO_ICR_CTIMEOUTC;

	SDIO->ICR = SDIO_STATIC_FLAGS;
	return ec;

#else
	#error Wrong CPUSTYLE_xxx
	return 1;
#endif
}

// ��������� ��������� ���������� ������� (��� ������ ������� � ��� ��������� ������) � ������� ��� ������
// ����� ���������� ��� �������������� CRC, ������� ������ ������ ������������.
// All responses, except for the R3 response type, are protected by a CRC.
static uint_fast8_t sdhost_get_resp_nocrc(void)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)

	unsigned long i;
	uint_fast8_t response;

	SDCARD_CS_LOW();
	for (i = 0; i <= RESPLIMIT; ++ i)
	{
		response = spi_read_byte(targetsdcard, 0xff);
		if ((response & 0x80) != 0)
			continue;
		return 0;
	}
	SDCARD_CS_HIGH();
	return 1;

#elif CPUSTYLE_R7S721

	uint_fast8_t ec = 0;

	while ((SDHI0.SD_INFO1 & (1U << 0)) == 0)	// INFO0
		;
	ec = (SDHI0.SD_ERR_STS2 & 0x007F) != 0;


	SDHI0.SD_INFO1 = (uint16_t) ~ (1U << 0);	// INFO0=0
	//SDHI0.SD_INFO1 = (uint16_t) ~ (1U << 2);	// INFO2=0

	//debug_printf_P(PSTR("Aftrer command w=%lu %04X %04X %04X\n"), w, SDHI0.SD_CMD, SDHI0.SD_ARG0, SDHI0.SD_ARG1);
	//debug_printf_P(PSTR("SD_INFO1=%04X, SD_INFO2=%04X\n"), SDHI0.SD_INFO1, SDHI0.SD_INFO2);	// SD_INFO1=0600, SD_INFO2=2080
	//debug_printf_P(PSTR("SD_ERR_STS1=%04X, SD_ERR_STS2=%04X\n"), SDHI0.SD_ERR_STS1, SDHI0.SD_ERR_STS2);	// SD_ERR_STS1=0000, SD_ERR_STS2=2000
	//debug_printf_P(PSTR("SD_SIZE=%04X\n"), SDHI0.SD_SIZE);
	
	return ec;

#elif CPUSTYLE_STM32F7XX || CPUSTYLE_STM32H7XX

	uint_fast8_t ec = 0;
	while (ec == 0)
	{
		const uint_fast32_t sta = SDMMC1->STA;
		if ((sta & SDMMC_STA_CTIMEOUT) != 0)
			ec = 1;
		if ((sta & SDMMC_STA_CCRCFAIL) != 0)
			break; //ec = 1;
		if ((sta & SDMMC_STA_CMDREND) != 0)
			break;
#if defined (SDMMC_STA_DTIMEOUT)
		if ((sta & SDMMC_STA_DTIMEOUT) != 0)
			ec = 1;
#endif /* defined (SDMMC_STA_DTIMEOUT) */
#if CPUSTYLE_STM32H7XX
		if ((sta & SDMMC_STA_BUSYD0END) != 0)
			break;
#endif /* CPUSTYLE_STM32H7XX */
	}

	SDMMC1->ICR = SDMMC_ICR_CMDRENDC;
	// ���� ���� ������ CRC ��� ����� ������ - �������� �
	if ((SDMMC1->STA & SDMMC_STA_CCRCFAIL) != 0)
		SDMMC1->ICR = SDMMC_ICR_CCRCFAILC;
	// ���� ���� ������ �������� ��� ����� ������ - �������� �
	if ((SDMMC1->STA & SDMMC_STA_CTIMEOUT) != 0)
		SDMMC1->ICR = SDMMC_ICR_CTIMEOUTC;

	SDMMC1->ICR = SDIO_STATIC_FLAGS;
	return ec;

#elif CPUSTYLE_STM32F4XX

	uint_fast8_t ec = 0;
	while (ec == 0)
	{
		const uint_fast32_t sta = SDIO->STA;
		if ((sta & SDIO_STA_CTIMEOUT) != 0)
			ec = 1;
		if ((sta & SDIO_STA_CCRCFAIL) != 0)
			break; //ec = 1;
		if ((sta & SDIO_STA_CMDREND) != 0)
			break;
	}

	SDIO->ICR = SDIO_ICR_CMDRENDC;
	// ���� ���� ������ CRC ��� ����� ������ - �������� �
	if ((SDIO->STA & SDIO_STA_CCRCFAIL) != 0)
		SDIO->ICR = SDIO_ICR_CCRCFAILC;
	// ���� ���� ������ �������� ��� ����� ������ - �������� �
	if ((SDIO->STA & SDIO_STA_CTIMEOUT) != 0)
		SDIO->ICR = SDIO_ICR_CTIMEOUTC;

	SDIO->ICR = SDIO_STATIC_FLAGS;
	return ec;

#else
	#error Wrong CPUSTYLE_xxx
	return 1;
#endif
}

static uint_fast32_t sdhost_get_resp32bit(void)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)
	unsigned long r = 0;
	r = r * 256 + spi_read_byte(targetsdcard, 0xff);
	r = r * 256 + spi_read_byte(targetsdcard, 0xff);
	r = r * 256 + spi_read_byte(targetsdcard, 0xff);
	r = r * 256 + spi_read_byte(targetsdcard, 0xff);
	SDCARD_CS_HIGH();
	return r;

#elif CPUSTYLE_R7S721
	// SD_RSP00: R23 to R8
	// SD_RSP01: R39 to R24
	// SD_RSP02: R55 to R40
	// SD_RSP03: R71 to R56
	// SD_RSP04: R87 to R72
	// SD_RSP05: R103 to R88
	// SD_RSP06: R119 to 104
	// SD_RSP07: R127 to 120 (8 bit)
	return ((uint32_t) SDHI0.SD_RSP01 << 16) | SDHI0.SD_RSP00;	// [39:8] OCR register

#elif CPUSTYLE_STM32F7XX || CPUSTYLE_STM32H7XX

	return SDMMC1->RESP1;	// Card Status [39:8]

#elif CPUSTYLE_STM32F4XX

	return SDIO->RESP1;		// Card Status[39:8]

#else
	#error Wrong CPUSTYLE_xxx
	return 0;
#endif
}

static void sdhost_get_resp128bit(uint8_t * resp128)
{
#if ! WITHSDHCHW
// SPI SD CARD (MMC SD)
	resp128 [0] = spi_read_byte(targetsdcard, 0xff);
	resp128 [1] = spi_read_byte(targetsdcard, 0xff);
	resp128 [2] = spi_read_byte(targetsdcard, 0xff);
	resp128 [3] = spi_read_byte(targetsdcard, 0xff);
	resp128 [4] = spi_read_byte(targetsdcard, 0xff);
	resp128 [5] = spi_read_byte(targetsdcard, 0xff);
	resp128 [6] = spi_read_byte(targetsdcard, 0xff);
	resp128 [7] = spi_read_byte(targetsdcard, 0xff);
	resp128 [8] = spi_read_byte(targetsdcard, 0xff);
	resp128 [9] = spi_read_byte(targetsdcard, 0xff);
	resp128 [10] = spi_read_byte(targetsdcard, 0xff);
	resp128 [11] = spi_read_byte(targetsdcard, 0xff);
	resp128 [12] = spi_read_byte(targetsdcard, 0xff);
	resp128 [13] = spi_read_byte(targetsdcard, 0xff);
	resp128 [14] = spi_read_byte(targetsdcard, 0xff);
	resp128 [15] = 0;				// CRC-7 and stop bit
	SDCARD_CS_HIGH();

#elif CPUSTYLE_R7S721
		//sdhost_get_R2: SD_RSP[127..8]=1D4144534420202002B0110DDA00E9
		// SD_RSP00: R23 to R8
		// SD_RSP01: R39 to R24
		// SD_RSP02: R55 to R40
		// SD_RSP03: R71 to R56
		// SD_RSP04: R87 to R72
		// SD_RSP05: R103 to R88
		// SD_RSP06: R119 to 104
		// SD_RSP07: R127 to 120 (8 bit)
		resp128 [0] = SDHI0.SD_RSP07;
		resp128 [1] = SDHI0.SD_RSP06 >> 8;
		resp128 [2] = SDHI0.SD_RSP06 >> 0;
		resp128 [3] = SDHI0.SD_RSP05 >> 8;
		resp128 [4] = SDHI0.SD_RSP05 >> 0;
		resp128 [5] = SDHI0.SD_RSP04 >> 8;
		resp128 [6] = SDHI0.SD_RSP04 >> 0;
		resp128 [7] = SDHI0.SD_RSP03 >> 8;
		resp128 [8] = SDHI0.SD_RSP03 >> 0;
		resp128 [9] = SDHI0.SD_RSP02 >> 8;
		resp128 [10] = SDHI0.SD_RSP02 >> 0;
		resp128 [11] = SDHI0.SD_RSP01 >> 8;
		resp128 [12] = SDHI0.SD_RSP01 >> 0;
		resp128 [13] = SDHI0.SD_RSP00 >> 8;
		resp128 [14] = SDHI0.SD_RSP00 >> 0;
		resp128 [15] = 0;				// CRC-7 and stop bit

#elif CPUSTYLE_STM32F7XX || CPUSTYLE_STM32H7XX

		// RESP1;Card Status [127:96]
		// RESP2;Card Status [95:64]
		// RESP3;Card Status [63:32]
		// RESP4;Card Status [31:1]0b
		
		resp128 [0] = SDMMC1->RESP1 >> 24;
		resp128 [1] = SDMMC1->RESP1 >> 16;
		resp128 [2] = SDMMC1->RESP1 >> 8;
		resp128 [3] = SDMMC1->RESP1 >> 0;

		resp128 [4] = SDMMC1->RESP2 >> 24;
		resp128 [5] = SDMMC1->RESP2 >> 16;
		resp128 [6] = SDMMC1->RESP2 >> 8;
		resp128 [7] = SDMMC1->RESP2 >> 0;

		resp128 [8] = SDMMC1->RESP3 >> 24;
		resp128 [9] = SDMMC1->RESP3 >> 16;
		resp128 [10] = SDMMC1->RESP3 >> 8;
		resp128 [11] = SDMMC1->RESP3 >> 0;

		resp128 [12] = SDMMC1->RESP4 >> 24;
		resp128 [13] = SDMMC1->RESP4 >> 16;
		resp128 [14] = SDMMC1->RESP4 >> 8;
		resp128 [15] = 0;				// CRC-7 and stop bit
#elif CPUSTYLE_STM32F4XX
		// RESP1;Card Status [127:96]
		// RESP2;Card Status [95:64]
		// RESP3;Card Status [63:32]
		// RESP4;Card Status [31:1]0b
		
		resp128 [0] = SDIO->RESP1 >> 24;
		resp128 [1] = SDIO->RESP1 >> 16;
		resp128 [2] = SDIO->RESP1 >> 8;
		resp128 [3] = SDIO->RESP1 >> 0;

		resp128 [4] = SDIO->RESP2 >> 24;
		resp128 [5] = SDIO->RESP2 >> 16;
		resp128 [6] = SDIO->RESP2 >> 8;
		resp128 [7] = SDIO->RESP2 >> 0;

		resp128 [8] = SDIO->RESP3 >> 24;
		resp128 [9] = SDIO->RESP3 >> 16;
		resp128 [10] = SDIO->RESP3 >> 8;
		resp128 [11] = SDIO->RESP3 >> 0;

		resp128 [12] = SDIO->RESP4 >> 24;
		resp128 [13] = SDIO->RESP4 >> 16;
		resp128 [14] = SDIO->RESP4 >> 8;
		resp128 [15] = 0;				// CRC-7 and stop bit
#else
	#error Wrong CPUSTYLE_xxx
#endif
}

// R1 (normal response command)
static uint_fast8_t sdhost_get_R1(uint_fast16_t cmd, uint_fast32_t * resp32)
{
	const uint_fast8_t ec = sdhost_get_resp();
	if (ec == 0)
	{
		if (sdhost_verify_resp(cmd) != 0)
			return 1;
		* resp32 = sdhost_get_resp32bit();
	}
	return ec;
}

// R7
static uint_fast8_t sdhost_get_R7(uint_fast16_t cmd, uint_fast32_t * resp32)
{
	const uint_fast8_t ec = sdhost_get_resp();
	if (ec == 0)
	{
		if (sdhost_verify_resp(cmd) != 0)
			return 1;
		* resp32 = sdhost_get_resp32bit();
	}
	return ec;
}


// R2 (CID, CSD register)
// Code length = 136 bits
static uint_fast8_t sdhost_get_R2(uint8_t * resp128)
{
	const uint_fast8_t ec = sdhost_get_resp();	/* �� ����������� ���������� command index */
	if (ec == 0)
	{
		sdhost_get_resp128bit(resp128);
	}
	return ec;
}

// R3 (OCR register)
// All responses, except for the R3 response type, are protected by a CRC.
static uint_fast8_t sdhost_get_R3(uint_fast32_t * resp32)
{
	const uint_fast8_t ec = sdhost_get_resp_nocrc();
	if (ec == 0)
	{
		* resp32 = sdhost_get_resp32bit();
	}
	return ec;
}

static uint_fast8_t sdhost_stop_transmission(void)
{
	uint_fast32_t resp;
	sdhost_short_resp(encode_cmd(SD_CMD_STOP_TRANSMISSION), 0);	// CMD12
	if (sdhost_get_R1(SD_CMD_STOP_TRANSMISSION, & resp) != 0)	// get R1b
	{
		debug_printf_P(PSTR("sdhost_stop_transmission error\n"));
		return 1;
	}
	return 0;
}


// ������� ����������.
static uint_fast8_t sdhost_sdcard_waitstatus(void)
{
	uint_fast8_t cardstate = 0xFF;
	unsigned long n;
	for (n = 1000000; -- n;)
	{
		uint_fast32_t resp;

		// get status
		sdhost_short_resp(encode_cmd(SD_CMD_SEND_STATUS), sdhost_sdcard_RCA << 16);	// CMD13
		if (sdhost_get_R1(SD_CMD_SEND_STATUS, & resp) != 0)
		{
			debug_printf_P(PSTR("sdhost_sdcard_waitstatus: SD_CMD_SEND_STATUS error\n"));
			return 1;
		}

		cardstate = (resp >> 9) & 0x0F;
		/*
			0 = Idle
			1 = Ready
			2 = Ident
			3 = Stby
			4 = Transfer
			5 = Data
			6 = Rcv
			7 = Programming
			8 = Disconnected
			9 = Btst
			10-15 = reserved
		*/
		switch (cardstate)
		{
		case 4:	// Transfer
		case 1:	// Ready
			return 0;

		case 7:	// Programming
			continue;	// ������� ��������� ������

		//case 6:	// Rcv
		//	continue;	// ������������� ��� ������������� ������� ������

		default:
			debug_printf_P(PSTR("sdhost_sdcard_waitstatus, resp=%08lX, cardstate=%u\n"), (unsigned long) resp, (unsigned) cardstate);
			return 1;
		}
	}
	debug_printf_P(PSTR("sdhost_sdcard_waitstatus, timeout error, cardstate=%u\n"), cardstate);
	return 1;
}

// �������� ��������� ��������� ��� ������� SD_CMD_READ_SINGLE_BLOCK/SD_CMD_WRITE_SINGLE_BLOCK
//  � ����������� �� ���� SD ����� ����� ��� LBA ��� �������� � ������
static uint_fast32_t sdhost_getaddresmultiplier(void)
{
	//if ((cmd58answer & 0x40000000) != 0)	//CCS (Card Capacity Status)
	if ((sdhost_SDType & SD_HIGH_CAPACITY) != 0)	//CCS (Card Capacity Status)
	{
		//debug_printf_P(PSTR("SDHC or SDXC (High Capacity)\n"));
		return 1; //mmcAddressMultiplier = 1;	// ��� SDHC (��������� ��� � 512-�� �������� ������)
	}
	else
	{
		//debug_printf_P(PSTR("SDSD - up to 2GB\n"));
		return 512; //mmcAddressMultiplier = MMC_SECTORSIZE;	// ��� ������� SD ����
	}
}

	/*
	if (1)
	{
		// 128 MB �����
		// set block length
		if (sdhost_sdcard_setblocklen(sdhost_getblocksize) != 0)
			return 1;
	}
	else if (sdhost_CardType == SDIO_HIGH_CAPACITY_SD_CARD)
	{
		// set block length
		if (sdhost_sdcard_setblocklen(512) != 0)
			return 1;
	}
	else
	{
		// set block length
		if (sdhost_sdcard_setblocklen(1) != 0)
			return 1;
	}
	*/

// �������� ��������� ��������� ��� ������� SD_CMD_SET_BLOCKLEN
//  � ����������� �� ���� ����� ������ ����� 1 ��� 512
/*
	In case of SDSC Card, block length is set by this command.
	In case of SDHC and SDXC Cards, block length of the memory access
	commands are fixed to 512 bytes. 
	The length of LOCK_UNLOCK command is set by this command
*/
static uint_fast32_t sdhost_getblocksize(void)
{
	return 512;
}


// ������ ���������� ������� APP_CMD � ��������� �� ��� ACMDxxx
// �������� R1 responce
static uint_fast8_t sdhost_short_acmd_resp_R1(uint_fast8_t cmd, uint_fast32_t arg, uint_fast32_t * resp, uint_fast8_t dtransfer)
{
	sdhost_short_resp(encode_cmd(SD_CMD_APP_CMD), sdhost_sdcard_RCA << 16);
	if (sdhost_get_R1(SD_CMD_APP_CMD, resp) != 0)
		return 1;
	sdhost_short_resp2(encode_appcmd(cmd), arg, dtransfer, 0);
	return sdhost_get_R1(cmd, resp);
}

// ������ ���������� ������� APP_CMD � ��������� �� ��� ACMDxxx
// �������� R3 responce
// All responses, except for the R3 response type, are protected by a CRC.
static uint_fast8_t sdhost_short_acmd_resp_R3(uint_fast8_t cmd, uint_fast32_t arg, uint_fast32_t * resp)
{
	sdhost_short_resp(encode_cmd(SD_CMD_APP_CMD), sdhost_sdcard_RCA << 16);
	if (sdhost_get_R1(SD_CMD_APP_CMD, resp) != 0)
		return 1;
	sdhost_short_resp2(encode_appcmd(cmd), arg, 0, 1);
	return sdhost_get_R3(resp);
}


#if WITHTEST_H7

static 
DRESULT SD_disk_write(
	BYTE drv,			/* Physical drive nmuber (0..) */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address (LBA) */
	UINT count			/* Number of sectors to write */
	)
{
	//Wait until card is ready for the data operations
	for (;;)
	{
		int t;
		//Make 10 tries to get card status before fail
		for(t = 0; SDSendCommand(13, sdhost_sdcard_RCA << 16, SD_CMD_SHORT_RESP) != SD_OK; t++)
		{
			if (t > 10)
				return RES_NOTRDY;
		}
		if ((SDMMC1->RESP1 & (SD_STAT_CURRENT_STATE|SD_STAT_READY_FOR_DATA)) == (SD_STAT_CURRENT_STATE_TRAN|SD_STAT_READY_FOR_DATA))break;
		local_delay_ms((1));
	}

	//debug_printf_P(" WR1:%x ", SDMMC1->RESP1);
	arm_hardware_flush((uintptr_t) buff, count * 512);
	DMA_SDIO_setparams((uintptr_t) buff, 512, count, 1);

	sdhost_dpsm_prepare((uintptr_t) buff, 1, 512 * count, 9);		// ���������� � ������ data path state machine - ��� ������ ����� ������ �������

	const uint_fast8_t cmd = (count == 1) ? 24 /*CMD24 single block write*/: 25 /*CMD25 multiple blocks write*/;
	if (SDSendCommand(cmd | SDMMC_CMD_CMDTRANS, sector * sdhost_getaddresmultiplier(), SD_CMD_SHORT_RESP))
	{
		//debug_printf_P("CMD%d failed!", cmd);
		//turn off DMA
		DMA_sdio_cancel();
		return RES_ERROR;
	}

	// Wait for data transfer finish
	sdhost_dpsm_wait(1);
	if (DMA_sdio_waitdone() != 0)
	{
		DMA_sdio_cancel();
		return RES_ERROR;
	}
	sdhost_dpsm_wait_fifo_empty();
	//DMA_sdio_cancel();

	return RES_OK;
}

#endif /* WITHTEST_H7 */

// write a size Byte big block beginning at the address.
static uint_fast8_t sdhost_sdcard_WriteSectors(
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address (LBA) */
	UINT count			/* Number of sectors to write */
	)
{
	//if ((unsigned long) buff & 0x1F)
	//{
	//	debug_printf_P(PSTR("sdhost_sdcard_WriteSectors: unalligned: buff=%p, sector=%lu, count=%lu\n"), buff, (unsigned long) sector, (unsigned long) count);
	//}
	//debug_printf_P(PSTR("sdhost_sdcard_WriteSectors: buff=%p, sector=%lu, count=%lu\n"), buff, (unsigned long) sector, (unsigned long) count);
	uint_fast32_t resp;
	
	if (sdhost_sdcard_waitstatus() != 0)
	{
		debug_printf_P(PSTR("sdhost_sdcard_WriteSectors: sdhost_sdcard_waitstatus error\n"));
		return 1;
	}

	if ((sdhost_SDType & SD_HIGH_CAPACITY) == 0)	//CCS (Card Capacity Status)
	{
		// set block length
		sdhost_short_resp(encode_cmd(SD_CMD_SET_BLOCKLEN), sdhost_getblocksize());	// CMD16
		if (sdhost_get_R1(SD_CMD_SET_BLOCKLEN, & resp) != 0)
		{
			debug_printf_P(PSTR("sdhost_sdcard_setblocklen error\n"));
			return 1;
		}
	}

	if (count == 1)
	{
		// wriite single block
		//debug_printf_P(PSTR("write single block\n"));
		// ������ ������������� DMA, ����� �������� ������� SD_CMD_WRITE_SINGLE_BLOCK
		// �������� � �� STM32Fxxx

		arm_hardware_flush_invalidate((uintptr_t) buff, 512 * count);	// ������ ��� ������ ����� ���������� �� DMA, ����� ���������� �� ���������
		DMA_SDIO_setparams((uintptr_t) buff, 512, count, 1);

		// write block
		sdhost_short_resp2(encode_cmd(SD_CMD_WRITE_SINGLE_BLOCK), sector * sdhost_getaddresmultiplier(), 1, 0);	// CMD24
		if (sdhost_get_R1(SD_CMD_WRITE_SINGLE_BLOCK, & resp) != 0)
		{
			DMA_sdio_cancel();
			debug_printf_P(PSTR("SD_CMD_WRITE_SINGLE_BLOCK error\n"));
			return 1;
		}

		sdhost_dpsm_prepare((uintptr_t) buff, 1, 512 * count, 9);		// ���������� � ������ data path state machine - ��� ������ ����� ������ �������

		if (sdhost_dpsm_wait(1) != 0)
		{
			DMA_sdio_cancel();
			debug_printf_P(PSTR("sdhost_sdcard_WriteSectors 1: sdhost_dpsm_wait error\n"));
			return 1;
		}
		else if (DMA_sdio_waitdone() != 0)
		{
			DMA_sdio_cancel();
			debug_printf_P(PSTR("sdhost_sdcard_WriteSectors 1: DMA_sdio_waitdone error\n"));
			return 1;
		}
		else
		{
			sdhost_dpsm_wait_fifo_empty();
		}
	}
	else
	{
		// write multiblock
		//debug_printf_P(PSTR("write multiblock, count=%d\n"), count);

		// Pre-erased Setting prior to a Multiple Block Write Operation
		// Setting a number of write blocks to be pre-erased (ACMD23)
		if (sdhost_short_acmd_resp_R1(SD_CMD_SD_APP_SET_NWB_PREERASED, count & 0x7FFFFF, & resp, 0) != 0) // ACMD23
		{
			debug_printf_P(PSTR("SD_CMD_SD_APP_SET_NWB_PREERASED error\n"));
			return 1;
		}
		//debug_printf_P(PSTR("SD_CMD_SD_APP_SET_NWB_PREERASED okay\n"));

		// write multiblock
		// ������ ������������� DMA, ����� �������� ������� SD_CMD_WRITE_MULT_BLOCK
		// �������� � �� STM32Fxxx
		arm_hardware_flush_invalidate((uintptr_t) buff, 512 * count);	// ������ ��� ������ ����� ���������� �� DMA, ����� ���������� �� ���������
		DMA_SDIO_setparams((uintptr_t) buff, 512, count, 1);
		
		if (sdhost_use_cmd23 != 0)
		{
			// set block count
			sdhost_short_resp(encode_cmd(SD_CMD_SET_BLOCK_COUNT), count);	// CMD23
			if (sdhost_get_R1(SD_CMD_SET_BLOCK_COUNT, & resp) != 0)	// get R1
			{
				DMA_sdio_cancel();
				debug_printf_P(PSTR("SD_CMD_SET_BLOCK_COUNT error\n"));
				return 1;
			}
		}

		// write blocks
		sdhost_short_resp2(encode_cmd(SD_CMD_WRITE_MULT_BLOCK), sector * sdhost_getaddresmultiplier(), 1, 0);	// CMD25
		if (sdhost_get_R1(SD_CMD_WRITE_MULT_BLOCK, & resp) != 0)
		{
			DMA_sdio_cancel();
			debug_printf_P(PSTR("SD_CMD_WRITE_MULT_BLOCK error\n"));
			return 1;
		}

		sdhost_dpsm_prepare((uintptr_t) buff, 1, 512 * count, 9);		// ���������� � ������ data path state machine - ��� ������ ����� ������ �������

		if (sdhost_dpsm_wait(1) != 0)
		{
			debug_printf_P(PSTR("sdhost_sdcard_WriteSectors: sdhost_dpsm_wait error\n"));
			DMA_sdio_cancel();
			if (sdhost_stop_transmission() != 0)
				debug_printf_P(PSTR("sdhost_sdcard_WriteSectors 2: sdhost_sdcard_waitstatus error\n"));
			return 1;
		}
		else if (DMA_sdio_waitdone() != 0)
		{
			DMA_sdio_cancel();
			debug_printf_P(PSTR("sdhost_sdcard_WriteSectors 2: DMA_sdio_waitdone error\n"));
			return 1;
		}
		else
		{
			sdhost_dpsm_wait_fifo_empty();

			#if ! CPUSTYLE_R7S721
			// � ���������� CPUSTYLE_R7S721 ������� CMD12 ����������� �����������
			if (sdhost_use_cmd23 == 0)
			{
				if (sdhost_stop_transmission() != 0)
					debug_printf_P(PSTR("sdhost_sdcard_WriteSectors 2: sdhost_sdcard_waitstatus error\n"));
			}
			#endif /* ! CPUSTYLE_R7S721 */
		}
		//debug_printf_P(PSTR("write multiblock, count=%d okay\n"), count);
	}
	//debug_printf_P(PSTR("sdhost_sdcard_WriteSectors: buff=%p, sector=%lu, count=%lu okay\n"), buff, (unsigned long) sector, (unsigned long) count);
	return 0;
}


#if WITHTEST_H7

static DRESULT 
SD_disk_read(
	BYTE drv,			/* Physical drive nmuber (0..) */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address (LBA) */
	UINT count		/* Number of sectors to read */
	)
{
   //Wait until card is ready for the data operations
   for(;;)
   {
	   int t;
      //Make ten tries to retrive card status before fail
      for(t = 0; SDSendCommand(13, sdhost_sdcard_RCA << 16, SD_CMD_SHORT_RESP) != SD_OK; t++)
      {
         if (t > 10)
			 return RES_NOTRDY;
      }
      if ((SDMMC1->RESP1 & (SD_STAT_CURRENT_STATE|SD_STAT_READY_FOR_DATA)) == (SD_STAT_CURRENT_STATE_TRAN|SD_STAT_READY_FOR_DATA))
		  break;
      local_delay_ms((1));
   }

   arm_hardware_flush_invalidate((uintptr_t) buff, 512 * count);
	DMA_SDIO_setparams((uintptr_t) buff, 512, count, 0);


	sdhost_dpsm_prepare((uintptr_t) buff, 0, 512 * count, 9);		// ���������� � ������ data path state machine - ��� ������ ����� ������ �������

	uint8_t cmd = (count == 1) ? 17 /*CMD17 single block read*/: 18 /*CMD18 multiple blocks read*/;
	if (SDSendCommand(cmd | SDMMC_CMD_CMDTRANS, sector * sdhost_getaddresmultiplier(), SD_CMD_SHORT_RESP))
	{
		//debug_printf_P("CMD%d failed!", cmd);
		//turn off DMA
		DMA_sdio_cancel();
		return RES_ERROR;
	}
	//Wait for data transfer finish
	sdhost_dpsm_wait(0);
	if (DMA_sdio_waitdone() != 0)
	{
		DMA_sdio_cancel();
		return RES_ERROR;
	}
	sdhost_dpsm_wait_fifo_empty();
	//DMA_sdio_cancel();
	return RES_OK;
}
#endif /* WITHTEST_H7 */

// read a size Byte big block beginning at the address.
static uint_fast8_t sdhost_sdcard_ReadSectors(
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address (LBA) */
	UINT count		/* Number of sectors to read */
	)
{
	//if ((unsigned long) buff & 0x1F)
	//{
	//	debug_printf_P(PSTR("sdhost_sdcard_ReadSectors: unalligned: buff=%p, sector=%lu, count=%lu\n"), buff, (unsigned long) sector, (unsigned long) count);
	//}
	//debug_printf_P(PSTR("sdhost_sdcard_ReadSectors: buff=%p, sector=%lu, count=%lu\n"), buff, (unsigned long) sector, (unsigned long) count);
	uint_fast32_t resp;
	
	if (sdhost_sdcard_waitstatus() != 0)
	{
		debug_printf_P(PSTR("sdhost_sdcard_ReadSectors: sdhost_sdcard_waitstatus error\n"));
		return 1;
	}
	
	
	//debug_printf_P(PSTR("sdhost_sdcard_ReadSectors: sdhost_CardType=%08lX, sdhost_SDType=%08lX\n"), (unsigned long) sdhost_CardType, (unsigned long) sdhost_SDType);

	if ((sdhost_SDType & SD_HIGH_CAPACITY) == 0)	//CCS (Card Capacity Status)
	{
		// set block length
		sdhost_short_resp(encode_cmd(SD_CMD_SET_BLOCKLEN), sdhost_getblocksize());	// CMD16
		if (sdhost_get_R1(SD_CMD_SET_BLOCKLEN, & resp) != 0)
		{
			debug_printf_P(PSTR("sdhost_sdcard_setblocklen error\n"));
			return 1;
		}
	}


	if (count == 1)
	{
		//debug_printf_P(PSTR("read one block\n"));
		// read one block
		sdhost_dpsm_prepare((uintptr_t) buff, 0, 512 * count, 9);		// ���������� � ������ data path state machine - ��� ������ ����� ������� �������
		//TP();
		//arm_hardware_invalidate((uint32_t) buff, 512 * count);		// ������ � ��� ������ ����� ������ �� DMA
		/* �������, �� �������! */arm_hardware_flush_invalidate((uintptr_t) buff, 512 * count);	// ������ ��� ������ ����� ���������� �� DMA, ����� ���������� �� ���������
		DMA_SDIO_setparams((uintptr_t) buff, 512, count, 0);
		//TP();

		// read block
		sdhost_short_resp2(encode_cmd(SD_CMD_READ_SINGLE_BLOCK), sector * sdhost_getaddresmultiplier(), 1, 0);	// CMD17
		if (sdhost_get_R1(SD_CMD_READ_SINGLE_BLOCK, & resp) != 0)
		{
			DMA_sdio_cancel();
			debug_printf_P(PSTR("SD_CMD_READ_SINGLE_BLOCK error\n"));
			return 1;
		}
		if (sdhost_dpsm_wait(0) != 0)
		{
			DMA_sdio_cancel();
			debug_printf_P(PSTR("sdhost_sdcard_ReadSectors 1: sdhost_dpsm_wait error\n"));
			return 1;
		}
		else if (DMA_sdio_waitdone() != 0)
		{
			DMA_sdio_cancel();
			debug_printf_P(PSTR("sdhost_sdcard_ReadSectors 1: DMA_sdio_waitdone error\n"));
			return 1;
		}
		else
		{
			sdhost_dpsm_wait_fifo_empty();
			return 0;
		}
	}
	else
	{
		//debug_printf_P(PSTR("read multiple blocks: count=%d\n"), count);
		// read multiple blocks
		sdhost_dpsm_prepare((uintptr_t) buff, 0, 512 * count, 9);		// ���������� � ������ data path state machine - ��� ������ ����� ������� �������
		//arm_hardware_invalidate((uint32_t) buff, 512 * count);		// ������ � ��� ������ ����� ������ �� DMA
		/* �������, �� �������! */arm_hardware_flush_invalidate((uintptr_t) buff, 512 * count);	// ������ ��� ������ ����� ���������� �� DMA, ����� ���������� �� ���������
		DMA_SDIO_setparams((uintptr_t) buff, 512, count, 0);
		if (sdhost_use_cmd23 != 0)
		{
			// set block count
			sdhost_short_resp(encode_cmd(SD_CMD_SET_BLOCK_COUNT), count);	// CMD23
			if (sdhost_get_R1(SD_CMD_SET_BLOCK_COUNT, & resp) != 0)	// get R1
			{
				DMA_sdio_cancel();
				debug_printf_P(PSTR("SD_CMD_SET_BLOCK_COUNT error\n"));
				return 1;
			}
		}
		// read block
		sdhost_short_resp2(encode_cmd(SD_CMD_READ_MULT_BLOCK), sector * sdhost_getaddresmultiplier(), 1, 0);	// CMD18
		if (sdhost_get_R1(SD_CMD_READ_MULT_BLOCK, & resp) != 0)
		{
			DMA_sdio_cancel();
			debug_printf_P(PSTR("SD_CMD_READ_MULT_BLOCK error\n"));
			return 1;
		}
		if (sdhost_dpsm_wait(0) != 0)
		{
			debug_printf_P(PSTR("sdhost_sdcard_ReadSectors 2: sdhost_dpsm_wait error\n"));
			DMA_sdio_cancel();
			if (sdhost_stop_transmission() != 0)
				debug_printf_P(PSTR("sdhost_sdcard_ReadSectors 2: sdhost_stop_transmission error\n"));
			return 1;
		}
		else if (DMA_sdio_waitdone() != 0)
		{
			DMA_sdio_cancel();
			debug_printf_P(PSTR("sdhost_sdcard_ReadSectors 2: DMA_sdio_waitdone error\n"));
			return 1;
		}
		else
		{
			sdhost_dpsm_wait_fifo_empty();
			#if ! CPUSTYLE_R7S721
			// � ���������� CPUSTYLE_R7S721 ������� CMD12 ����������� �����������
			if (sdhost_use_cmd23 == 0)
			{
				if (sdhost_stop_transmission() != 0)
					debug_printf_P(PSTR("sdhost_sdcard_ReadSectors 2: sdhost_sdcard_waitstatus error\n"));
			}
			#endif /* ! CPUSTYLE_R7S721 */
			return 0;
		}
	}
}

static uint_fast8_t sdhost_sdcard_checkversion(void)
{
	uint_fast32_t resp;

	const unsigned COUNTLIMIT = 16;
	unsigned count;
	for (count = 0; count < COUNTLIMIT; ++ count)
	{
		sdhost_no_resp(encode_cmd(SD_CMD_GO_IDLE_STATE), 0x00000000uL);	// CMD0
		sdhost_get_none_resp();

		sdhost_short_resp(encode_cmd(SDIO_SEND_IF_COND), SD_CHECK_PATTERN);	// CMD8 -> R7 (Card interface condition)
		if (sdhost_get_R7(SDIO_SEND_IF_COND, & resp) == 0)	// R7: (Card interface condition)
		{
			sdhost_SDType = SD_HIGH_CAPACITY;
			sdhost_CardType = SDIO_STD_CAPACITY_SD_CARD_V2_0; /*!< SD Card 2.0 */

			debug_printf_P(PSTR("SD CARD is V2, R1 resp: stuff=%08lX\n"), resp);
			return 0;
		}
	}
	/* �� ����������... */

	sdhost_SDType = SD_STD_CAPACITY;
	sdhost_CardType = SDIO_STD_CAPACITY_SD_CARD_V1_1; /*!< SD Card 1.0 */

	sdhost_short_resp(encode_cmd(SD_CMD_APP_CMD), sdhost_sdcard_RCA << 16);	// CMD55 APP_CMD
	if (sdhost_get_R1(SD_CMD_APP_CMD, & resp) != 0)
	{
		
		debug_printf_P(PSTR("sdhost_sdcard_checkversion failure\n"));
		return 1;
	}
	debug_printf_P(PSTR("SD CARD is V1, R1 resp: stuff=%08lX\n"), resp);
	return 0;
}

static uint_fast8_t sdhost_sdcard_poweron(void)
{
	uint_fast32_t resp;

	debug_printf_P(PSTR("SD CARD power on start\n"));
	sdhost_sdcard_RCA = 0;


	if (sdhost_sdcard_checkversion() != 0)
		return 1;

	debug_printf_P(PSTR("Set voltage conditions\n"));

	const unsigned COUNTLIMIT = 10000;
	unsigned count;
	for (count = 0; count < COUNTLIMIT; ++ count)
	{
#if WITHSDHCHW
		if (sdhost_short_acmd_resp_R3(SD_CMD_SD_APP_OP_COND, SD_VOLTAGE_WINDOW_SD | sdhost_SDType, & resp) != 0)	// ACMD41
		{
			debug_printf_P(PSTR("voltage send process: sdhost_short_acmd_resp_R3(SD_CMD_SD_APP_OP_COND) failure\n"));
			return 1;
		}
#else /* WITHSDHCHW */
		sdhost_short_resp(encode_cmd(58), 0);	// CMD58
		if (sdhost_get_R3(& resp) != 0)	// Response of ACMD41 (R3)
		{
			debug_printf_P(PSTR("Set voltage conditions error\n"));
			return 1;
		}
#endif /* WITHSDHCHW */
 		//debug_printf_P(PSTR("voltage send waiting: R3 resp: stuff=%08lX\n"), resp);
		if ((resp & (1UL << 31)) == 0)	// check for voltage range is okay
			continue;
		debug_printf_P(PSTR("voltage send okay: R3 resp: stuff=%08lX\n"), resp);
		if ((resp & SD_HIGH_CAPACITY) != 0)
		{
            sdhost_CardType = SDIO_HIGH_CAPACITY_SD_CARD;
			debug_printf_P(PSTR("SD CARD is high capacity\n"));
		}
		debug_printf_P(PSTR("SD CARD power on done, no errors\n"));
		return 0;
	}
	debug_printf_P(PSTR("SD CARD power on done, error\n"));
	return 1;
}

static uint_fast8_t sdhost_read_registers_acmd(uint16_t acmd, uint8_t * buff, unsigned size, unsigned lenpower, unsigned sizeofarray)
{
	uint_fast32_t resp;
	
	ASSERT(size == (1U << lenpower));
	if (sdhost_sdcard_waitstatus() != 0)
	{
		debug_printf_P(PSTR("sdhost_read_registers_acmd: sdhost_sdcard_waitstatus error\n"));
		return 1;
	}
	
	//debug_printf_P(PSTR("sdhost_read_registers_acmd: sdhost_CardType=%08lX, sdhost_SDType=%08lX\n"), (unsigned long) sdhost_CardType, (unsigned long) sdhost_SDType);

	sdhost_dpsm_prepare((uintptr_t) buff, 0, size, lenpower);		// ���������� � ������ data path state machine - ��� ����������� ������� �������
	//arm_hardware_invalidate((uint32_t) buff, size * 1);		// ������ � ��� ������ ����� ������ �� DMA
	/* �������, �� �������! */arm_hardware_flush_invalidate((uintptr_t) buff, sizeofarray);	// ������ ��� ������ ����� ���������� �� DMA, ����� ���������� �� ���������
	DMA_SDIO_setparams((uintptr_t) buff, size, 1, 0);

	// read block
	if (sdhost_short_acmd_resp_R1(acmd, 0, & resp, 1) != 0)	// ACMD51
	{
		DMA_sdio_cancel();
		debug_printf_P(PSTR("sdhost_read_registers_acmd: sdhost_get_R1 (acmd=0x%02lX) error\n"), acmd);
		return 1;
	}
	if (sdhost_dpsm_wait(0) != 0)
	{
		DMA_sdio_cancel();
		debug_printf_P(PSTR("sdhost_read_registers_acmd: sdhost_dpsm_wait error\n"));
		return 1;
	}
	else if (DMA_sdio_waitdone() != 0)
	{
		DMA_sdio_cancel();
		debug_printf_P(PSTR("sdhost_read_registers_acmd: DMA_sdio_waitdone error\n"));
		return 1;
	}
	else
	{
		sdhost_dpsm_wait_fifo_empty();
		return 0;
	}
}

static uint_fast8_t sdhost_sdcard_identification(void)
{
	uint_fast32_t resp;

	debug_printf_P(PSTR("SD CARD identification start\n"));

	sdhost_long_resp(encode_cmd(SD_CMD_ALL_SEND_CID), 0);	// CMD2
	if (sdhost_get_R2(sdhost_sdcard_CID) != 0)
	{
		debug_printf_P(PSTR("SD_CMD_ALL_SEND_CID error\n"));
		return 1;
	}

#if WITHSDHCHW

	sdhost_short_resp(encode_cmd(SD_CMD_SET_REL_ADDR), 0);	// CMD3
	if (sdhost_get_R1(SD_CMD_SET_REL_ADDR, & resp) != 0)
	{
		debug_printf_P(PSTR("SD_CMD_SET_REL_ADDR error\n"));
		return 1;
	}
	else
	{
		sdhost_sdcard_RCA = 0xFFFF & (resp >> 16);
		debug_printf_P(PSTR("RCA=%08lX\n"), sdhost_sdcard_RCA);
	}
#endif /* WITHSDHCHW */

	// Get CSD
	sdhost_long_resp(encode_cmd(SD_CMD_SEND_CSD), sdhost_sdcard_RCA << 16);	// CMD9 The contents of the CSD register are sent as a response to CMD9.
	if (sdhost_get_R2(sdhost_sdcard_CSD) != 0)
	{
		debug_printf_P(PSTR("SD_CMD_SEND_CSD error\n"));
		return 1;
	}
	debug_printf_P(PSTR("SD_CMD_SEND_CSD okay\n"));

#if WITHSDHCHW
	// Select card
	sdhost_short_resp(encode_cmd(SD_CMD_SEL_DESEL_CARD), sdhost_sdcard_RCA << 16);
	if (sdhost_get_R1(SD_CMD_SEL_DESEL_CARD, & resp) != 0)
	{
		debug_printf_P(PSTR("SD_CMD_SEL_DESEL_CARD error\n"));
		return 1;
	}
	debug_printf_P(PSTR("SD_CMD_SEL_DESEL_CARD okay\n"));
#endif /* WITHSDHCHW */

	// Get SCR
#if WITHSDHCHW4BIT
	uint_fast8_t bussupport4b = 1;
	uint_fast8_t bussupport1b = 1;
#else /* WITHSDHCHW4BIT */
	uint_fast8_t bussupport4b = 0;
	uint_fast8_t bussupport1b = 1;
#endif /* WITHSDHCHW4BIT */
	sdhost_use_cmd23 = 0;
	sdhost_use_cmd20 = 0;
#if 0 && WITHSDHCHW
	static RAMNOINIT_D1 ALIGNX_BEGIN uint8_t sdhost_sdcard_SCR [32] ALIGNX_END;	// ���� ������ 8 ����, �� �����-�� �������� � ��� - �������� ��� 32 � �����

	if (sdhost_read_registers_acmd(SD_CMD_SD_APP_SEND_SCR, sdhost_sdcard_SCR, 8, 3, sizeof sdhost_sdcard_SCR) == 0)		// ACMD51
	{
		debug_printf_P(PSTR("SD_CMD_SD_APP_SEND_SCR okay: SCR=%02X%02X%02X%02X%02X%02X%02X%02X\n"), 
			sdhost_sdcard_SCR [0], sdhost_sdcard_SCR [1], sdhost_sdcard_SCR [2], sdhost_sdcard_SCR [3], sdhost_sdcard_SCR [4], sdhost_sdcard_SCR [5], sdhost_sdcard_SCR [6], sdhost_sdcard_SCR [7]
			);

		// sdhost_sdcard_SCR [0]: 63..56
		// sdhost_sdcard_SCR [1]: 55..48
		// sdhost_sdcard_SCR [2]: 47..40
		// sdhost_sdcard_SCR [3]: 39..32
		// sdhost_sdcard_SCR [4]: 31..24
		// sdhost_sdcard_SCR [5]: 23..16
		// sdhost_sdcard_SCR [6]: 15..8
		// sdhost_sdcard_SCR [7]: 7..0
		debug_printf_P(PSTR("SCR Structure=0x%02lX\n"), array_get_bits(sdhost_sdcard_SCR, 64, 63, 4));			// [63:60]
		debug_printf_P(PSTR("SD Memory Card - Spec. Version=0x%02lX\n"), array_get_bits(sdhost_sdcard_SCR, 64, 63, 4));	// [59:56]
		debug_printf_P(PSTR("SD Memory Card - Spec3. Version=0x%02lX\n"), array_get_bits(sdhost_sdcard_SCR, 64, 47, 1));	// [47]
		debug_printf_P(PSTR("SD Memory Card - ext security=0x%02lX\n"), array_get_bits(sdhost_sdcard_SCR, 64, 46, 4));	// [46:43]
		debug_printf_P(PSTR("DAT Bus widths supported=0x%02lX\n"), array_get_bits(sdhost_sdcard_SCR, 64, 51, 4));	// [51:48]
		debug_printf_P(PSTR("CPRM Security Support=0x%02lX\n"), array_get_bits(sdhost_sdcard_SCR, 64, 54, 3));			// [54:52]
		debug_printf_P(PSTR("CMD Support=0x%02lX\n"), array_get_bits(sdhost_sdcard_SCR, 64, 33, 2));			// [33:32]

		bussupport1b = array_get_bits(sdhost_sdcard_SCR, 64, 48, 1); //(sdhost_sdcard_SCR [1] & 0x01) != 0;
		bussupport4b = array_get_bits(sdhost_sdcard_SCR, 64, 51, 1); //(sdhost_sdcard_SCR [1] & 0x04) != 0;
		sdhost_use_cmd20 = array_get_bits(sdhost_sdcard_SCR, 64, 32, 1); //(sdhost_sdcard_SCR [3] & 0x01) != 0;
		#if ! CPUSTYLE_R7S721
		sdhost_use_cmd23 = array_get_bits(sdhost_sdcard_SCR, 64, 33, 1); //(sdhost_sdcard_SCR [3] & 0x02) != 0;
		#endif /* ! CPUSTYLE_R7S721 */
	}
#endif /* WITHSDHCHW */

#if WITHSDHCHW

#if WITHSDHCHW4BIT
	if (bussupport4b != 0)
	{
		// Set 4 bit bus width
		if (sdhost_short_acmd_resp_R1(SD_CMD_APP_SD_SET_BUSWIDTH, 0x0002, & resp, 0) != 0) // ACMD6
		{
			debug_printf_P(PSTR("SD_CMD_APP_SD_SET_BUSWIDTH error\n"));
			return 1;
		}
		debug_printf_P(PSTR("SD_CMD_APP_SD_SET_BUSWIDTH okay\n"));
		hardware_sdhost_setbuswidth(1);		// 4-bit width
	}
	else if (bussupport1b != 0)
		hardware_sdhost_setbuswidth(0);		// 1-bit width
	else
		return 1;

#else /* WITHSDHCHW4BIT */

	if (bussupport1b == 0)
		return 1;

#endif /* WITHSDHCHW4BIT */
#endif /* WITHSDHCHW */

	// Get SD status
#if 0 && WITHSDHCHW
	static RAMNOINIT_D1 ALIGNX_BEGIN uint8_t sdhost_sdcard_SDSTATUS [64] ALIGNX_END;
	if (sdhost_read_registers_acmd(SD_CMD_SD_APP_STATUS, sdhost_sdcard_SDSTATUS, 64, 6, sizeof sdhost_sdcard_SDSTATUS) == 0)		// ACMD13
	{

		debug_printf_P(PSTR("SECURED_MODE=%02lx\n"), array_get_bits(sdhost_sdcard_SDSTATUS, 512, 509, 1));
		debug_printf_P(PSTR("DAT_BUS_WIDTH=%02lx\n"), array_get_bits(sdhost_sdcard_SDSTATUS, 512, 511, 2));
		debug_printf_P(PSTR("SD_CARD_TYPE=%04lx\n"), array_get_bits(sdhost_sdcard_SDSTATUS, 512, 495, 16));

		debug_printf_P(PSTR("SIZE_OF_PROTECTED_AREA=%08lx\n"), array_get_bits(sdhost_sdcard_SDSTATUS, 512, 479, 32));

		debug_printf_P(PSTR("SPEED_CLASS=%02lx\n"), array_get_bits(sdhost_sdcard_SDSTATUS, 512, 447, 8));
		debug_printf_P(PSTR("PERFORMANCE_MOVE=%02lx\n"), array_get_bits(sdhost_sdcard_SDSTATUS, 512, 439, 8));
		debug_printf_P(PSTR("AU_SIZE=%02lx\n"), array_get_bits(sdhost_sdcard_SDSTATUS, 512, 431, 4));
		debug_printf_P(PSTR("ERASE_SIZE=%04x\n"), array_get_bits(sdhost_sdcard_SDSTATUS, 512, 423, 16));

	}
#endif /* WITHSDHCHW */
	debug_printf_P(PSTR("SD CARD identification done\n"));
	return 0;
}

static 
char sd_initialize2(void)
{
	sd_power_cycle();

	debug_printf_P(PSTR("hardware_sdhost_setspeed to 400 kHz\n"));
	hardware_sdhost_setspeed(400000uL);
	hardware_sdhost_setbuswidth(0);		// 1-bit width
	if (sdhost_sdcard_poweron() == 0)
	{
		if (sdhost_sdcard_identification() == 0)
		{
			switch (sdhost_CardType)
			{
			case SDIO_STD_CAPACITY_SD_CARD_V2_0:
			case SDIO_HIGH_CAPACITY_SD_CARD:
				debug_printf_P(PSTR("hardware_sdhost_setspeed to 24 MHz\n"));
				hardware_sdhost_setspeed(24000000uL);
				break;

			default:
				debug_printf_P(PSTR("hardware_sdhost_setspeed to 12 MHz\n"));
				hardware_sdhost_setspeed(12000000uL);
				break;
			}

#if WITHDEBUG
			{
				uint_fast64_t v = sdhost_sdcard_parse_CSD(sdhost_sdcard_CSD);
				debug_printf_P(PSTR("SD Card size = %lu KB (%lu MB) (%08lx:%08lx bytes)\n"), 
					(unsigned long) (v / 1024), 
					(unsigned long) (v / 1024 / 1024), 
					(unsigned long) (v >> 32), 
					(unsigned long) (v >> 0));

			}
#endif /* WITHDEBUG */
			
			return MMC_SUCCESS;
		}
	}
	return MMC_RESPONSE_ERROR;
}

#if ! WITHTEST_H7
// write a size Byte big block beginning at the address.
static 
DRESULT SD_disk_write(
	BYTE drv,			/* Physical drive nmuber (0..) */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address (LBA) */
	UINT count			/* Number of sectors to write */
	)
{
	if (sdhost_sdcard_WriteSectors(buff, sector, count) != 0)
		return RES_ERROR;
	return RES_OK;
}

// read a size Byte big block beginning at the address.
static 
DRESULT SD_disk_read(
	BYTE drv,			/* Physical drive nmuber (0..) */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address (LBA) */
	UINT count		/* Number of sectors to read */
	)
{
	if (sdhost_sdcard_ReadSectors(buff, sector, count) != 0)
		return RES_ERROR;
	return RES_OK;
}


#endif /* ! WITHTEST_H7 */

static 
uint_fast64_t SD_ReadCardSize(void)
{
	return sdhost_sdcard_parse_CSD(sdhost_sdcard_CSD);
}



// for _USE_MKFS
static 
DRESULT SD_Get_Block_Size (
	BYTE drv,		/* Physical drive nmuber (0..) */
	DWORD  *buff	/* Data buffer to store read data */
	)
{
	enum { KB = 1024, MB = KB * KB };
	static const unsigned long aus [16] =
	{
		0,
		16 * KB,	// 1h 16 KB
		32 * KB,
		64 * KB,
		128 * KB,
		256 * KB,
		512 * KB,
		1 * MB,
		2 * MB,
		4 * MB,
		8 * MB,
		12 * MB,
		16 * MB,
		24 * MB,
		32 * MB,
		64 * MB,
	};
	static RAMNOINIT_D1 ALIGNX_BEGIN uint8_t sdhost_sdcard_SDSTATUS [64] ALIGNX_END;

	if (sdhost_read_registers_acmd(SD_CMD_SD_APP_STATUS, sdhost_sdcard_SDSTATUS, 64, 6, sizeof sdhost_sdcard_SDSTATUS) == 0)		// ACMD13
	{
		const unsigned au = array_get_bits(sdhost_sdcard_SDSTATUS, 512, 431, 4);	// [431:428] AU_SIZE
		const unsigned es = array_get_bits(sdhost_sdcard_SDSTATUS, 512, 423, 16);	// [423:408] ERASE_SIZE
		debug_printf_P(PSTR("ioctl: GET_BLOCK_SIZE: AU_SIZE=0x%02x, ERASE_SIZE=0x%04x\n"), au, es);
		* buff = aus [au] * es;
		return RES_OK;
	}
	return RES_ERROR;
}

// for _USE_MKFS
static 
DRESULT SD_Get_Sector_Count (
	BYTE drv,		/* Physical drive nmuber (0..) */
	DWORD  *buff	/* Data buffer to store read data */
	)
{
	* buff = SD_ReadCardSize() / FF_MAX_SS;
	return RES_OK;
}

/* ������ ���������������� ������ �� �������� */
static 
DRESULT SD_Sync(BYTE drv)
{
	if (sdhost_sdcard_waitstatus() != 0)
		return RES_ERROR;
	return RES_OK;
}

static 
DSTATUS SD_Initialize (
	BYTE drv				/* Physical drive nmuber (0..) */
)
{
	debug_printf_P(PSTR("SD_Initialize: drv=%d\n"), (int) drv);
	if (1)
	{
#if WITHSDHCHW
		if (HARDWARE_SDIOSENSE_CD() == 0)
		{
			debug_printf_P(PSTR("SD_Initialize: STA_NODISK\n"));
			return STA_NODISK;
		}
		//if (HARDWARE_SDIOSENSE_WP() != 0)
		//	return STA_PROTECT;
		char ec = sd_initialize2();
		return (ec == MMC_SUCCESS) ? 0 : STA_NODISK;	// STA_NOINIT or STA_NODISK or STA_PROTECT
#endif /* WITHSDHCHW */
	}
	return STA_NODISK;
}


static 
DSTATUS SD_Status (
	BYTE drv		/* Physical drive nmuber (0..) */
)
{
	//debug_printf_P(PSTR("disk_status: drv=%d\n"), (int) drv);
	if (1)
	{
#if WITHSDHCHW
		if (HARDWARE_SDIOSENSE_CD() == 0)
			return STA_NODISK;
		if (HARDWARE_SDIOSENSE_WP() != 0)
			return STA_PROTECT;
		return 0;	// STA_NOINIT or STA_NODISK or STA_PROTECT
#endif /* WITHSDHCHW */
	}
	return STA_NODISK;
}


const struct drvfunc SD_drvfunc =
{
	SD_Initialize,
	SD_Status,
	SD_Sync,
	SD_disk_write,
	SD_disk_read,
	SD_Get_Sector_Count,
	SD_Get_Block_Size,
};

#else /* WITHSDHCHW */


// Tokens (necessary  because at NPO/IDLE (and CS active) only 0xff is on the data/command line)
#define MMC_START_DATA_BLOCK_TOKEN          0xfe   // Data token start byte, Start Single Block Read
#define MMC_START_DATA_MULTIPLE_BLOCK_READ  0xfe   // Data token start byte, Start Multiple Block Read
#define MMC_START_DATA_BLOCK_WRITE          0xfe   // Data token start byte, Start Single Block Write
#define MMC_START_DATA_MULTIPLE_BLOCK_WRITE 0xfc   // Data token start byte, Start Multiple Block Write
#define MMC_STOP_DATA_MULTIPLE_BLOCK_WRITE  0xfd   // Data toke stop byte, Stop Multiple Block Write


// an affirmative R1 response (no errors)
#define MMC_R1_RESPONSE       0x00

// commands: first bit 0 (start bit), second 1 (transmission bit); CMD-number + Offsett 0x40
#define MMC_GO_IDLE_STATE          0x40     //CMD0
#define MMC_SEND_OP_COND           0x41     //CMD1
#define MMC_SEND_IF_COND           0x48     //CMD8 - added by mgs
#define MMC_READ_CSD               0x49     //CMD9
#define MMC_SEND_CID               0x4a     //CMD10
#define MMC_STOP_TRANSMISSION      0x4c     //CMD12
#define MMC_SEND_STATUS            0x4d     //CMD13
#define MMC_SET_BLOCKLEN           0x50     //CMD16 Set block length for next read/write
#define MMC_READ_SINGLE_BLOCK      0x51     //CMD17 Read block from memory
#define MMC_READ_MULTIPLE_BLOCK    0x52     //CMD18
#define MMC_CMD_WRITEBLOCK         0x54     //CMD20 Write block to memory
#define MMC_WRITE_BLOCK            0x58     //CMD24
#define MMC_WRITE_MULTIPLE_BLOCK   0x59     //CMD25
#define MMC_WRITE_CSD              0x5b     //CMD27 PROGRAM_CSD
#define MMC_SET_WRITE_PROT         0x5c     //CMD28
#define MMC_CLR_WRITE_PROT         0x5d     //CMD29
#define MMC_SEND_WRITE_PROT        0x5e     //CMD30
#define MMC_TAG_SECTOR_START       0x60     //CMD32
#define MMC_TAG_SECTOR_END         0x61     //CMD33
#define MMC_UNTAG_SECTOR           0x62     //CMD34
#define MMC_TAG_EREASE_GROUP_START 0x63     //CMD35
#define MMC_TAG_EREASE_GROUP_END   0x64     //CMD36
#define MMC_UNTAG_EREASE_GROUP     0x65     //CMD37
#define MMC_EREASE                 0x66     //CMD38
#define MMC_READ_OCR               0x67     //CMD39
#define MMC_CRC_ON_OFF             0x68     //CMD40

#define MMC_ACMD23	(0x40 + 23)	// ACMD23
#define MMC_ACMD41	(0x40 + 41)	// ACMD41

#define MMC_APP_CMD	(0x40 + 55)		// prefix command - application command - next ACMDxx
#define MMC_GEN_CMD	(0x40 + 56)		// Used either to transfer a Data Block to the card or to get a Data Block from the card for general purpose/application specific commands

/*
	This document describes the SPI mode to control the MMC/SDCs. 
	The SPI mode is an alternative operating mode that defined to use the MMC/SDCs without 
	native host interface. The communication protocol of the SPI mode is a little simple 
	compared to its native operating mode. The MMC/SDC can be attached to the most 
	microcontrollers via a generic SPI interface or GPIO ports. Therefore the SPI mode 
	is suitable for low cost embedded applications with no native host interface is available. 
	There are four different SPI modes, 0 to 3, depends on clock phase and polarity. 
	Mode 0 is defined for SDC. For the MMC, it is not the SPI timing, both latch and shift 
	actions are defined with rising edge of the SCLK, but it seems to work at mode 0 in the SPI mode. 
	Thus the Mode 0 (CPHA=0, CPOL=0) is the proper setting to control MMC/SDC, 
	but mode 3 (CPHA=1, CPOL=1) also works as well in most case.
  */


/* SD card definitions */
static char mmcInit(void);	// ����������, ������������ ��� CS ����������

/* Assert the CS signal, active low (CS=0) */
static void SDCARD_CS_LOW(void)
{
	spi_select2(targetsdcard, SPIC_MODE0, SPIC_SPEEDSDCARD);	// MODE0/MODE3 - ������ ����������� ����
}

/* Deassert the CS signal (CS=1) */
static void SDCARD_CS_HIGH(void)
{
	spi_unselect(targetsdcard);
}
// ���������� �������� SPI ��� ���������� ������ �����������
static void sdcard_spi_setfreq(unsigned long freq)
{
	hardware_spi_master_setfreq(SPIC_SPEEDSDCARD, freq);
}

#define RESPLIMIT 64000U
#define RESPLIMITREAD 640000L

// mmc Get Responce (one byte)
static unsigned char mmcGetResponseR1(void)
{
	unsigned long i;

	unsigned char response;

	for (i = 0; i <= RESPLIMIT; ++ i)
	{
		response = spi_read_byte(targetsdcard, 0xff);
		//if ((response & 0x80) == 0)
		if ((response & 0x80) == 0)
			break;
	}
	return response;
}
// mmc Get Responce (one byte) with busy
static unsigned char mmcGetResponseR1b(void)
{
	unsigned long i;

	unsigned char response;

	for (i = 0; i <= RESPLIMIT; ++ i)
	{
		response = spi_read_byte(targetsdcard, 0xff);
		//debug_printf_P(PSTR("mmcGetResponseR1b 1: %02x\n"), response);
		if ((response & 0x80) == 0)
			break;
	}
	// skip busy
	for (; i <= RESPLIMIT; ++ i)
	{
		unsigned char v = spi_read_byte(targetsdcard, 0xff);
		//debug_printf_P(PSTR("mmcGetResponseR1b 2: %02x\n"), v);
		if (v != 0)
			break;
	}
	return response;
}

static unsigned char waitwhilebusy(void)
{
	unsigned long i;
	// skip busy
	for (i = 0; i <= RESPLIMITREAD; ++ i)
	{
		unsigned char v = spi_read_byte(targetsdcard, 0xff);
		//debug_printf_P(PSTR("mmcGetResponseR1b 2: %02x\n"), v);
		if (v != 0)
			return 0;
	}
	return 1;
}

// mmc Get Responce (five bytes)
static unsigned char mmcGetResponseR3(unsigned long * rp)
{
	//Response comes 1-8bytes after command
	//the first bit will be a 0
	//followed by an error code
	//data will be 0xff until response
	unsigned long i;

	unsigned char response;

	for (i = 0; i <= RESPLIMIT; ++ i)
	{
		response = spi_read_byte(targetsdcard, 0xff);
		if ((response & 0x80) != 0)
			continue;
		{
			//R1print("R3", response);
			unsigned long r = 0;
			r = r * 256 + spi_read_byte(targetsdcard, 0xff);
			r = r * 256 + spi_read_byte(targetsdcard, 0xff);
			r = r * 256 + spi_read_byte(targetsdcard, 0xff);
			r = r * 256 + spi_read_byte(targetsdcard, 0xff);

			* rp = r;
			break;
		}
	}
	return response;
}

// mmc Get Responce (five bytes)
static unsigned char mmcGetResponseR7(unsigned long * rp)
{
	//Response comes 1-8bytes after command
	//the first bit will be a 0
	//followed by an error code
	//data will be 0xff until response
	unsigned long i;

	unsigned char response;

	for (i = 0; i <= RESPLIMIT; ++ i)
	{
		response = spi_read_byte(targetsdcard, 0xff);
		if ((response & 0x80) != 0)
			continue;
		{
			//R1print("R7", response);
			unsigned long r = 0;
			r = r * 256 + spi_read_byte(targetsdcard, 0xff);
			r = r * 256 + spi_read_byte(targetsdcard, 0xff);
			r = r * 256 + spi_read_byte(targetsdcard, 0xff);
			r = r * 256 + spi_read_byte(targetsdcard, 0xff);
			spi_read_byte(targetsdcard, 0xff);	// crc and end bit
			* rp = r;
			break;
		}

	}
	return response;
}


static uint_fast8_t mmcGetXXResponse(uint_fast8_t resp)
{
	//Response comes 1-8bytes after command
	//the first bit will be a 0
	//followed by an error code
	//data will be 0xff until response
	long i = 0;

	uint_fast8_t response;

	while (i <= RESPLIMITREAD)	// � ������� ���� ������ ������ ��������� ����� �������� ������ ������ ������
	{
		response = spi_read_byte(targetsdcard, 0xff);
		if (response == resp)
			break;
		i ++;
	}
	//debug_printf_P(PSTR("waiting count: %d\n"), i);
	return response;
}

/* ��������������� ������������ ������� CRC ��� ������ ����.
   CRC �������� � ������� 7 �����.
*/
static  uint_fast8_t crc7b1(uint_fast8_t crc, uint_fast8_t v1)
{
	if ((v1 != 0) != ((crc & 0x80) != 0))
	{
		crc <<= 1;
		crc ^= (0x09 << 1);
	}
	else
	{
		crc <<= 1;
	}

	return crc;
}

static void mmcSendDummyByte(void)
{
	spi_progval8_p1(targetsdcard, 0xff);	// dummy byte
	spi_complete(targetsdcard);
}

/* ��������������� ������������ ������� CRC ��� ������ ����� */
static uint_fast8_t crc7b8(uint_fast8_t crc, uint_fast8_t v8)
{
	crc = crc7b1(crc, v8 & 0x80);
	crc = crc7b1(crc, v8 & 0x40);
	crc = crc7b1(crc, v8 & 0x20);
	crc = crc7b1(crc, v8 & 0x10);
	crc = crc7b1(crc, v8 & 0x08);
	crc = crc7b1(crc, v8 & 0x04);
	crc = crc7b1(crc, v8 & 0x02);
	crc = crc7b1(crc, v8 & 0x01);

	return crc;
}
// send command to MMC`
// with real CRC value
static void mmcSendCmdCRC7(uint_fast8_t cmd, uint_fast32_t data)
{
	uint_fast8_t crc;

	crc = crc7b8(0x00, cmd);		
	crc = crc7b8(crc, data >> 24);	
	crc = crc7b8(crc, data >> 16);	
	crc = crc7b8(crc, data >> 8);	
	crc = crc7b8(crc, data >> 0);	

	spi_progval8_p1(targetsdcard, cmd);	// command
	spi_progval8_p2(targetsdcard, data >> 24);
	spi_progval8_p2(targetsdcard, data >> 16);
	spi_progval8_p2(targetsdcard, data >> 8);
	spi_progval8_p2(targetsdcard, data >> 0);
	spi_progval8_p2(targetsdcard, crc | 0x01);		// CRC and end transmit bit
	spi_complete(targetsdcard);
}
// send command to MMC
static void mmcSendCmd(uint_fast8_t cmd, uint_fast32_t data)
{
	spi_progval8_p1(targetsdcard, cmd);	// command
	spi_progval8_p2(targetsdcard, data >> 24);
	spi_progval8_p2(targetsdcard, data >> 16);
	spi_progval8_p2(targetsdcard, data >> 8);
	spi_progval8_p2(targetsdcard, data >> 0);
	spi_progval8_p2(targetsdcard, 0xff);	// CRC
	spi_complete(targetsdcard);
}

static uint_fast8_t mmcCardVersion2;
// ��� �� SDHC (��������� ��� � ������)
// ��� SDHC (��������� ��� � 512-�� �������� ������)
static uint_fast32_t mmcAddressMultiplier;

// set MMC in Idle mode
// ����������, ������������ ��� CS ����������
static char mmcGoIdle(void)
{
	unsigned char response;
	unsigned long cmd8answer = 0;

	SDCARD_CS_HIGH();
	/////spi_read_byte(targetsdcard, 0xff);
	SDCARD_CS_LOW();
	mmcSendDummyByte();
	//Send Command 0 to put MMC in SPI mode
	mmcSendCmdCRC7(MMC_GO_IDLE_STATE, 0);	// CMD0 - ����������� � ���������� CRC
	//Now wait for READY RESPONSE
	if ((response = mmcGetResponseR1()) != 0x01)
	{
		return MMC_INIT_ERROR;
	}

	mmcSendDummyByte();
	mmcSendCmdCRC7(MMC_SEND_IF_COND, 0x000001aa);	// CMD8 - ����������� � ���������� CRC. 3.3 Volt VCC
	response = mmcGetResponseR7(& cmd8answer);
	if ((response & 0x04) != 0)	// illegal command or pattern not match
	{
		unsigned long cmd58answer = 0;
		mmcAddressMultiplier = MMC_SECTORSIZE;
		mmcCardVersion2 = 0;

		// if no responce
		// Ver2.00 or later SD Memory Card(voltage mismatch)
		// or Ver1.X SD Memory Card
		// or not SD Memory Card
		debug_printf_P(PSTR("Ver1.X SD Memory Card or not SD Memory Card\n")); 

		// check voltage range here
		mmcSendDummyByte();
		mmcSendCmd(0x40 + 58, 0);	// CMD58
		response = mmcGetResponseR3(& cmd58answer);
		debug_printf_P(PSTR("1: CMD58 has responce %02x, value = %08lx\n"), response, cmd58answer); 
		if (response == 0x00 || response == 0x01)
		{
			if ((cmd58answer & (1UL << 21)) || (cmd58answer & (1UL << 20)))
			{
				debug_printf_P(PSTR("3.3 volt VCC suitable.\n"));
			}
			else
			{
				debug_printf_P(PSTR("3.3 volt VCC NOT suitable.\n"));
				return MMC_INIT_ERROR;
			}
		}

		for (;;)
		{
			mmcSendDummyByte();
			mmcSendCmd(MMC_APP_CMD, 0);	// APP_CMD
			//Now wait for READY RESPONSE
			response = mmcGetResponseR1();
			debug_printf_P(PSTR("mmcGoIdle: 1: APP_CMD responce R1 = %02x\n"), response);
			if (response != 0x01 && response != 0x00)
				continue;

			mmcSendDummyByte();
			mmcSendCmd(MMC_ACMD41, 0);	// ACMD41(0)
			//Now wait for READY RESPONSE
			response = mmcGetResponseR1();
			debug_printf_P(PSTR("ACMD41 responce R1 = %02x\n"), response);
			if (response == 0x00)
				break;
		}
		//debug_printf_P(PSTR("ACMD41 has responce %02x\n"), response); 

		//return MMC_INIT_ERROR;
	}
	else if ((cmd8answer & 0xff) != 0xaa)
	{
		debug_printf_P(PSTR("Bad CMD8 answer. SD Card unuseable\n"));
		return MMC_INIT_ERROR;
	}
	else
	{
		unsigned long cmd58answer = 0;
		mmcCardVersion2 = 1;

		// Ver2.00 or later SD Memory Card
		debug_printf_P(PSTR("Ver2.00 or later SD Memory Card\n")); 	

		// check voltage range here
		mmcSendDummyByte();
		mmcSendCmd(0x40 + 58, 0);	// CMD58
		response = mmcGetResponseR3(& cmd58answer);
		debug_printf_P("2: CMD58 has responce %02x, value = %08lx\n", response, cmd58answer); 
		if (response == 0x00 || response == 0x01)
		{
			if ((cmd58answer & (1UL << 21)) || (cmd58answer & (1UL << 20)))
			{
				debug_printf_P(PSTR("3.3 volt VCC suitable.\n"));
			}
			else
			{
				debug_printf_P(PSTR("3.3 volt VCC NOT suitable.\n"));
				return MMC_INIT_ERROR;
			}
		}

		mmcSendDummyByte();
		mmcSendCmd(0x40 + 59, 1);	// shitch CRC check on - required before ACMD41
		response = mmcGetResponseR1();
		//debug_printf_P(PSTR("CMD59 has responce %02x\n"), response); 

		for (;;)
		{
			mmcSendDummyByte();
			mmcSendCmdCRC7(MMC_APP_CMD, 0);	// APP_CMD
			//Now wait for READY RESPONSE
			response = mmcGetResponseR1();
			//debug_printf_P(PSTR("mmcGoIdle: 2: APP_CMD responce R1 = %02x\n"), response);
			if (response != 0x01 && response != 0x00)
				continue;

			mmcSendDummyByte();
			mmcSendCmdCRC7(MMC_ACMD41, 0x40000000);	// HCS (High Capacity Support)
			//Now wait for READY RESPONSE
			response = mmcGetResponseR1();
			//debug_printf_P(PSTR("ACMD41 responce R1 = %02x\n"), response);
			if (response == 0x00)
				break;
		}

		mmcSendDummyByte();
		mmcSendCmdCRC7(0x40 + 59, 0);	// shitch CRC check off
		response = mmcGetResponseR1();
		//debug_printf_P("CMD59 has responce %02x\n", response); 

		// Check CCS bit (capacity) bit
		mmcSendDummyByte();
		mmcSendCmd(0x40 + 58, 0);	// CMD58
		response = mmcGetResponseR3(& cmd58answer);
		debug_printf_P(PSTR("3: CMD58 has responce %02x, value = %08lx\n"), response, cmd58answer); 
		if (response == 0x00 || response == 0x01)
		{
			if ((cmd58answer & 0x40000000) != 0)	//CCS (Card Capacity Status)
			{
				debug_printf_P(PSTR("SDHC or SDXC (High Capacity)\n"));
				mmcAddressMultiplier = 1;	// ��� SDHC (��������� ��� � 512-�� �������� ������)
			}
			else
			{
				debug_printf_P(PSTR("SDSD - up to 2GB\n"));
				mmcAddressMultiplier = MMC_SECTORSIZE;	// ��� ������� SD ����
			}
		}
	}

	//debug_printf_P("mmcGoIdle() done.\n");

	spi_read_byte(targetsdcard, 0xff);
	return (MMC_SUCCESS);
}


uint_fast64_t MMC_ReadCardSize(void)
{
	// Read contents of Card Specific Data (CSD)

	uint_fast64_t MMC_CardSize = 0;




	SDCARD_CS_LOW();

	// Asks the selected card to send its card-specific data (CSD)
	mmcSendDummyByte();
	mmcSendCmd(MMC_READ_CSD, 0x00000000);   // CMD 9

	if (mmcGetResponseR1() == 0x00)
	{
		// now look for the data token to signify the start of
		// the data
		if (mmcGetXXResponse(MMC_START_DATA_BLOCK_TOKEN) == MMC_START_DATA_BLOCK_TOKEN)
		{
			static RAMNOINIT_D1 ALIGNX_BEGIN uint8_t bv [16] ALIGNX_END;
			// ����� ���� ����������� ������ DMA � ���������� ������ ������
			// �������� � STM32F4x ��� ������� � CCM ������ �� DMA

			prog_spi_read_frame(targetsdcard, bv, 16);	// ����������� ���������� ������ �������� � ����� �������, ������ 0xFF �� MOSI
			
			spi_read_byte(targetsdcard, 0xff);  // least Ncr (max = 4 bytes) cycles after
			spi_read_byte(targetsdcard, 0xff);  // least Ncr (max = 4 bytes) cycles after
			spi_read_byte(targetsdcard, 0xff);  // least Ncr (max = 4 bytes) cycles after
			spi_read_byte(targetsdcard, 0xff);  // least Ncr (max = 4 bytes) cycles after
			// the card response is received
			spi_read_byte(targetsdcard, 0xff);
			MMC_CardSize = sdhost_sdcard_parse_CSD(bv);	// ��������� ���������������� ����� (Code length is 128 bits = 16 bytes)

		}
	}
	else
	{
		mmcInit();
	}
	SDCARD_CS_HIGH();

	return (MMC_CardSize);

}

// ����������, ������������ ��� CS ����������
static char mmcInit(void)
{
	char ec;
	int i;

	debug_printf_P(PSTR("SPI clock set to 400 kHz\n"));

	SDCARD_CS_HIGH();
	sdcard_spi_setfreq(400000);
	SDCARD_CS_LOW();

	for(i = 0; i <= 9; i ++)
		spi_read_byte(targetsdcard, 0xff);

	ec = mmcGoIdle();	// ����������, ������������ ��� CS ����������

	if (ec == 0)
	{
		if (mmcCardVersion2 != 0)
		{
			debug_printf_P("SPI clock set to 24 MHz\n");
			SDCARD_CS_HIGH();
			sdcard_spi_setfreq(24000000);
			SDCARD_CS_LOW();
		}
		else
		{
			debug_printf_P("SPI clock set to 12 MHz\n");
			SDCARD_CS_HIGH();
			sdcard_spi_setfreq(12000000);
			SDCARD_CS_LOW();
		}
	}
	else
	{
			debug_printf_P("mmcGoIdle return ec=%d\n", ec);
	}
	
	return ec;
}

//--------------- set blocklength 2^n ------------------------------------------------------
static char mmcSetBlockLength (unsigned long blocklength)
{
	char ec = MMC_SUCCESS;
	// CS = LOW (on)
	SDCARD_CS_LOW();
	// Set the block length to read
	mmcSendDummyByte();
	mmcSendCmd(MMC_SET_BLOCKLEN, blocklength);

	// get response from MMC - make sure that its 0x00 (R1 ok response format)
	if (mmcGetResponseR1() != 0x00)
	{ 
		ec = mmcInit();	// ����������, ������������ ��� CS ����������

		if (ec == 0)
		{
		  // Set the block length to read
			mmcSendDummyByte();
			mmcSendCmd(MMC_SET_BLOCKLEN, blocklength);
			ec = mmcGetResponseR1() == 0x00 ? MMC_SUCCESS : MMC_RESPONSE_ERROR;
		}
	}
	else
		ec = MMC_BLOCK_SET_ERROR;
	// ���������� ���� - ����� ����������� CS
	// Send 8 Clock pulses of delay.
	spi_read_byte(targetsdcard, 0xff);

	SDCARD_CS_HIGH();


	return ec;
} // Set block_length


/*
	0 = Idle
	1 = Ready
	2 = Ident
	3 = Stby
	4 = Transfer
	5 = Data
	6 = Rcv
	7 = Programming
	8 = Disconnected
	9 = Btst
	10-15 = reserved
*/

// Check if MMC card is still busy
static char mmcCheckBusy(void)
{
  //Response comes 1-8bytes after command
  //the first bit will be a 0
  //followed by an error code
  //data will be 0xff until response
  int i = 0;

  char response;
  char rvalue;
  while (i <= 64)
  {
    response = spi_read_byte(targetsdcard, 0xff);
    response &= 0x1f;
    switch (response)
    {
      case 0x05: rvalue = MMC_SUCCESS; break;
      case 0x0b: return MMC_CRC_ERROR;
      case 0x0d: return MMC_WRITE_ERROR;
      default:
        rvalue = MMC_OTHER_ERROR;
        break;
    }
    if (rvalue == MMC_SUCCESS)
		break;
    i ++;
  }
  i = 0;
  do
  {
    response = spi_read_byte(targetsdcard, 0xff);
    i ++;
  } while (response == 0);
  return rvalue;
}



// The card will respond with a standard response token followed by a data
// block suffixed with a 16 bit CRC.

// read a size Byte big block beginning at the address.
static char mmcReadSectors(
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address (LBA) */
	UINT count		/* Number of sectors to read */
	)
{
	char rvalue = MMC_RESPONSE_ERROR;

	if (count == 1)
	{
		// single block read
		// CS = LOW (on)
		SDCARD_CS_LOW();

		mmcSendDummyByte();
		if (waitwhilebusy() != 0)	// 1) �������� � ������ ���� ������� ������/������/sync
		{
			SDCARD_CS_HIGH();
			return MMC_RESPONSE_ERROR;
		}

		// send read command MMC_READ_SINGLE_BLOCK=CMD17
		mmcSendDummyByte();
		mmcSendCmd(MMC_READ_SINGLE_BLOCK, sector * mmcAddressMultiplier);
		//mmcCheckBusy();	// INSERTED

		// Send 8 Clock pulses of delay, check if the MMC acknowledged the read block command
		// it will do this by sending an affirmative response
		// in the R1 format (0x00 is no errors)
		if (mmcGetResponseR1() == 0x00)
		{
			// now look for the data token to signify the start of
			// the data
			if (mmcGetXXResponse(MMC_START_DATA_BLOCK_TOKEN) == MMC_START_DATA_BLOCK_TOKEN)
			{
				// clock the actual data transfer and receive the bytes; mmcGetXXResponse automatically finds the Data Block
				spi_read_frame(targetsdcard, buff, MMC_SECTORSIZE);
				// get CRC bytes (not really needed by us, but required by MMC)
				spi_progval8_p1(targetsdcard, 0xff);
				spi_progval8_p2(targetsdcard, 0xff);
				spi_complete(targetsdcard);
				rvalue = MMC_SUCCESS;
			}
			else
			{
				// the data token was never received
				//debug_printf_P("MMC_DATA_TOKEN_ERROR\n");
				rvalue = MMC_DATA_TOKEN_ERROR;      // 3
			}
		}
		else
		{
			// the MMC never acknowledge the read command
			//debug_printf_P("MMC_RESPONSE_ERROR\n");
			rvalue = MMC_RESPONSE_ERROR;          // 2
		}

		SDCARD_CS_HIGH();
		////////spi_read_byte(targetsdcard, 0xff);
		return rvalue;
	}
	else
	{
		// multi block read
		// CS = LOW (on)
		SDCARD_CS_LOW();

		mmcSendDummyByte();
		if (waitwhilebusy() != 0)	// 1) �������� � ������ ���� ������� ������/������/sync
		{
			SDCARD_CS_HIGH();
			return MMC_RESPONSE_ERROR;
		}


		// send read command MMC_READ_SINGLE_BLOCK=CMD17
		mmcSendDummyByte();
		mmcSendCmd(MMC_READ_MULTIPLE_BLOCK, sector * mmcAddressMultiplier);
		//mmcCheckBusy();	// INSERTED

		// Send 8 Clock pulses of delay, check if the MMC acknowledged the read block command
		// it will do this by sending an affirmative response
		// in the R1 format (0x00 is no errors)
		if (mmcGetResponseR1() == 0x00)
		{
			do
			{
				// now look for the data token to signify the start of
				// the data
				if (mmcGetXXResponse(MMC_START_DATA_BLOCK_TOKEN) == MMC_START_DATA_BLOCK_TOKEN)
				{
					// clock the actual data transfer and receive the bytes; mmcGetXXResponse automatically finds the Data Block
					spi_read_frame(targetsdcard, buff, MMC_SECTORSIZE);
					// get CRC bytes (not really needed by us, but required by MMC)
					spi_progval8_p1(targetsdcard, 0xff);
					spi_progval8_p2(targetsdcard, 0xff);
					spi_complete(targetsdcard);
					rvalue = MMC_SUCCESS;

					buff += MMC_SECTORSIZE;
				}
				else
				{
					// the data token was never received
					debug_printf_P("mmcReadSectors: MMC_DATA_TOKEN_ERROR\n");
					rvalue = MMC_DATA_TOKEN_ERROR;      // 3
					break;
				}
			} while (-- count);

			mmcSendCmd(MMC_STOP_TRANSMISSION, 0);
			mmcSendDummyByte();
			if (mmcGetResponseR1b() != 0x00)
			{
				debug_printf_P("mmcReadSectors: MMC_STOP_TRANSMISSION error\n");
				rvalue = MMC_RESPONSE_ERROR;    
			}
			mmcCheckBusy();
		}
		else
		{
			// the MMC never acknowledge the read command
			debug_printf_P("SD_ReadSectors: MMC_READ_MULTIPLE_BLOCK: MMC_RESPONSE_ERROR\n");
			rvalue = MMC_RESPONSE_ERROR;          // 2
		}

		SDCARD_CS_HIGH();
		////////spi_read_byte(targetsdcard, 0xff);
		return rvalue;
	}
}// mmc_read_block

// write a size Byte big block beginning at the address.
static char mmcWriteSectors(
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address (LBA) */
	UINT count			/* Number of sectors to write */
	)
{
	char rvalue = MMC_RESPONSE_ERROR;         // MMC_SUCCESS;
	//  char c = 0x00;

	if (count == 1)
	{
		// signle block write
		// Set the block length to read
		//if (mmcSetBlockLength (count) == MMC_SUCCESS)   // block length could be set
		// CS = LOW (on)
		SDCARD_CS_LOW();
		// send write command
		mmcSendDummyByte();
		if (waitwhilebusy() != 0)	// 1) �������� � ������ ���� ������� ������/������/sync
		{
			SDCARD_CS_HIGH();
			return MMC_RESPONSE_ERROR;
		}

		mmcSendDummyByte();
		mmcSendCmd(MMC_WRITE_BLOCK, sector * mmcAddressMultiplier);

		// check if the MMC acknowledged the write block command
		// it will do this by sending an affirmative response
		// in the R1 format (0x00 is no errors)
		if (mmcGetXXResponse(MMC_R1_RESPONSE) == MMC_R1_RESPONSE)
		{
			spi_progval8_p1(targetsdcard, 0xff);
			// send the data token to signify the start of the data
			spi_progval8_p2(targetsdcard, MMC_START_DATA_BLOCK_WRITE);
			spi_complete(targetsdcard);

			// clock the actual data transfer and transmitt the bytes
			arm_hardware_flush((uintptr_t) buff, MMC_SECTORSIZE);
			spi_send_frame(targetsdcard, buff, MMC_SECTORSIZE);

			// put CRC bytes (not really needed by us, but required by MMC)
			spi_progval8_p1(targetsdcard, 0xff);
			spi_progval8_p2(targetsdcard, 0xff);
			spi_complete(targetsdcard);
			// read the data response xxx0<status>1 : status 010: Data accected, status 101: Data
			//   rejected due to a crc error, status 110: Data rejected due to a Write error.
			mmcCheckBusy();
			rvalue = MMC_SUCCESS;
		}
		else
		{
			// the MMC never acknowledge the write command
			//debug_printf_P("MMC_BLOCK_SET_ERROR\n");
			rvalue = MMC_BLOCK_SET_ERROR;   // 2
		}
		// give the MMC the required clocks to finish up what ever it needs to do
		//  for (i = 0; i < 9; ++i)
		//    spi_progval8(targetsdcard, 0xff);

		SDCARD_CS_HIGH();
		// Send 8 Clock pulses of delay.
		////////spi_read_byte(targetsdcard, 0xff);
		return rvalue;
	}
	else
	{
		// write multiblock
		//debug_printf_P(PSTR("write multiblock, count=%d\n"), count);
		// Set the block length to read
		//if (mmcSetBlockLength (count) == MMC_SUCCESS)   // block length could be set
		// CS = LOW (on)
		SDCARD_CS_LOW();
		mmcSendDummyByte();
		if (waitwhilebusy() != 0)	// 1) �������� � ������ ���� ������� ������/������/sync
		{
			SDCARD_CS_HIGH();
			return MMC_RESPONSE_ERROR;
		}

		{
			unsigned char response;
			mmcSendDummyByte();
			mmcSendCmd(MMC_APP_CMD, 0);	// APP_CMD
			//Now wait for READY RESPONSE
			response = mmcGetResponseR1();
			if (/*response != 0x01 && */response != 0x00)
			{
				debug_printf_P(PSTR("mmcWriteSectors: 1: APP_CMD responce R1 = %02x\n"), response);
				SDCARD_CS_HIGH();
				return MMC_BLOCK_SET_ERROR;
			}

			mmcSendDummyByte();
			mmcSendCmd(MMC_ACMD23, count & 0x7FFFFF);	// ACMD23(number of blocks for erase) - 23 bits
			//Now wait for READY RESPONSE
			response = mmcGetResponseR1();
			if (/*response != 0x01 && */response != 0x00)
			{
				debug_printf_P(PSTR("mmcWriteSectors: 1: MMC_ACMD23 responce R1 = %02x\n"), response);
				SDCARD_CS_HIGH();
				return MMC_BLOCK_SET_ERROR;
			}
		}

		// send write command
		mmcSendDummyByte();
		mmcSendCmd(MMC_WRITE_MULTIPLE_BLOCK, sector * mmcAddressMultiplier);

		// check if the MMC acknowledged the write block command
		// it will do this by sending an affirmative response
		// in the R1 format (0x00 is no errors)
		if (mmcGetXXResponse(MMC_R1_RESPONSE) == MMC_R1_RESPONSE)
		{
			do
			{
				spi_progval8_p1(targetsdcard, 0xff);
				// send the data token to signify the start of the data
				spi_progval8_p2(targetsdcard, MMC_START_DATA_MULTIPLE_BLOCK_WRITE);
				spi_complete(targetsdcard);


				// clock the actual data transfer and transmitt the bytes
				arm_hardware_flush((uintptr_t) buff, MMC_SECTORSIZE);
				spi_send_frame(targetsdcard, buff, MMC_SECTORSIZE);

				// put CRC bytes (not really needed by us, but required by MMC)
				spi_progval8_p1(targetsdcard, 0xff);
				spi_progval8_p2(targetsdcard, 0xff);
				spi_complete(targetsdcard);
				//unsigned char rr = spi_read_byte(targetsdcard, 0xff);
				//debug_printf_P(PSTR("mmcWriteSectors: 1: rr=%02x\n"), rr);
				//if ((rr & 0x1f) != 0x05)
				//{
				//	break;
				//}
				if (mmcCheckBusy() != MMC_SUCCESS)
					break;


				buff += MMC_SECTORSIZE;
			} while (-- count);
			// stop thh train
			//spi_progval8_p1(targetsdcard, 0xff);
			// send the data token to signify the start of the data
			spi_progval8_p1(targetsdcard, MMC_STOP_DATA_MULTIPLE_BLOCK_WRITE);
			spi_complete(targetsdcard);

			// read the data response xxx0<status>1 : status 010: Data accected, status 101: Data
			//   rejected due to a crc error, status 110: Data rejected due to a Write error.
			//waitwhilebusy();	// 1) ������ ������ � �������� � ������ ���� ������� ������/������/sync

			rvalue = count == 0 ? MMC_SUCCESS : MMC_WRITE_ERROR;
		}
		else
		{
			// the MMC never acknowledge the write command
			//debug_printf_P("MMC_BLOCK_SET_ERROR\n");
			rvalue = MMC_BLOCK_SET_ERROR;   // 2
		}
		// give the MMC the required clocks to finish up what ever it needs to do
		//  for (i = 0; i < 9; ++i)
		//    spi_progval8(targetsdcard, 0xff);

		SDCARD_CS_HIGH();
		// Send 8 Clock pulses of delay.
		////////spi_read_byte(targetsdcard, 0xff);
		//debug_printf_P(PSTR("write multiblock, count=%d done\n"), count);
		return rvalue;
	}
} // mmc_write_block


// for _USE_MKFS
static 
DRESULT MMC_Get_Sector_Count (
	BYTE drv,		/* Physical drive nmuber (0..) */
	DWORD  *buff	/* Data buffer to store read data */
	)
{
	* buff = MMC_ReadCardSize() / FF_MAX_SS;
	return RES_OK;
}

// for _USE_MKFS
static 
DRESULT MMC_Get_Block_Size (
	BYTE drv,		/* Physical drive nmuber (0..) */
	DWORD  *buff	/* Data buffer to store read data */
	)
{
	return RES_ERROR;
}

static 
char mmc_detectcard(void)
{
	sd_power_cycle();

	char ec;
	SDCARD_CS_LOW();
	ec = mmcInit();
	SDCARD_CS_HIGH();


	if (ec != MMC_SUCCESS)
		return ec;

	ec = mmcSetBlockLength(MMC_SECTORSIZE);
	return ec;
}


static 
DRESULT MMC_disk_read (
	BYTE drv,		/* Physical drive nmuber (0..) */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address (LBA) */
	UINT count		/* Number of sectors to read */
)
{
	if (mmcReadSectors(buff, sector, count) != MMC_SUCCESS)
		return RES_ERROR;
	return RES_OK;
}

static 
DRESULT MMC_disk_write (
	BYTE drv,			/* Physical drive nmuber (0..) */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address (LBA) */
	UINT count			/* Number of sectors to write */
)
{
	if (mmcWriteSectors(buff, sector, count) != MMC_SUCCESS)
		return RES_ERROR;
	return RES_OK;
}

/* ������ ���������������� ������ �� �������� */
static 
DRESULT MMC_Sync(BYTE drv)
{
	SDCARD_CS_LOW();
	mmcSendDummyByte();
	if (waitwhilebusy() != 0)	// 1) �������� � ������ ���� ������� ������/������/sync
	{
		SDCARD_CS_HIGH();
		return RES_ERROR;
	}
	SDCARD_CS_HIGH();
	return RES_OK;
}

static 
DSTATUS MMC_Initialize (
	BYTE drv				/* Physical drive nmuber (0..) */
)
{
	//debug_printf_P(PSTR("MMC_Initialize: drv=%d\n"), (int) drv);
	if (1)
	{
		if (HARDWARE_SDIOSENSE_CD() == 0)
			return STA_NODISK;
		//if (HARDWARE_SDIOSENSE_WP() != 0)
		//	return STA_PROTECT;
		char ec = mmc_detectcard();
		return (ec == MMC_SUCCESS) ? 0 : STA_NODISK;	// STA_NOINIT or STA_NODISK or STA_PROTECT
	}
	return STA_NODISK;
}

static 
DSTATUS MMC_Status (
	BYTE drv		/* Physical drive nmuber (0..) */
)
{
	//debug_printf_P(PSTR("disk_status: drv=%d\n"), (int) drv);
	if (1)
	{
		if (HARDWARE_SDIOSENSE_CD() == 0)
			return STA_NODISK;
		if (HARDWARE_SDIOSENSE_WP() != 0)
			return STA_PROTECT;
		return 0;	// STA_NOINIT or STA_NODISK or STA_PROTECT
	}
	return STA_NODISK;
}


const struct drvfunc MMC_drvfunc =
{
	MMC_Initialize,
	MMC_Status,
	MMC_Sync,
	MMC_disk_write,
	MMC_disk_read,
	MMC_Get_Sector_Count,
	MMC_Get_Block_Size,
};

#endif /* WITHSDHCHW */

#endif /* WITHUSESDCARD */
