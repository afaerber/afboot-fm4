#include <stdlib.h>
#include <stdint.h>

#define PERIPH_BASE	0x40000000

#define FLASH_BASE	0x40000000

#define FLASH_FBFCR_BE	(1UL << 0)

#define FLASH_CRTRMM_TRMM_MASK	(0x3ff << 0)
#define FLASH_CRTRMM_TTRMM_MASK	(0x1f << 16)

#define CLOCK_BASE	0x40010000

#define CLOCK_SCM_CTL_MOSCE		(1UL << 1)
#define CLOCK_SCM_CTL_PLLE		(1UL << 4)
#define CLOCK_SCM_CTL_RCS_MAIN_PLL	(0x2UL << 5)

#define CLOCK_SCM_STR_MORDY	(1UL << 1)
#define CLOCK_SCM_STR_PLRDY	(1UL << 4)
#define CLOCK_SCM_STR_RCM_MAIN_PLL	(0x2UL << 5)
#define CLOCK_SCM_STR_RCM_MASK		(0x7UL << 5)

#define WDG_BASE	0x40011000

#define CR_TRIM_BASE	0x4002E000

#define GPIO_BASE	0x4006F000
#define GPIO_PFRx	(GPIO_BASE + 0x000)
#define GPIO_PCRx	(GPIO_BASE + 0x100)
#define GPIO_DDRx	(GPIO_BASE + 0x200)
#define GPIO_PDIRx	(GPIO_BASE + 0x300)
#define GPIO_PDORx	(GPIO_BASE + 0x400)
#define GPIO_EPFRx	(GPIO_BASE + 0x600)

#define PERIPH_BITBAND(_addr) (0x42000000 + (_addr - PERIPH_BASE) * 32)

static void watchdog_disable(void)
{
	volatile uint32_t *WDG_CTL = (void *)(WDG_BASE + 0x008);
	volatile uint32_t *WDG_LCK = (void *)(WDG_BASE + 0xC00);

	*WDG_LCK = 0x1ACCE551;
	*WDG_LCK = 0xE5331AAE;
	*WDG_CTL = 0;
}

static void trace_buffer_enable(void)
{
	volatile uint32_t *FBFCR = (void *)(FLASH_BASE + 0x014);

	*FBFCR = FLASH_FBFCR_BE;
}

unsigned long masterclk;

static void clock_setup(void)
{
	volatile uint32_t *SCM_CTL   = (void *)(CLOCK_BASE + 0x000);
	volatile uint32_t *SCM_STR   = (void *)(CLOCK_BASE + 0x004);
	volatile uint32_t *BSC_PSR   = (void *)(CLOCK_BASE + 0x010);
	volatile uint32_t *APBC0_PSR = (void *)(CLOCK_BASE + 0x014);
	volatile uint32_t *APBC1_PSR = (void *)(CLOCK_BASE + 0x018);
	volatile uint32_t *APBC2_PSR = (void *)(CLOCK_BASE + 0x01C);
	volatile uint32_t *SWC_PSR   = (void *)(CLOCK_BASE + 0x020);
	volatile uint32_t *TTC_PSR   = (void *)(CLOCK_BASE + 0x028);
	volatile uint32_t *CSW_TMR   = (void *)(CLOCK_BASE + 0x030);
	volatile uint32_t *PSW_TMR   = (void *)(CLOCK_BASE + 0x034);
	volatile uint32_t *PLL_CTL1  = (void *)(CLOCK_BASE + 0x038);
	volatile uint32_t *PLL_CTL2  = (void *)(CLOCK_BASE + 0x03C);
	const unsigned long clkmo = 4000000UL;

	*BSC_PSR = 0;
	*APBC0_PSR = 1;
	*APBC1_PSR = 0x80;
	*APBC2_PSR = 0x81;
	*SWC_PSR = (1 << 7) | (3 << 0);
	*TTC_PSR = 0;

	*CSW_TMR = (0x5 << 4) | (0xC << 0);

	*SCM_CTL |= CLOCK_SCM_CTL_MOSCE;
	while (!(*SCM_STR & CLOCK_SCM_STR_MORDY)) {}

	*PSW_TMR = 0;
	*PLL_CTL1 = 1;
#ifdef S6E2CC
	*PLL_CTL2 = 0x31;
#elif defined(MB9BF)
	*PLL_CTL2 = 0x27;
#endif

	*SCM_CTL |= CLOCK_SCM_CTL_PLLE;
	while (!(*SCM_STR & CLOCK_SCM_STR_PLRDY)) {}

	*SCM_CTL |= CLOCK_SCM_CTL_RCS_MAIN_PLL;
	while ((*SCM_STR & CLOCK_SCM_STR_RCM_MASK) != CLOCK_SCM_STR_RCM_MAIN_PLL) {}

	masterclk = clkmo * ((*PLL_CTL2 & 0x3f) + 1);
	masterclk /= (((*PLL_CTL1 >> 4) & 0xf) + 1);
	// BSC_PSR == 0
}

static void trimming_setup(void)
{
	volatile uint32_t *FLASH_CRTRMM = (void *)(FLASH_BASE + 0x100);
	volatile uint32_t *MCR_FTRM = (void *)(CR_TRIM_BASE + 0x004);
	volatile uint32_t *MCR_TTRM = (void *)(CR_TRIM_BASE + 0x008);
	volatile uint32_t *MCR_RLR  = (void *)(CR_TRIM_BASE + 0x00C);

	if ((*FLASH_CRTRMM & FLASH_CRTRMM_TRMM_MASK) != FLASH_CRTRMM_TRMM_MASK) {
		*MCR_RLR = 0x1ACCE554;
		*MCR_TTRM = (*FLASH_CRTRMM & FLASH_CRTRMM_TTRMM_MASK) >> 16;
		*MCR_FTRM = *FLASH_CRTRMM & FLASH_CRTRMM_TRMM_MASK;
		*MCR_RLR = 0x00000000;
	}
}

#define GPIO_PFR(_bank, _pin)	(PERIPH_BITBAND(GPIO_PFRx)  + _bank * 32 * 4 + _pin * 4)
#define GPIO_DDR(_bank, _pin)	(PERIPH_BITBAND(GPIO_DDRx)  + _bank * 32 * 4 + _pin * 4)
#define GPIO_PDOR(_bank, _pin)	(PERIPH_BITBAND(GPIO_PDORx) + _bank * 32 * 4 + _pin * 4)
#define GPIO_ADE(_func) (PERIPH_BITBAND(GPIO_BASE + 0x500) + _func * 4)

static inline void gpio_set_pfr(int bank, int pin, uint8_t value)
{
	volatile uint8_t *reg  = (void *)GPIO_PFR(bank, pin);
	*reg = value;
}

static inline void gpio_set_ddr(int bank, int pin, uint8_t value)
{
	volatile uint8_t *reg  = (void *)GPIO_DDR(bank, pin);
	*reg = value;
}

static inline void gpio_set_pdor(int bank, int pin, uint8_t value)
{
	volatile uint8_t *reg  = (void *)GPIO_PDOR(bank, pin);
	*reg = value;
}

static inline uint8_t gpio_get_pdor(int bank, int pin)
{
	volatile uint8_t *reg  = (void *)GPIO_PDOR(bank, pin);
	return *reg;
}

static inline void gpio_set_ade(int func, uint8_t value)
{
	volatile uint8_t *reg  = (void *)GPIO_ADE(func);
	*reg = value;
}

#define MFS_BASE(_nr) (0x40038000 + _nr * 0x100)

static void uart_setup(void)
{
	volatile uint32_t *GPIO_EPFR07 = (void *)(GPIO_EPFRx + 7 * 4);
	volatile uint8_t *SMR  = (void *)(MFS_BASE(0) + 0x00);
	volatile uint8_t *SCR  = (void *)(MFS_BASE(0) + 0x01);
	volatile uint8_t *SCR_UPCL = (void *)(PERIPH_BITBAND(MFS_BASE(0) + 0x00) + 15 * 4);
	volatile uint8_t *SCR_RXE  = (void *)(PERIPH_BITBAND(MFS_BASE(0) + 0x00) +  9 * 4);
	volatile uint8_t *SCR_TXE  = (void *)(PERIPH_BITBAND(MFS_BASE(0) + 0x00) +  8 * 4);
	volatile uint8_t *ESCR = (void *)(MFS_BASE(0) + 0x04);
	volatile uint8_t *SSR  = (void *)(MFS_BASE(0) + 0x05);
	volatile uint8_t *SSR_REC  = (void *)(PERIPH_BITBAND(MFS_BASE(0) + 0x04) + 15 * 4);
	volatile uint16_t *BGR = (void *)(MFS_BASE(0) + 0x0C);
	volatile uint8_t *FCR0 = (void *)(MFS_BASE(0) + 0x14);
	volatile uint8_t *FCR1 = (void *)(MFS_BASE(0) + 0x15);
	uint32_t val32;

#ifdef MB9BF
	gpio_set_ade(17, 0);
#endif
	gpio_set_pfr(0x2, 0x1, 1);

#ifdef S6E2CC
	gpio_set_ade(31, 0);
#elif defined(MB9BF)
	gpio_set_ade(16, 0);
#endif
	gpio_set_pfr(0x2, 0x2, 1);

	val32 = *GPIO_EPFR07;
	val32 &= ~((3 << 6) | (3 << 4));
	val32 |= (1 << 6) | (0 << 4);
	*GPIO_EPFR07 = val32;

	// deinit
	*SCR_TXE = 0;
	*SCR_RXE = 0;
	*BGR = 0;
	*SMR = 0;
	*SCR = 0;
	*SSR_REC = 1;
	*SSR = 0;
	*ESCR = 0;
	*FCR0 = 0;
	*FCR1 = 0;
	*SCR_UPCL = 1;

	*SMR = 0;
	*SCR = 0;
	*SCR_UPCL = 1;

	*SMR = (0 << 5) | (0 << 2); // mode 0, lsb
	*SMR = (0 << 5) | (0 << 3) | (0 << 2) | (1 << 0); // 1 stop bit, output enable
	*SCR = 0;
	*ESCR = 0;
	*SSR_REC = 1;
	val32 = masterclk / 2;
	val32 /= 115200;
	val32 -= 1;
	*BGR = val32 & 0x7fff;
#ifdef S6E2CC
	//*BGR = 867 & 0x7fff;
#elif defined(MB9BF)
	//*BGR = (80000000UL / 115200UL - 1) & 0x7fff;
#endif
	*SCR_TXE = 1;
	//*SCR_RXE = 1;
}

void uart_putch(char ch);

void uart_putch(char ch)
{
	volatile uint8_t *SSR_TDRE = (void *)(PERIPH_BITBAND(MFS_BASE(0) + 0x04) +  9 * 4);
	volatile uint16_t *TDR = (void *)(MFS_BASE(0) + 0x08);

	while (*SSR_TDRE == 0) {}
	*TDR = (uint8_t)ch;
}

static inline void gpio_set_epfr(int regnr, int bit, uint8_t val)
{
	volatile uint8_t *reg = (void *)(PERIPH_BITBAND(GPIO_EPFRx + regnr * 4) + bit * 4);
	*reg = val;
}

#define EXTIF_BASE	0x4003F000
#define EXTIF_MODEx	(EXTIF_BASE + 0x000)
#define EXTIF_TIMx	(EXTIF_BASE + 0x020)
#define EXTIF_AREAx	(EXTIF_BASE + 0x040)
#define EXTIF_ATIMx	(EXTIF_BASE + 0x060)

static void ebif_setup(void)
{
	volatile uint8_t *CGT_CKEN0_EXBCK  = (void *)(PERIPH_BITBAND(0x4003C100 + 0x0) + 26 * 4);
	volatile uint8_t *CGT_MRST0_EXBRST = (void *)(PERIPH_BITBAND(0x4003C100 + 0x4) + 26 * 4);
#ifdef S6E2CC
	volatile uint32_t *MODE0 = (void *)(EXTIF_MODEx + 0 * 4);
	volatile uint32_t *TIM0 = (void *)(EXTIF_TIMx + 0 * 4);
	volatile uint32_t *AREA0 = (void *)(EXTIF_AREAx + 0 * 4);
	volatile uint32_t *ATIM0 = (void *)(EXTIF_ATIMx + 0 * 4);
#endif
#ifdef MB9BF
	volatile uint32_t *SDMODE = (void *)(EXTIF_BASE + 0x100);
	volatile uint32_t *REFTIM = (void *)(EXTIF_BASE + 0x104);
	volatile uint32_t *PWRDWN = (void *)(EXTIF_BASE + 0x108);
	volatile uint32_t *SDTIM = (void *)(EXTIF_BASE + 0x10c);
#endif
	volatile uint32_t *DCLKR = (void *)(EXTIF_BASE + 0x300);
#ifdef S6E2CC
	volatile uint32_t *AMODE = (void *)(EXTIF_BASE + 0x310);
#endif
#ifdef MB9BF
	uint32_t value;
#endif
	int i;

#ifdef S6E2CC
	// MAD10..MAD11
	gpio_set_ade(14, 0);
	gpio_set_ade(15, 0);
	// MAD12..MAD15, MAD17..MAD18
	for (i = 24; i <= 29; i++) {
		gpio_set_ade(i, 0);
	}
#elif defined(MB9BF)
	// MAD07..MAD11
	for (i = 0; i <= 4; i++) {
		gpio_set_ade(i, 0);
	}
	// MAD14..MAD15
	for (i = 7; i <= 8; i++) {
		gpio_set_ade(i, 0);
	}
#endif

#ifdef S6E2CC
	// MWEX, MDQM1 and MDQM0, MOEX
	for (i = 3; i <= 5; i++) {
		gpio_set_epfr(10, i, 1);
	}
	// MAD08..MAD20
	for (i = 15; i <= 27; i++) {
		gpio_set_epfr(10, i, 1);
	}
	// MCSX0, MAD01..MAD07, MADATA00..MADATA15
	for (i = 1; i <= 24; i++) {
		gpio_set_epfr(11, i, 1);
	}
#elif defined(MB9BF)
	// MDQCM1 and MDQCM0
	gpio_set_epfr(10, 4, 1);
	// MAD00, MAD08..MAD11
	for (i = 14; i <= 18; i++) {
		gpio_set_epfr(10, i, 1);
	}
	// MAD14..MAD15
	for (i = 21; i <= 22; i++) {
		gpio_set_epfr(10, i, 1);
	}
	// MAD01..MAD07, MADATA00..MADATA15
	for (i = 2; i <= 24; i++) {
		gpio_set_epfr(11, i, 1);
	}
	// MADCLK, MSDCKE, MRASX, MCASX, MSDWEX, MCSX8
	for (i = 0; i <= 5; i++) {
		gpio_set_epfr(20, i, 1);
	}
#endif

#ifdef S6E2CC
	// pins 21-22
	gpio_set_pfr(0x0, 0x8, 1);
	gpio_set_pfr(0x0, 0x9, 1);

	// pins 116-117
	gpio_set_pfr(0x1, 0xE, 1);
	gpio_set_pfr(0x1, 0xF, 1);

	// pins 118-124
	for (i = 0x4; i <= 0xA; i++) {
		gpio_set_pfr(0x2, i, 1);
	}

	// pins 39-40
	gpio_set_pfr(0x3, 0xD, 1);
	gpio_set_pfr(0x3, 0xE, 1);

	// pins 169-170
	gpio_set_pfr(0x6, 0x2, 1);
	gpio_set_pfr(0x6, 0x3, 1);

	// pins 67-69, 76-81
	for (i = 0x2; i <= 0xA; i++) {
		gpio_set_pfr(0x7, i, 1);
	}
	// pin 56
	gpio_set_pfr(0x7, 0xE, 1);

	// pins 2-9, 13-20
	for (i = 0x0; i <= 0xF; i++) {
		gpio_set_pfr(0xA, i, 1);
	}
#elif defined(MB9BF)
	// pins 92-93
	for (i = 0xD; i <= 0xE; i++) {
		gpio_set_pfr(0x0, i, 1);
	}
	// pins 62-66
	for (i = 0x0; i <= 0x4; i++) {
		gpio_set_pfr(0x1, i, 1);
	}
	// pins 69, 74
	for (i = 0x7; i <= 0x8; i++) {
		gpio_set_pfr(0x1, i, 1);
	}
	// pins 14-17
	for (i = 0x0; i <= 0x3; i++) {
		gpio_set_pfr(0x3, i, 1);
	}
	// pins 23-29
	for (i = 0x9; i <= 0xF; i++) {
		gpio_set_pfr(0x3, i, 1);
	}
	// pins 34-35
	for (i = 0x2; i <= 0x3; i++) {
		gpio_set_pfr(0x4, i, 1);
	}
	// pins 47-50
	for (i = 0xB; i <= 0xE; i++) {
		gpio_set_pfr(0x4, i, 1);
	}
	// pins 2-13
	for (i = 0x0; i <= 0xB; i++) {
		gpio_set_pfr(0x5, i, 1);
	}
#endif

	*CGT_CKEN0_EXBCK = 0;
	*CGT_MRST0_EXBRST = 1;
	*CGT_MRST0_EXBRST = 0;
	*CGT_CKEN0_EXBCK = 1;

#ifdef S6E2CC
	*MODE0 = (1 << 11) | (1 << 7) | (1 << 2) | (1 << 0); // MPXDOFF, SHRTDOUT, RBMON, 16-bit
	*DCLKR = (1 << 4) | ((4 - 1) << 0);
	*TIM0 = ((1 - 1) << 28) | ((4 - 1) << 24) | ((1 - 1) << 20) | ((5 - 1) << 16) |
	        ((1 - 1) << 12) | ((1 - 1) << 8) | (0 << 4) | ((4 - 1) << 0);
	*AREA0 = (1 << 16) | (((0x60000000 >> 20) & 0xff) << 0);
	*ATIM0 = ((1 - 1) << 8) | (0 << 4) | ((1 - 1) << 0);
	*AMODE = 0;
#elif defined(MB9BF)
	*SDMODE |= (1 << 16);
	*DCLKR = (0 << 4) | ((4 - 1) << 0);
	while (*DCLKR & (1 << 4)) {}
	*DCLKR |= (1 << 4);
	while (!(*DCLKR & (1 << 4))) {}
	*SDMODE &= ~(1 << 16);
	value = *SDMODE;
	value &= ~((0xf << 12) | (0xf << 8) | (3 << 4) | (1 << 2) | (1 << 1));
	value |= (1 << 12) | (3 << 8) | (0 << 4) | (0 << 2) | (0 << 1);
	*SDMODE = value;
	*REFTIM = (0 << 24) | ((1 - 1) << 16) | ((52 - 1) << 0);
	*PWRDWN = 0;
	*SDTIM = ((1 - 1) << 24) | ((5 - 1) << 20) | ((3 - 1) << 16) | ((2 - 1) << 12) |
	         ((2 - 1) << 8) | ((5 - 1) << 4) | ((2 - 1) << 0);
	//*SDTIM |= 1u << 31; // BOFF
	*SDMODE |= 1;
#endif
}

#ifdef S6E2CC
static void ethernet_setup(void)
{
	int i;

	for (i = 0; i <= 10; i++) {
		gpio_set_pfr(0xC, i, 1);
	}
	for (i = 12; i <= 15; i++) {
		gpio_set_pfr(0xC, i, 1);
	}
	for (i = 0; i <= 2; i++) {
		gpio_set_pfr(0xD, i, 1);
	}

	for (i = 18; i <= 28; i++) {
		gpio_set_epfr(14, i, 1);
	}
	gpio_set_epfr(14, 29, 0);

	gpio_set_pfr(0x6, 0xA, 0);
	gpio_set_ddr(0x6, 0xA, 1);

	gpio_set_pfr(0x6, 0xE, 0);
	gpio_set_ddr(0x6, 0xE, 1);
}

static void start_kernel(void)
{
	void (*kernel)(uint32_t reserved, uint32_t mach, uint32_t dt)
		= (void (*)(uint32_t, uint32_t, uint32_t))(0x00004000 | 1);

	kernel(0, ~0UL, 0x00002000);
}
#endif

int main(void)
{
#ifdef S6E2CC
	volatile uint32_t *psram32 = (void *)0x60000000;
	//volatile uint8_t *psram8   = (void *)0x60000000;
#endif
#ifdef MB9BF
	volatile uint32_t *sdram32 = (void *)0x70000000;
	volatile uint8_t *sdram8 = (void *)0x70000000;
#endif
	uint8_t val;
	int i;

	watchdog_disable();
	trace_buffer_enable();
	clock_setup();
	trimming_setup();

#ifdef S6E2CC
	gpio_set_pdor(0xB, 0x2, 0);
	gpio_set_ddr(0xB, 0x2, 1);
	gpio_set_pfr(0xB, 0x2, 0);
	gpio_set_ade(18, 0);
	gpio_set_pdor(0xB, 0x2, 0);

	gpio_set_pdor(0x1, 0x8, 0);
	gpio_set_ddr(0x1, 0x8, 1);
	gpio_set_pfr(0x1, 0x8, 0);
	gpio_set_ade(8, 0);
	gpio_set_pdor(0x1, 0x8, 1);

	gpio_set_pdor(0x1, 0xA, 0);
	gpio_set_ddr(0x1, 0xA, 1);
	gpio_set_pfr(0x1, 0xA, 0);
	gpio_set_ade(10, 0);
	gpio_set_pdor(0x1, 0xA, 0);
#elif defined(MB9BF)
	gpio_set_pdor(0x2, 0x7, 0);
	gpio_set_ddr(0x2, 0x7, 1);

	gpio_set_pdor(0x3, 0x8, 0);
	gpio_set_ddr(0x3, 0x8, 1);

	gpio_set_pdor(0xE, 0x0, 0);
	gpio_set_ddr(0xE, 0x0, 1);
#endif

	uart_setup();
	uart_putch('x');

	ebif_setup();
#ifdef S6E2CC
	/**psram32 = 0xdeadbeef;
	for (i = 0; i < 4; i++) {
		uint8_t val = psram8[i];
		uart_putch(((val >> 4) > 9) ? 'A' + (val >> 4) - 0xA : '0' + (val >> 4));
		uart_putch(((val & 0xf) > 9) ? 'A' + (val & 0xf) - 0xA : '0' + (val & 0xf));
	}*/
	for (i = 0; i < 0x00200000 / sizeof(*psram32); i++) {
		psram32[i] = 0;
	}
#elif defined(MB9BF)
	sdram32[0] = 0xdeadbeef;
	for (i = 0; i < 4; i++) {
		uint8_t val = sdram8[i];
		uart_putch(((val >> 4) > 9) ? 'A' + (val >> 4) - 0xA : '0' + (val >> 4));
		uart_putch(((val & 0xf) > 9) ? 'A' + (val & 0xf) - 0xA : '0' + (val & 0xf));
	}
	for (i = 0; i < 0x01000000 / sizeof(*sdram32); i++) {
		sdram32[i] = 0;
		//if ((i % 1024) == 0)
		//	uart_putch('.');
	}
#endif

#ifdef S6E2CC
	ethernet_setup();
	gpio_set_pdor(0x1, 0xA, 1);
	start_kernel();
#endif

	while (1) {
#ifdef S6E2CC
		val = gpio_get_pdor(0xB, 0x2);
#elif defined(MB9BF)
		val = gpio_get_pdor(0x2, 0x7);
#endif
		if (val) {
#ifdef S6E2CC
			gpio_set_pdor(0xB, 0x2, 0);
			gpio_set_pdor(0x1, 0x8, 1);
			gpio_set_pdor(0x1, 0xA, 1);
#elif defined(MB9BF)
			gpio_set_pdor(0x2, 0x7, 0);
			gpio_set_pdor(0x3, 0x8, 1);
			gpio_set_pdor(0xE, 0x0, 1);
#endif
		} else {
#ifdef S6E2CC
			gpio_set_pdor(0xB, 0x2, 1);
			gpio_set_pdor(0x1, 0x8, 0);
			gpio_set_pdor(0x1, 0xA, 0);
#elif defined(MB9BF)
			gpio_set_pdor(0x2, 0x7, 1);
			gpio_set_pdor(0x3, 0x8, 0);
			gpio_set_pdor(0xE, 0x0, 0);
#endif
		}
		for (i = 0; i < 10000000; i++) {
			asm volatile ("nop");
		}
	}

	return 0;
}

static void noop(void)
{
	while (1) {
	}
}

extern unsigned int _end_text;
extern unsigned int _start_data;
extern unsigned int _end_data;
extern unsigned int _start_bss;
extern unsigned int _end_bss;

void reset(void);

void reset(void)
{
	unsigned int *src, *dst;

	//asm volatile ("cpsid i");

	src = &_end_text;
	dst = &_start_data;
	while (dst < &_end_data) {
		*dst++ = *src++;
	}

	dst = &_start_bss;
	while (dst < &_end_bss) {
		*dst++ = 0;
	}

	main();
}

extern unsigned long _stack_top;

__attribute__((section(".vector_table")))
void (*vector_table[16 + 128])(void) = {
	(void (*))&_stack_top,
	reset,
	noop,
	noop,
	noop,
	noop,
	noop,
	NULL,
	NULL,
	NULL,
	NULL,
	noop,
	noop,
	NULL,
	noop,
	noop,
	[16 ... 127] = noop,
};
