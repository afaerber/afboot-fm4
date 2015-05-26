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

//unsigned long masterclk;

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
	//const unsigned long clkmo = 4000000UL;

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
	*PLL_CTL2 = 0x31;

	*SCM_CTL |= CLOCK_SCM_CTL_PLLE;
	while (!(*SCM_STR & CLOCK_SCM_STR_PLRDY)) {}

	*SCM_CTL |= CLOCK_SCM_CTL_RCS_MAIN_PLL;
	while ((*SCM_STR & CLOCK_SCM_STR_RCM_MASK) != CLOCK_SCM_STR_RCM_MAIN_PLL) {}

	//masterclk = clkmo * ((*PLL_CTL2 & 0x3f) + 1);
	//masterclk /= (((*PLL_CTL1 >> 4) & 0xf) + 1);
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

	gpio_set_pfr(0x2, 0x1, 1);

	gpio_set_ade(31, 0);
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
	//val32 = masterclk / 2;
	//val32 /= 115200;
	//val32 -= 1;
	//*BGR = val32 & 0x7fff;
	*BGR = 867 & 0x7fff;
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

int main(void)
{
	uint8_t val;
	int i;

	watchdog_disable();
	trace_buffer_enable();
	clock_setup();
	trimming_setup();

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

	uart_setup();
	uart_putch('x');

	while (1) {
		val = gpio_get_pdor(0xB, 0x2);
		if (val) {
			gpio_set_pdor(0xB, 0x2, 0);
			gpio_set_pdor(0x1, 0x8, 1);
			gpio_set_pdor(0x1, 0xA, 1);
		} else {
			gpio_set_pdor(0xB, 0x2, 1);
			gpio_set_pdor(0x1, 0x8, 0);
			gpio_set_pdor(0x1, 0xA, 0);
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
void (*vector_table[16 + 91])(void) = {
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
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
};
