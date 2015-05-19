#include <stdlib.h>
#include <stdint.h>

#define GPIO_BASE	0x4006F000
#define GPIO_PFRx	(GPIO_BASE + 0x000)
#define GPIO_PCRx	(GPIO_BASE + 0x100)
#define GPIO_DDRx	(GPIO_BASE + 0x200)
#define GPIO_PDIRx	(GPIO_BASE + 0x300)
#define GPIO_PDORx	(GPIO_BASE + 0x400)

#define FLASH_BASE	0x40000000

#define FLASH_FBFCR_BE	(1UL << 0)

#define CLOCK_BASE	0x40010000

#define CLOCK_SCM_CTL_MOSCE		(1UL << 1)
#define CLOCK_SCM_CTL_PLLE		(1UL << 4)
#define CLOCK_SCM_CTL_RCS_MAIN_PLL	(0x2UL << 5)

#define CLOCK_SCM_STR_MORDY	(1UL << 1)
#define CLOCK_SCM_STR_PLRDY	(1UL << 4)
#define CLOCK_SCM_STR_RCM_MAIN_PLL	(0x2UL << 5)
#define CLOCK_SCM_STR_RCM_MASK		(0x7UL << 5)

#define WDG_BASE	0x40011000

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
}

int main(void)
{
	volatile uint32_t *GPIO_PFR1  = (void *)(GPIO_BASE + 0x004);
	volatile uint32_t *GPIO_PFRB  = (void *)(GPIO_BASE + 0x02C);
	volatile uint32_t *GPIO_DDR1  = (void *)(GPIO_BASE + 0x204);
	volatile uint32_t *GPIO_DDRB  = (void *)(GPIO_BASE + 0x22C);
	volatile uint32_t *GPIO_PDOR1 = (void *)(GPIO_BASE + 0x404);
	volatile uint32_t *GPIO_PDORB = (void *)(GPIO_BASE + 0x42C);
	uint32_t val;
	int i;

	watchdog_disable();
	trace_buffer_enable();
	clock_setup();

	*GPIO_PFRB &= ~(1 << 0x2);
	*GPIO_PFR1 &= ~((1 << 0x8) | (1 << 0xa));
	*GPIO_DDRB |= 1 << 0x2;
	*GPIO_DDR1 |= (1 << 0x8) | (1 << 0xa);

	while (1) {
		val = *GPIO_PDORB;
		if (val & (1 << 0x2)) {
			*GPIO_PDORB = val & ~(1 << 0x2);
			*GPIO_PDOR1 |= (1 << 0x8) | (1 << 0xa);
		} else {
			*GPIO_PDORB = val | (1 << 0x2);
			*GPIO_PDOR1 &= ~((1 << 0x8) | (1 << 0xa));
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
