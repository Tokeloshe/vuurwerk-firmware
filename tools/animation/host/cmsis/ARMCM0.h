/* host stand-in for the CMSIS device header: no interrupts, no barriers */
#ifndef HOST_ARMCM0_H
#define HOST_ARMCM0_H
#include <stdint.h>
typedef int IRQn_Type;
static inline void __disable_irq(void) {}
static inline void __enable_irq(void) {}
static inline void __DSB(void) {}
static inline void __ISB(void) {}
static inline void __WFI(void) {}
static inline void __NOP(void) {}
static inline void NVIC_EnableIRQ(IRQn_Type n) { (void)n; }
static inline void NVIC_DisableIRQ(IRQn_Type n) { (void)n; }
static inline void NVIC_SystemReset(void) { __builtin_trap(); }
#endif
