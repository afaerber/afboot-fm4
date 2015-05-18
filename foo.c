#include <stdlib.h>
#include <stdint.h>

#define GPIO_BASE	0x4006F000
#define GPIO_PFRx	(GPIO_BASE + 0x000)
#define GPIO_PCRx	(GPIO_BASE + 0x100)
#define GPIO_DDRx	(GPIO_BASE + 0x200)
#define GPIO_PDIRx	(GPIO_BASE + 0x300)
#define GPIO_PDORx	(GPIO_BASE + 0x400)

#define WDG_BASE	0x40011000

static void watchdog_disable(void)
{
	volatile uint32_t *WDG_CTL = (void *)(WDG_BASE + 0x008);
	volatile uint32_t *WDG_LCK = (void *)(WDG_BASE + 0xC00);

	*WDG_LCK = 0x1ACCE551;
	*WDG_LCK = 0xE5331AAE;
	*WDG_CTL = 0;
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
