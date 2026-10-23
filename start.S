# Copyright 2023 Dual Tachyon
# https://github.com/DualTachyon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

	.cpu cortex-m0
	.fpu softvfp
	.thumb

	.global Stack
	.global HandlerReset
	.global Main

	.global OVERLAY_Install
	.global BOARD_FLASH_Init
	.global BSS_Init

	.global SystickHandler

	.section .text.isr

Stack:
	.long	0x20003FF0
Reset:
	.long	HandlerReset + 1
NMI:
	.long	HandlerFault + 1
HardFault:
	.long	HandlerFault + 1
Reserved_10:
	.long	0
Reserved_14:
	.long	0
Reserved_18:
	.long	0
Reserved_1C:
	.long	0
Reserved_20:
	.long	0
Reserved_24:
	.long	0
Reserved_28:
	.long	0
SVCall:
	.long	HandlerFault + 1
Reserved_30:
	.long	0
Reserved_34:
	.long	0
PendSV:
	.long	HandlerFault + 1
SysTick:
	.long	SystickHandler + 1
WWDT:
	.long	HandlerFault + 1
IWDT:
	.long	HandlerFault + 1
RTC:
	.long	HandlerFault + 1
DMA:
	.long	HandlerFault + 1
SARADC:
	.long	HandlerFault + 1
TIMER_BASE0:
	.long	HandlerFault + 1
TIMER_BASE1:
	.long	HandlerFault + 1
TIMER_PLUS0:
	.long	HandlerFault + 1
TIMER_PLUS1:
	.long	HandlerFault + 1
PWM_BASE0:
	.long	HandlerFault + 1
PWM_BASE1:
	.long	HandlerFault + 1
PWM_PLUS0:
	.long	HandlerFault + 1
PWM_PLUS1:
	.long	HandlerFault + 1
UART0:
	.long	HandlerFault + 1
UART1:
	.long	HandlerFault + 1
UART2:
	.long	HandlerFault + 1
SPI0:
	.long	HandlerFault + 1
SPI1:
	.long	HandlerFault + 1
IIC0:
	.long	HandlerFault + 1
IIC1:
	.long	HandlerFault + 1
CMP:
	.long	HandlerFault + 1
TIMER_BASE2:
	.long	HandlerFault + 1
GPIOA5:
	.long	HandlerFault + 1
GPIOA6:
	.long	HandlerFault + 1
GPIOA7:
	.long	HandlerFault + 1
GPIOB0:
	.long	HandlerFault + 1
GPIOB1:
	.long	HandlerFault + 1
GPIOC0:
	.long	HandlerFault + 1
GPIOC1:
	.long	HandlerFault + 1
GPIOA:
	.long	HandlerFault + 1
GPIOB:
	.long	HandlerFault + 1
GPIOC:
	.long	HandlerFault + 1

	.section .text

# VUURWERK: G087 every fault and unused vector requests a system reset
# (SYSRESETREQ, as NVIC_SystemReset) so a fault while keyed cannot hold
# the carrier up; BK4819_Init stops TX after the reboot.
HandlerFault:
	ldr	r0, =0xE000ED0C
	ldr	r1, =0x05FA0004
	str	r1, [r0]
	b	.

HandlerReset:
	ldr	r0, =0x20003FF0
	mov	sp, r0
	bl	DATA_Init
	bl	BSS_Init
#if defined(ENABLE_OVERLAY)
	bl	BOARD_FLASH_Init
#endif
	bl	Main
	b 	.

