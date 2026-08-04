#ifndef _ADS1248_H_
#define _ADS1248_H_

// Constantes de comando/registrador do TI ADS1248 (datasheet SBAS426H,
// secoes 9.5.3 "Commands" e 9.6.3 "ADS1247 and ADS1248 Register Map").
// So constantes aqui - o driver de verdade mora em main/light_sensor_ads1248.cpp,
// seguindo o mesmo padrao ja usado pelo componente cs5534 (cs553x.h).

#include <stdint.h>

// ---- Comandos SPI (1 byte, alguns tem 'x' no bit 0 = don't care) ----
#define ADS1248_CMD_WAKEUP   0x00
#define ADS1248_CMD_SLEEP    0x02
#define ADS1248_CMD_SYNC     0x04
#define ADS1248_CMD_RESET    0x06
#define ADS1248_CMD_NOP      0xFF
#define ADS1248_CMD_RDATA    0x12
#define ADS1248_CMD_RDATAC   0x14
#define ADS1248_CMD_SDATAC   0x16
#define ADS1248_CMD_RREG     0x20 // OR com endereco do primeiro registrador (0-15)
#define ADS1248_CMD_WREG     0x40 // OR com endereco do primeiro registrador (0-15)
#define ADS1248_CMD_SYSOCAL  0x60
#define ADS1248_CMD_SYSGCAL  0x61
#define ADS1248_CMD_SELFOCAL 0x62

// ---- Enderecos de registrador ----
#define ADS1248_REG_MUX0     0x00
#define ADS1248_REG_VBIAS    0x01
#define ADS1248_REG_MUX1     0x02
#define ADS1248_REG_SYS0     0x03
#define ADS1248_REG_OFC0     0x04
#define ADS1248_REG_OFC1     0x05
#define ADS1248_REG_OFC2     0x06
#define ADS1248_REG_FSC0     0x07
#define ADS1248_REG_FSC1     0x08
#define ADS1248_REG_FSC2     0x09
#define ADS1248_REG_IDAC0    0x0A
#define ADS1248_REG_IDAC1    0x0B
#define ADS1248_REG_GPIOCFG  0x0C
#define ADS1248_REG_GPIODIR  0x0D
#define ADS1248_REG_GPIODAT  0x0E

// ---- MUX0 (00h): BCS[1:0] | MUX_SP[2:0] | MUX_SN[2:0] ----
// Indices de entrada analogica (0-7) usados para montar MUX_SP e MUX_SN.
#define ADS1248_AIN0 0
#define ADS1248_AIN1 1
#define ADS1248_AIN2 2
#define ADS1248_AIN3 3
#define ADS1248_AIN4 4
#define ADS1248_AIN5 5
#define ADS1248_AIN6 6
#define ADS1248_AIN7 7

#define ADS1248_MUX0_BYTE(mux_sp, mux_sn) \
    ((uint8_t)((((mux_sp) & 0x07) << 3) | ((mux_sn) & 0x07)))

// ---- MUX1 (02h): CLKSTAT | VREFCON[1:0] | REFSELT[1:0] | MUXCAL[2:0] ----
#define ADS1248_VREFCON_OFF        (0x0 << 5) // referencia interna sempre desligada (default)
#define ADS1248_VREFCON_ALWAYS_ON  (0x1 << 5)
#define ADS1248_VREFCON_AUTO       (0x2 << 5) // liga durante conversao, desliga em SLEEP/START baixo
#define ADS1248_REFSELT_REFP0N0    (0x0 << 3) // REFP0/REFN0 (default)
#define ADS1248_REFSELT_REFP1N1    (0x1 << 3)
#define ADS1248_REFSELT_INTERNAL   (0x2 << 3)
#define ADS1248_MUXCAL_NORMAL      0x0

// ---- SYS0 (03h): 0 | PGA[2:0] | DR[3:0] ----
#define ADS1248_PGA_1    (0x0 << 4)
#define ADS1248_PGA_2    (0x1 << 4)
#define ADS1248_PGA_4    (0x2 << 4)
#define ADS1248_PGA_8    (0x3 << 4)
#define ADS1248_PGA_16   (0x4 << 4)
#define ADS1248_PGA_32   (0x5 << 4)
#define ADS1248_PGA_64   (0x6 << 4)
#define ADS1248_PGA_128  (0x7 << 4)

#define ADS1248_DR_5SPS    0x0
#define ADS1248_DR_10SPS   0x1
#define ADS1248_DR_20SPS   0x2
#define ADS1248_DR_40SPS   0x3
#define ADS1248_DR_80SPS   0x4
#define ADS1248_DR_160SPS  0x5
#define ADS1248_DR_320SPS  0x6
#define ADS1248_DR_640SPS  0x7
#define ADS1248_DR_1000SPS 0x8
#define ADS1248_DR_2000SPS 0x9

// ---- IDAC0 (0Ah): ID[3:0] (RO) | DRDY MODE | IMAG[2:0] ----
#define ADS1248_DRDY_MODE_DOUT_ONLY (0x0 << 3) // default: DOUT/DRDY so como DOUT
#define ADS1248_IMAG_OFF   0x0 // sem fonte de excitacao (default, nao usado no sensor de luz)

#endif // _ADS1248_H_
