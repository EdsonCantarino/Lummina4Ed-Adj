#include "include/light_sensor.h"
#include "esp_system.h"
#include "sdkconfig.h"

// Driver do CS5534 (Cirrus) - so compilado quando essa placa e a selecionada
// em "ADC (conversor analogico)" no menuconfig. O driver do ADS1248 (TI, placa
// nova) mora em light_sensor_ads1248.cpp e implementa a mesma API publica.
#if CONFIG_ADC_CHIP_CS5534

static const char *TAG = "LIGHT_SENSOR";

//SemaphoreHandle_t xMutex;

char sDebug[120];

#define HIGH 1
#define LOW 0

#define SPI_MODE  0
#define MOSI_PIN (gpio_num_t)11
#define MISO_PIN (gpio_num_t)13

//#define MISO_PIN (gpio_num_t)11
//#define MOSI_PIN (gpio_num_t)13

#define SCLK_PIN  (gpio_num_t)12
#define CS_PIN    (gpio_num_t)10

// Timeout de espera pelo sinal DRDY do CS5532/CS5534 (linha MOSI baixa = pronto).
// Leitura normal fica na faixa de ~10-20ms; 1000ms da bastante margem sem travar
// o teste indefinidamente caso o ADC nao sinalize pronto (falha intermitente de hardware).
#define CS5532_DRDY_TIMEOUT_MS 1000

unsigned char vTimeOutCS5532 = 0;
signed long vOffset = 0;

/// Initial Reset SPI
#define SYNC1 0xFF
#define SYNC0 0xFE

/// Command Register
#define CMDL_Single_Access 0x00
#define CMDL_Array_Access  0x40

#define CMDL_CH1      0x00
#define CMDL_CH2      0x10
#define CMDL_CH3      0x20
#define CMDL_CH4      0x30
//
#define CMDL_Write    0x00
#define CMDL_Read     0x08
//
#define CMDL_Resev1   0x00
#define CMDL_OffSet   0x01
#define CMDL_Gain     0x02
#define CMDL_Config   0x03
#define CMDL_Chan_Setup    0x05
#define CMDL_Resev2   0x06
#define CMDL_Resev3   0x07

#define CMDH_Multi_Convertion 0x40   | 0x080
#define CMDH_Single_Convertion 0x00 | 0x80

#define CMDH_Channel_Setup_Pt1 0x00  | 0x80
#define CMDH_Channel_Setup_Pt2 0x08  | 0x80
#define CMDH_Channel_Setup_Pt3 0x10  | 0x80
#define CMDH_Channel_Setup_Pt4 0x18  | 0x80
#define CMDH_Channel_Setup_Pt5 0x20  | 0x80
#define CMDH_Channel_Setup_Pt6 0x28  | 0x80
#define CMDH_Channel_Setup_Pt7 0x30  | 0x80
#define CMDH_Channel_Setup_Pt8 0x38  | 0x80

#define CMDH_Conver_Calib_Normal 				0x00 | 0x80
#define CMDH_Conver_Calib_Self_Offset 	0x01 | 0x80
#define CMDH_Conver_Calib_Self_Gain 		0x02  | 0x80
#define CMDH_Conver_Calib_Reserv1 			0x03  | 0x80
#define CMDH_Conver_Calib_Reserv2 			0x04  | 0x80
#define CMDH_Conver_Calib_System_Offset 0x05  | 0x80
#define CMDH_Conver_Calib_System_Gain 	0x06   | 0x80
#define CMDH_Conver_Calib_Reserv3 			0x07   | 0x80

/////////////////////////////////////// Channel Setup Register
// First Group
// Chanel
#define CMD_Setup1_CH0 0x00000000
#define CMD_Setup1_CH1 0x40000000
#define CMD_Setup1_CH2 0x80000000
#define CMD_Setup1_CH3 0xC0000000
unsigned char vCMD_Setup1_Channel = 0;

// Gain
#define CMD_Setup1_Gain1 0x00000000 //	 G=1
#define CMD_Setup1_Gain2 0x08000000 //	 G=2
#define CMD_Setup1_Gain3 0x10000000 //	 G=4
#define CMD_Setup1_Gain4 0x18000000 //	 G=8
#define CMD_Setup1_Gain5 0x20000000 //	 G=16
#define CMD_Setup1_Gain6 0x21000000 //	 G=32
#define CMD_Setup1_Gain7 0x30000000 //	 G=64
unsigned char vCMD_Setup1_Gain = 0;

// Word rate - continuos mode depend clock and FRS
#define CMD_Setup1_WR1  0x00800000 //	60 50
#define CMD_Setup1_WR2  0x01000000 // 30 25
#define CMD_Setup1_WR3  0x01800000 // 15 12,5
#define CMD_Setup1_WR4  0x02000000  // 7,5 6.25
#define CMD_Setup1_WR5  0x04000000 // 3840 3200
#define CMD_Setup1_WR6  0x04800000 //1920 1600
#define CMD_Setup1_WR7  0x05000000 // 960 800
#define CMD_Setup1_WR8  0x05800000  // 480 400
#define CMD_Setup1_WR9  0x06000000 // 240 200
unsigned char vCMD_Setup1_WR = 0;

// Uniplar Bipolar
#define CMD_Setup1_2POL 0x00000000
#define CMD_Setup1_1POL 0x00200000
unsigned char vCMD_Setup1_POL = 0;

// Output Latch
#define CMD_Setup1_OL00 0x00000000 //A0=0 e A1=0
#define CMD_Setup1_OL01 0x00100000 //A0=0 e A1=1
#define CMD_Setup1_OL10 0x00200000 //A0=1 e A1=0
#define CMD_Setup1_OL11 0x00300000 //A0=1 e A1=1
unsigned char vCMD_Setup1_OL = 0;

// Delay Time
#define CMD_Setup1_DT00 0x00000000 // Delay=0
#define CMD_Setup1_DT01 0x00040000 //delay= 1280 Mclk (FRS=0) or 1536 (FRS=1)
unsigned char vCMD_Setup1_DT = 0;

// Open Circuit Detect
#define CMD_Setup1_OCD_OFF 0x00000000 // Normal
#define CMD_Setup1_OCD_ON  0x00020000 //  Source current 300nA (AIN+) problem if temp use only thermocouple
unsigned char vCMD_Setup1_OCD = 0;

// OG OffSet/Gain register pointer olny OGS=1
#define CMD_Setup1_OG1 0x00000000 // offset gain register channer 1
#define CMD_Setup1_OG2 0x00010000 // offset gain register channer 2
#define CMD_Setup1_OG3 0x00020000 // offset gain register channer 3
#define CMD_Setup1_OG4 0x00030000 // offset gain register channer 4
unsigned char vCMD_Setup1_OG = 0;
/// Secound Group
// Chanel
#define CMD_Setup2_CH0 0x00000000
#define CMD_Setup2_CH1 0x00004000
#define CMD_Setup2_CH2 0x00008000
#define CMD_Setup2_CH3 0x0000C000
unsigned char vCMD_Setup2_Channel = 1;

// Gain
#define CMD_Setup2_Gain1 0x00000000 //	 G=1
#define CMD_Setup2_Gain2 0x00000800 //	 G=2
#define CMD_Setup2_Gain3 0x00001000 //	 G=4
#define CMD_Setup2_Gain4 0x00001800 //	 G=8
#define CMD_Setup2_Gain5 0x00002000 //	 G=16
#define CMD_Setup2_Gain6 0x00002100 //	 G=32
#define CMD_Setup2_Gain7 0x00003000 //	 G=64
unsigned char vCMD_Setup2_Gain = 0;

// Word rate - continuos mode depend clock and FRS
#define CMD_Setup2_WR1  0x00000080 //	60 50
#define CMD_Setup2_WR2  0x00000100 // 30 25
#define CMD_Setup2_WR3  0x00000180 // 15 12,5
#define CMD_Setup2_WR4  0x00000200  // 7,5 6.25
#define CMD_Setup2_WR5  0x00000400 // 3840 3200
#define CMD_Setup2_WR6  0x00000480 //1920 1600
#define CMD_Setup2_WR7  0x00000500 // 960 800
#define CMD_Setup2_WR8  0x00000580  // 480 400
#define CMD_Setup2_WR9  0x00000600 // 240 200
unsigned char vCMD_Setup2_WR = 0;

// Uniplar Bipolar
#define CMD_Setup2_2POL 0x00000000
#define CMD_Setup2_1POL 0x00000020
unsigned char vCMD_Setup2_POL = 1; // liga 1 polarizacao somente e torna saida unsigned

// Output Latch
#define CMD_Setup2_OL00 0x00000000 //A0=0 e A1=0
#define CMD_Setup2_OL01 0x00000010 //A0=0 e A1=1
#define CMD_Setup2_OL10 0x00000020 //A0=1 e A1=0
#define CMD_Setup2_OL11 0x00000030 //A0=1 e A1=1
unsigned char vCMD_Setup2_OL = 0;

// Delay Time
#define CMD_Setup2_DT00 0x00000000 // Delay=0
#define CMD_Setup2_DT01 0x00000004 //delay= 1280 Mclk (FRS=0) or 1536 (FRS=1)
unsigned char vCMD_Setup2_DT = 0;

// Open Circuit Detect
#define CMD_Setup2_OCD_OFF 0x00000000 // Normal
#define CMD_Setup2_OCD_ON  0x00000002 //  Source current 300nA (AIN+) problem if temp use only thermocouple
unsigned char vCMD_Setup2_OCD = 0;

// OG OffSet/Gain register pointer olny OGS=1
#define CMD_Setup2_OG1 0x00000000 // offset gain register channer 1
#define CMD_Setup2_OG2 0x00000001 // offset gain register channer 2
#define CMD_Setup2_OG3 0x00000002 // offset gain register channer 3
#define CMD_Setup2_OG4 0x00000003 // offset gain register channer 4
unsigned char vCMD_Setup2_OG = 0;

/////////////////////////////////////// Configuration Register
#define CMD_Conf_PSSP_Save   0x80000000 // sleep Mode
unsigned char vCMD_ConfPowerSafe = 0;

#define CMD_Conf_PDW_Active 0x40000000 // power down
unsigned char vCMD_ConfPowerDown = 0;

#define CMD_Conf_RS_Active 0x20000000 // reset
unsigned char vCMD_ResetSystem = 0;

#define CMD_Conf_RV_Active 0x10000000 // read - reset cycle active

#define CMD_Conf_Input_Short 0x08000000
unsigned char vCMD_Conf_Input_Short = 0;

#define CMD_Conf_GuardSignal 0x04000000
unsigned char vCMD_Conf_GuardSignal = 0; // Modifed A0 output

#define CMD_Conf_VRef_25_VA 0x00000000
#define CMD_Conf_VRef_10_25 0x02000000
unsigned char vCMD_Conf_VRef = 0;

#define CMD_Conf_OutLach00 0x00000000
#define CMD_Conf_OutLach01 0x00800000
#define CMD_Conf_OutLach10 0x01000000
#define CMD_Conf_OutLach11 0x01800000
unsigned char vCMD_Conf_OutLach = 0;

#define CMD_Conf_OutLachSelSetup 0x00000000 // setp register control out latch
#define CMD_Conf_OutLachSelConf 0x00400000 // setp register control out latch
unsigned char vCMD_Conf_OutLachSel = 0;

#define CMD_Conf_Offset_Gain_OGS_CS 0x00000000 // use CS1 and CS0 bits
#define CMD_Conf_Offset_Gain_OGS_OG 0x00100000 // use OG1 and OG0 bits
unsigned char vCMD_Conf_Offset_Gain_OGS = 0;

#define CMD_Conf_FilterRateSelect_Word 0x00000000 //use word to rate
#define CMD_Conf_FilterRateSelect_Scale 0x00800000 //use word to rate
unsigned char vCMD_Conf_FilterRateSelect = 0;

unsigned long confRegSetup = CMD_Setup1_CH0 | CMD_Setup1_Gain1 | CMD_Setup1_WR1
		| CMD_Setup1_2POL | CMD_Setup1_OL00 | CMD_Setup1_DT00
		| CMD_Setup1_OCD_OFF | CMD_Setup1_OG1 | CMD_Setup2_CH0
		| CMD_Setup2_Gain1 | CMD_Setup2_WR1 | CMD_Setup2_2POL | CMD_Setup2_OL00
		| CMD_Setup2_DT00 | CMD_Setup2_OCD_OFF | CMD_Setup2_OG1;

///////////////
unsigned long vOffSetCH = 0x00000000;

unsigned long vGainCH1 = 0x00100000;
unsigned long vGainCH2 = 0x00100000;
unsigned long vGainCH3 = 0x00100000;
unsigned long vGainCH4 = 0x00100000;

////////Convertion
#define  CONV_OverRange 0x00000004
#define  CONV_Channel   0x00000003
///////////////////////////////////////////////

#define coDelayCS5532 200

int read_pin(gpio_num_t pin) {
	return gpio_get_level(pin);
}

void write_pin(gpio_num_t pin, int level) {
	gpio_set_level(pin, level);
}

// Read_Pin_Dout_CS5532
int read_mosi_pin() {
	return read_pin(MOSI_PIN);
}

// Aguarda o DRDY igual as leituras normais, mas usado apenas nas rotinas de
// calibracao chamadas no boot (antes de qualquer teste rodar). Se o ADC nao
// sinalizar pronto dentro do timeout, nao ha teste em andamento pra
// preservar - reinicia o equipamento pra dar uma nova chance ao hardware
// (falha de energizacao/DRDY intermitente costuma sumir num novo boot), em
// vez de travar o boot para sempre esperando um sinal que pode nunca vir.
static void wait_drdy_or_reset_boot(const char *contexto) {
	uint16_t drdy_wait_ms = 0;

	while (read_mosi_pin()) {
		vTaskDelay(pdMS_TO_TICKS(10));
		drdy_wait_ms += 10;

		if (drdy_wait_ms >= CS5532_DRDY_TIMEOUT_MS) {
			ESP_LOGE(TAG,
					"Timeout aguardando DRDY do CS5532/CS5534 durante %s no boot - reiniciando",
					contexto);
			esp_restart();
		}
	}
}

// Write_Pin_Din_CS5532_H
// Write_Pin_Din_CS5532_L
void write_miso_pin(int level) {
	write_pin(MISO_PIN, level);
}

// Write_Pin_Clk_CS5532_H
// Write_Pin_Clk_CS5532_L
void write_sclk_pin(int level) {
	write_pin(SCLK_PIN, level);
}

// Write_Pin_CS_CS5532_H
// Write_Pin_CS_CS5532_L
void write_cs_pin(int level) {
	write_pin(CS_PIN, level);
}

/********************************************************************************/
// Delay CS5532
/********************************************************************************/
void Delay_CS5532(unsigned long vDelay) {
	for (; vDelay != 0; vDelay--)
		;

	//vTaskDelay(vDelay / portTICK_PERIOD_MS);
}

void Inic_CS_Line(void) {
//	printf("Aqui no CS LINE\n");

	write_sclk_pin(LOW);
	write_miso_pin(LOW);
	write_cs_pin(LOW);

	Delay_CS5532(coDelayCS5532);
//	printf("Saiu do CS LINE\n");
}
void End_CS_Line(void) {
//	printf("Aqui no END CS LINE\n");

	write_sclk_pin(LOW);
	write_miso_pin(LOW);
	write_cs_pin(HIGH);

	Delay_CS5532(coDelayCS5532);

//	printf("Saiu do END CS LINE\n");
}

/********************************************************************************/
// Write/Read CS5532 byte
/********************************************************************************/
unsigned char CS5532_BYTE(unsigned char vAux) {
	unsigned char i, res;
	res = 0;
	for (i = 0; i < 8; i++) {
		res = res << 1;
		if ((vAux & 0x80) != 0) {
			write_miso_pin(HIGH);
		} else {
			write_miso_pin(LOW);
		}

		Delay_CS5532(coDelayCS5532);

		write_sclk_pin(HIGH);

		Delay_CS5532(coDelayCS5532);

		if (read_mosi_pin()) {
			res = res | 0x01;
		}

		write_sclk_pin(LOW);
		Delay_CS5532(coDelayCS5532);
		vAux = vAux << 1;
	}
	return (res);
}

/********************************************************************************/
// Write/Read CS5532 Long
/********************************************************************************/
unsigned long CS5532_LONG(unsigned long cData) {
	unsigned long resl = 0;

//	printf("Aqui no CS5532_LONG\n");

	for (int i = 0; i < 32; i++) {
		resl = resl << 1;
		if ((cData & 0x80000000) != 0) {
			write_miso_pin(HIGH);
		} else {
			write_miso_pin(LOW);
		}

//		printf("Aqui delay 1\n");

		Delay_CS5532(coDelayCS5532);
		write_sclk_pin(HIGH);

//		printf("Aqui delay 2\n");
		Delay_CS5532(coDelayCS5532);

//		printf("Lendo o MOSI\n");
		if (read_mosi_pin())
			resl = resl | 0x00000001;

		write_sclk_pin(LOW);

//		printf("Aqui delay 3\n");
		Delay_CS5532(coDelayCS5532);
		cData = cData << 1;
	}

//	printf("Saiu no CS5532_LONG\n");
	return (resl);
}

/********************************************************************************/
// Read Simple Conversion CS5532
// first 8 bits command
// second 32 bits register
/********************************************************************************/
unsigned long CS5532_Read_Single_Conv_Setup1(void) {
	unsigned long resl = 0;
	unsigned char resc = 0;
	Inic_CS_Line();
	resc = CS5532_BYTE(CMDH_Single_Convertion | CMDH_Channel_Setup_Pt1);
	Delay_CS5532(coDelayCS5532);
	vTimeOutCS5532 = 10;
	while ((read_mosi_pin()) && (vTimeOutCS5532 != 0)) {
		vTaskDelay(pdMS_TO_TICKS(10));
	}

	if (vTimeOutCS5532 != 0) {
		resc = CS5532_BYTE(0x00);
		resl = CS5532_LONG(0X00000000);
		End_CS_Line();
		if ((resl & CONV_OverRange) != 0) {
			resl = 0xffffffff;
			sprintf(sDebug, "\r\nOverRange leitura peso");
			printf("%s", sDebug);
		} else {
			resc = resl & CONV_Channel;
			resl = resl >> 8;
			if (resl > vOffset) {
				resl = resl - vOffset;
			} else {
				resl = vOffset - resl;
				resl = ~resl + 1; //twos complement
			}
			sprintf(sDebug, "\r\nLeitura Canal %d = %ld", resc, resl);
			printf("%s", sDebug);
		}
	} else {
		sprintf(sDebug, "\r\nErro timeout SPI CS5532");
		printf("%s", sDebug);

		resl = 0xffffffff;
	}
	return (resl);
}

/******************************************************************/
// stop CS5532 continous conversion mode so that it can receive
// new command.
/******************************************************************/
unsigned char Turn_OFF_Cont_Conv() {
	unsigned char res = 0;
	if (read_mosi_pin()) {
		res = 0;
	} else {
		CS5532_BYTE(0x0FF);
		CS5532_LONG(0X00000000);
		res = 1;
	}
	return (res);
}

/********************************************************************************/
// Read Multi Conversion CS5532
// first 8 bits command
// second 32 bits register
/********************************************************************************/
unsigned long CS5532_Read_Multi_Conv_Setup1(void) {
	signed long resl = 0;
	unsigned char resc = 0;
	Inic_CS_Line();
	Delay_CS5532(coDelayCS5532);
	if (!(read_mosi_pin())) {
		resc = CS5532_BYTE(0x00);
		resl = CS5532_LONG(0X00000000);
	}
	End_CS_Line();
	if ((resl & CONV_OverRange) != 0) {
		resl = 0xffffffff;
		sprintf(sDebug, "\r\nOverRange leitura peso");
		printf("%s", sDebug);
	} else {
		if (resl != 0) {
			resc = (unsigned char) resl & CONV_Channel;
			resl = resl >> 8;
			if (resl > vOffset) {
				resl = resl - vOffset;
			} else {
				resl = vOffset - resl;
				resl = resl | 0x80000000;
			}
			sprintf(sDebug, "\r\nLeitura Canal %d = %ld", resc, resl);
			printf("%s", sDebug);
		} else {
			sprintf(sDebug, "\r\nNot Ready");
			printf("%s", sDebug);

			resl = 0xffffffff;
		}
	}
	return (resl);
}

/********************************************************************************/
//Send Commnand Register CS5532
// first 8 bits command
// second 32 bits register
/********************************************************************************/
unsigned long CS5532_CMD_REG(unsigned char CMD, unsigned long REG) {
	unsigned long resl = 0;
	unsigned char resc = 0;
	Inic_CS_Line();
/////////////////////////////
	resc = CS5532_BYTE(CMD);
	resl = CS5532_LONG(REG);
/////////////////////////////
	End_CS_Line();
	vTaskDelay(pdMS_TO_TICKS(5));
	return (resl);
}

/////////////////////////////////////////////////////7
// Channel Register Gain
/////////////////////////////////////////////

unsigned long CS5532_Channel_Setup_Regiter12(void) {

	unsigned long auxl = 0;
	// Chanel
	switch (vCMD_Setup1_Channel) {
	case 1:
		auxl = auxl | CMD_Setup1_CH1;
		break;
	case 2:
		auxl = auxl | CMD_Setup1_CH2;
		break;
	case 3:
		auxl = auxl | CMD_Setup1_CH3;
		break;
	default:
		auxl = auxl | CMD_Setup1_CH0;
		break;
	}

///Gain
	switch (vCMD_Setup1_Gain) {
	case 1:
		auxl = auxl | CMD_Setup1_Gain2;
		break;
	case 2:
		auxl = auxl | CMD_Setup1_Gain3;
		break;
	case 3:
		auxl = auxl | CMD_Setup1_Gain4;
		break;
	case 4:
		auxl = auxl | CMD_Setup1_Gain5;
		break;
	case 5:
		auxl = auxl | CMD_Setup1_Gain6;
		break;
	case 6:
		auxl = auxl | CMD_Setup1_Gain7;
		break;
	default:
		auxl = auxl | CMD_Setup1_Gain1;
		break;
	}

// Word rate - continuos mode depend clock and FRS
	switch (vCMD_Setup1_WR) {
	case 1:
		auxl = auxl | CMD_Setup1_WR2;
		break;
	case 2:
		auxl = auxl | CMD_Setup1_WR3;
		break;
	case 3:
		auxl = auxl | CMD_Setup1_WR4;
		break;
	case 4:
		auxl = auxl | CMD_Setup1_WR5;
		break;
	case 5:
		auxl = auxl | CMD_Setup1_WR6;
		break;
	case 6:
		auxl = auxl | CMD_Setup1_WR7;
		break;
	case 7:
		auxl = auxl | CMD_Setup1_WR8;
		break;
	case 8:
		auxl = auxl | CMD_Setup1_WR9;
		break;
	default:
		auxl = auxl | CMD_Setup1_WR1;
		break;
	}
// Uniplar Bipolar
	switch (vCMD_Setup1_POL) {
	case 1:
		auxl = auxl | CMD_Setup1_1POL;
		break;
	default:
		auxl = auxl | CMD_Setup1_2POL;
		break;
	}

// Output Latch
	switch (vCMD_Setup1_OL) {
	case 1:
		auxl = auxl | CMD_Setup1_OL01;
		break;
	case 2:
		auxl = auxl | CMD_Setup1_OL10;
		break;
	case 3:
		auxl = auxl | CMD_Setup1_OL11;
		break;
	default:
		auxl = auxl | CMD_Setup1_OL00;
		break;
	}

// Delay Time
	switch (vCMD_Setup1_DT) {
	case 1:
		auxl = auxl | CMD_Setup1_DT01;
		break;
	default:
		auxl = auxl | CMD_Setup1_DT00;
		break;
	}

// Open Circuit Detect
	switch (vCMD_Setup1_OCD) {
	case 1:
		auxl = auxl | CMD_Setup1_OCD_ON;
		break;
	default:
		auxl = auxl | CMD_Setup1_OCD_OFF;
		break;
	}

// OG OffSet/Gain register pointer olny OGS=1
	switch (vCMD_Setup1_OG) {
	case 2:
		auxl = auxl | CMD_Setup1_OG2;
		break;
	case 3:
		auxl = auxl | CMD_Setup1_OG3;
		break;
	case 4:
		auxl = auxl | CMD_Setup1_OG4;
		break;
	default:
		auxl = auxl | CMD_Setup1_OG1;
		break;
	}
// Chanel
	switch (vCMD_Setup2_Channel) {
	case 1:
		auxl = auxl | CMD_Setup2_CH1;
		break;
	case 2:
		auxl = auxl | CMD_Setup2_CH2;
		break;
	case 3:
		auxl = auxl | CMD_Setup2_CH3;
		break;
	default:
		auxl = auxl | CMD_Setup2_CH0;
		break;
	}

///Gain
	switch (vCMD_Setup2_Gain) {
	case 1:
		auxl = auxl | CMD_Setup2_Gain2;
		break;
	case 2:
		auxl = auxl | CMD_Setup2_Gain3;
		break;
	case 3:
		auxl = auxl | CMD_Setup2_Gain4;
		break;
	case 4:
		auxl = auxl | CMD_Setup2_Gain5;
		break;
	case 5:
		auxl = auxl | CMD_Setup2_Gain6;
		break;
	case 6:
		auxl = auxl | CMD_Setup2_Gain7;
		break;
	default:
		auxl = auxl | CMD_Setup2_Gain1;
		break;
	}

// Word rate - continuos mode depend clock and FRS
	switch (vCMD_Setup2_WR) {
	case 1:
		auxl = auxl | CMD_Setup2_WR2;
		break;
	case 2:
		auxl = auxl | CMD_Setup2_WR3;
		break;
	case 3:
		auxl = auxl | CMD_Setup2_WR4;
		break;
	case 4:
		auxl = auxl | CMD_Setup2_WR5;
		break;
	case 5:
		auxl = auxl | CMD_Setup2_WR6;
		break;
	case 6:
		auxl = auxl | CMD_Setup2_WR7;
		break;
	case 7:
		auxl = auxl | CMD_Setup2_WR8;
		break;
	case 8:
		auxl = auxl | CMD_Setup2_WR9;
		break;
	default:
		auxl = auxl | CMD_Setup2_WR1;
		break;
	}
// Uniplar Bipolar
	switch (vCMD_Setup2_POL) {
	case 1:
		auxl = auxl | CMD_Setup2_1POL;
		break;
	default:
		auxl = auxl | CMD_Setup2_2POL;
		break;
	}

// Output Latch
	switch (vCMD_Setup2_OL) {
	case 1:
		auxl = auxl | CMD_Setup2_OL01;
		break;
	case 2:
		auxl = auxl | CMD_Setup2_OL10;
		break;
	case 3:
		auxl = auxl | CMD_Setup2_OL11;
		break;
	default:
		auxl = auxl | CMD_Setup2_OL00;
		break;
	}

// Delay Time
	switch (vCMD_Setup2_DT) {
	case 1:
		auxl = auxl | CMD_Setup2_DT01;
		break;
	default:
		auxl = auxl | CMD_Setup2_DT00;
		break;
	}

// Open Circuit Detect
	switch (vCMD_Setup1_OCD) {
	case 1:
		auxl = auxl | CMD_Setup2_OCD_ON;
		break;
	default:
		auxl = auxl | CMD_Setup2_OCD_OFF;
		break;
	}

// OG OffSet/Gain register pointer olny OGS=1
	switch (vCMD_Setup2_OG) {
	case 2:
		auxl = auxl | CMD_Setup2_OG2;
		break;
	case 3:
		auxl = auxl | CMD_Setup2_OG3;
		break;
	case 4:
		auxl = auxl | CMD_Setup2_OG4;
		break;
	default:
		auxl = auxl | CMD_Setup2_OG1;
		break;
	}
	return (auxl);
}

///////////////////// Configuration Register
unsigned long CS5532_Conf_Reg(void) {

	unsigned long auxl = 0;
	if (vCMD_ConfPowerSafe != 0)
		auxl = auxl | CMD_Conf_PSSP_Save;
	if (vCMD_ConfPowerDown != 0)
		auxl = auxl | CMD_Conf_PDW_Active;
	if (vCMD_ResetSystem != 0)
		auxl = auxl | CMD_Conf_RS_Active;
	if (vCMD_Conf_Input_Short != 0)
		auxl = auxl | CMD_Conf_Input_Short;
	if (vCMD_Conf_GuardSignal != 0)
		auxl = auxl | CMD_Conf_GuardSignal;
	if (vCMD_Conf_VRef != 0)
		auxl = auxl | CMD_Conf_VRef_10_25;
	switch (vCMD_Conf_OutLach) {
	case 1:
		auxl = auxl | CMD_Conf_OutLach01;
		break;
	case 2:
		auxl = auxl | CMD_Conf_OutLach10;
		break;
	case 3:
		auxl = auxl | CMD_Conf_OutLach01;
		break;
	default:
		auxl = auxl | CMD_Conf_OutLach00;
		break;
	}
	if (vCMD_Conf_OutLachSel == 0)
		auxl = auxl | CMD_Conf_OutLachSelSetup;
	else
		auxl = auxl | CMD_Conf_OutLachSelConf;
	if (vCMD_Conf_Offset_Gain_OGS == 0)
		auxl = auxl | CMD_Conf_Offset_Gain_OGS_CS;
	else
		auxl = auxl | CMD_Conf_Offset_Gain_OGS_OG;
	if (vCMD_Conf_FilterRateSelect == 0)
		auxl = auxl | CMD_Conf_FilterRateSelect_Word;
	else
		auxl = auxl | CMD_Conf_FilterRateSelect_Scale;
	return (auxl);
}

/******************************************************************/
//                      CS5532 Reset
// Set RS bit in configuration reg to "1" to trigger reset, then
// change it back to "0"
// Return 0x10 if reset is successful. Return 0x0 if reset
// failed.
/******************************************************************/
unsigned long CS5532_Reset(void) {
	unsigned long resl = 0;
	unsigned long auxl = 0;

	vCMD_ResetSystem = 1; // reset
	auxl = CS5532_Conf_Reg();
	vCMD_ResetSystem = 0;
	resl = CS5532_CMD_REG(
	CMDL_Single_Access | CMDL_CH1 | CMDL_Write | CMDL_Config, auxl);

	vTaskDelay(5 / portTICK_PERIOD_MS);

	vCMD_ResetSystem = 0;
	auxl = CS5532_Conf_Reg();
	resl = CS5532_CMD_REG(
	CMDL_Single_Access | CMDL_CH1 | CMDL_Read | CMDL_Config, auxl);

	printf("Rx=%08lx", resl);

	return (resl);
}

//**********************************************************
// Tara res=1 success 0 fail
//***********************************************************
unsigned char Tara(void) {
	unsigned char res = 0;
	unsigned long auxl = 0;
	vOffset = 0;
	auxl = CS5532_Read_Single_Conv_Setup1();
	if (auxl != 0xffffffff) {
		vOffset = auxl;
		res = 1;
	} else {
		res = 0;
		vOffset = 0;
	}
	return (res);
}
/******************************************************************/
// CS5532 Gain Calibration
// not work if  (VREF+ - VREF-) > 2.5V.
//Self calibration of gain is performed in the GAIN =1x mode
/******************************************************************/
unsigned long CS5532_Auto_Gain_Calibration(unsigned char vChannel) {
	unsigned long auxl = 0;
	unsigned long resl = 0;
	unsigned char cmd = 0;
	unsigned char vCh = 0;
	vCMD_Setup2_Gain = 0; // only works if Gain=1
	vCMD_Setup1_Gain = 0;
	auxl = CS5532_Conf_Reg();
	cmd = CMDH_Single_Convertion | CMDH_Conver_Calib_Self_Gain;
	if (vChannel == 1)
		vCh = CMDH_Channel_Setup_Pt1;
	if (vChannel == 2)
		vCh = CMDH_Channel_Setup_Pt2;
	if (vChannel == 3)
		vCh = CMDH_Channel_Setup_Pt3;
	if (vChannel == 4)
		vCh = CMDH_Channel_Setup_Pt4;
	if (vChannel == 5)
		vCh = CMDH_Channel_Setup_Pt5;
	if (vChannel == 6)
		vCh = CMDH_Channel_Setup_Pt6;
	if (vChannel == 7)
		vCh = CMDH_Channel_Setup_Pt7;
	if (vChannel == 8)
		vCh = CMDH_Channel_Setup_Pt8;
	cmd = cmd | vCh;
	resl = CS5532_CMD_REG(cmd, auxl);
	wait_drdy_or_reset_boot("CS5532_Auto_Gain_Calibration");

	return (resl);
}

/******************************************************************/
// CS5532 OffSet Calibration
// not work if  (VREF+ - VREF-) > 2.5V.
/******************************************************************/
unsigned long CS5532_Auto_OffSet_Calibration(unsigned char vChannel) {
	unsigned long auxl = 0;
	unsigned long resl = 0;
	unsigned char cmd = 0;
	unsigned char vCh;
	auxl = CS5532_Conf_Reg();
	auxl = auxl | CMD_Conf_Input_Short;
	cmd = CMDH_Single_Convertion | CMDH_Conver_Calib_Self_Offset;
	if (vChannel == 1)
		vCh = CMDH_Channel_Setup_Pt1;
	if (vChannel == 2)
		vCh = CMDH_Channel_Setup_Pt2;
	if (vChannel == 3)
		vCh = CMDH_Channel_Setup_Pt3;
	if (vChannel == 4)
		vCh = CMDH_Channel_Setup_Pt4;
	if (vChannel == 5)
		vCh = CMDH_Channel_Setup_Pt5;
	if (vChannel == 6)
		vCh = CMDH_Channel_Setup_Pt6;
	if (vChannel == 7)
		vCh = CMDH_Channel_Setup_Pt7;
	if (vChannel == 8)
		vCh = CMDH_Channel_Setup_Pt8;
	cmd = cmd | vCh;
	resl = CS5532_CMD_REG(cmd, auxl);
	wait_drdy_or_reset_boot("CS5532_Auto_OffSet_Calibration");

// turn off input short
	cmd = CMDH_Single_Convertion | vCh;
	resl = CS5532_CMD_REG(cmd, auxl);
	return (resl);
}

//void task_isr(void *arg) {
//	BaseType_t aux = false;
//	xSemaphoreGiveFromISR(xMutex, &aux);
//	//ets_printf("ISR\n");
//
//	if (aux) {
//		portYIELD_FROM_ISR();
//	}
//
//}

void setup_pins() {
	gpio_config_t cs_conf;
	cs_conf.pull_down_en = (gpio_pulldown_t) 0;
	cs_conf.intr_type = (gpio_int_type_t) GPIO_INTR_DISABLE;
	cs_conf.pin_bit_mask = (1ULL << CS_PIN );
	cs_conf.mode = GPIO_MODE_OUTPUT;
	cs_conf.pull_up_en = (gpio_pullup_t) 0;
	gpio_config(&cs_conf);

	gpio_config_t sclk_conf;
	sclk_conf.pull_down_en = (gpio_pulldown_t) 0;
	sclk_conf.intr_type = (gpio_int_type_t) GPIO_INTR_DISABLE;
	sclk_conf.pin_bit_mask = (1ULL << SCLK_PIN );
	sclk_conf.mode = GPIO_MODE_OUTPUT;
	sclk_conf.pull_up_en = (gpio_pullup_t) 0;
	gpio_config(&sclk_conf);

	gpio_config_t miso_conf;
	miso_conf.pull_down_en = (gpio_pulldown_t) 0;
	miso_conf.intr_type = (gpio_int_type_t) GPIO_INTR_DISABLE;
	miso_conf.pin_bit_mask = (1ULL << MISO_PIN );
	miso_conf.mode = GPIO_MODE_OUTPUT;
	miso_conf.pull_up_en = (gpio_pullup_t) 0;
	gpio_config(&miso_conf);

	gpio_config_t mosi_conf;
	mosi_conf.pull_down_en = (gpio_pulldown_t) 0;
	mosi_conf.intr_type = (gpio_int_type_t) GPIO_INTR_DISABLE;
	mosi_conf.pin_bit_mask = (1ULL << MOSI_PIN );
	mosi_conf.mode = GPIO_MODE_INPUT;
	mosi_conf.pull_up_en = (gpio_pullup_t) 0;

	gpio_config(&mosi_conf);

//	gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
//	gpio_intr_enable(MOSI_PIN);
//	gpio_set_intr_type(MOSI_PIN, GPIO_INTR_NEGEDGE);
//	gpio_isr_handler_add(MOSI_PIN, task_isr, NULL);
}

/******************************************************************/
//                      CS5532 Initilization
// At least 15 SYNC1 followed by SYNC0
// Call subroutine CS5532_Reset() to reset CS5532
// Return 0x10 if reset is successful
/******************************************************************/
unsigned char CS5532_Init(void) {
	unsigned long resl;
	unsigned long auxl = 0;
	unsigned char resc;
	unsigned char i;
	unsigned char res = 1;

	setup_pins();

	End_CS_Line();

	vTaskDelay(5 / portTICK_PERIOD_MS);
	Inic_CS_Line();
	for (i = 0; i != 15; i++)
		resc = CS5532_BYTE(SYNC1);
	resc = CS5532_BYTE(SYNC0);
	End_CS_Line();
	vTaskDelay(5 / portTICK_PERIOD_MS);
	resl = CS5532_Reset();
	if (resl != CMD_Conf_RV_Active)
		res = 0;
	if (res == 1) {
////  Config
		auxl = CS5532_Conf_Reg() | CMD_Conf_VRef_10_25;
		resl = CS5532_CMD_REG(
		CMDL_Single_Access | CMDL_CH1 | CMDL_Write | CMDL_Config, auxl);
///// Chan_Setup Register
		auxl = CS5532_Channel_Setup_Regiter12();
		resl = CS5532_CMD_REG(
		CMDL_Single_Access | CMDL_CH1 | CMDL_Write | CMDL_Chan_Setup, auxl);
////
		CS5532_Auto_Gain_Calibration(1);
		CS5532_Auto_OffSet_Calibration(1);
///
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH1 | CMDL_Write | CMDL_Gain,
				vGainCH1);

	}
	return (res);
}

void read_channel(uint8_t channel) {
	read_channel_value(channel);

	vTaskDelay(pdMS_TO_TICKS(500));
}

// Ultima leitura valida e contador de falhas consecutivas por canal (0-3),
// usados quando o DRDY do CS5532/CS5534 estoura o timeout - em vez de
// devolver um valor inventado (-1), "congela" no ultimo dado confiavel
// daquele canal. O contador e exposto para quem chama decidir se desiste
// da cavidade apos falhas repetidas (ver get_channel_consecutive_timeouts).
static long last_valid_channel_value[4] = { 0, 0, 0, 0 };
static uint8_t channel_consecutive_timeouts[4] = { 0, 0, 0, 0 };

uint8_t get_channel_consecutive_timeouts(uint8_t channel) {
	if (channel > 3)
		return 0;
	return channel_consecutive_timeouts[channel];
}

void reset_channel_consecutive_timeouts(uint8_t channel) {
	if (channel > 3)
		return;
	channel_consecutive_timeouts[channel] = 0;
}

long read_channel_value(uint8_t channel) {
	uint8_t aux = 0;
	long auxl = 0;
	uint8_t res = 0;

	vCMD_Setup1_Channel = channel;
	vCMD_Setup2_Channel = channel;

	switch (channel) {
	case 0:
		vCMD_Setup1_Channel = 0;
		vCMD_Setup2_Channel = 0;
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH1 | CMDL_Write | CMDL_OffSet,
				vOffSetCH);
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH1 | CMDL_Write | CMDL_Gain,
				vGainCH1);
		break;
	case 1:
		vCMD_Setup1_Channel = 1;
		vCMD_Setup2_Channel = 1;
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH2 | CMDL_Write | CMDL_OffSet,
				vOffSetCH);
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH2 | CMDL_Write | CMDL_Gain,
				vGainCH2);
		break;
	case 2:
		vCMD_Setup1_Channel = 2;
		vCMD_Setup2_Channel = 2;
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH3 | CMDL_Write | CMDL_OffSet,
				vOffSetCH);
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH3 | CMDL_Write | CMDL_Gain,
				vGainCH3);
		break;
	case 3:
		vCMD_Setup1_Channel = 3;
		vCMD_Setup2_Channel = 3;
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH4 | CMDL_Write | CMDL_OffSet,
				vOffSetCH);
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH4 | CMDL_Write | CMDL_Gain,
				vGainCH4);

		break;
	}
	vTaskDelay(pdMS_TO_TICKS(50));

	for (uint8_t i = 0; i != 5; i++) {
		Inic_CS_Line();
		switch (channel) {
		case 0:
			CS5532_BYTE(CMDH_Channel_Setup_Pt1);
			break;
		case 1:
			CS5532_BYTE(CMDH_Channel_Setup_Pt3);
			break;
		case 2:
			CS5532_BYTE(CMDH_Channel_Setup_Pt5);
			break;
		case 3:
			CS5532_BYTE(CMDH_Channel_Setup_Pt7);
			break;
		}

		uint16_t drdy_wait_ms = 0;
		while (read_mosi_pin()) {
			vTaskDelay(pdMS_TO_TICKS(10));
			drdy_wait_ms += 10;
			if (drdy_wait_ms >= CS5532_DRDY_TIMEOUT_MS) {
				ESP_LOGE(TAG,
						"Timeout aguardando DRDY do CS5532/CS5534 (canal %d, leitura %d/5) - assumindo ultima leitura valida",
						channel + 1, i + 1);
				End_CS_Line();

				if (channel_consecutive_timeouts[channel] < 255)
					channel_consecutive_timeouts[channel]++;

				return last_valid_channel_value[channel];
			}
		}
		CS5532_BYTE(0x00);
		auxl = CS5532_LONG(0X00000000);
		aux = (((uint8_t) auxl) & 0x03) + 1;
		auxl = auxl >> 8;
		End_CS_Line();
		vTaskDelay(pdMS_TO_TICKS(10));
	}

	last_valid_channel_value[channel] = auxl;
	channel_consecutive_timeouts[channel] = 0;

//		vTaskDelay(pdMS_TO_TICKS(10));
//	}

	printf("Read CH%02d %d = %ld\n", channel + 1, aux, auxl);
#if CONFIG_ADC_DEBUG_SERIAL
	printf("[ADCDBG] chip=CS5534 ch=%u raw=%ld\n", channel, auxl);
#endif

	return auxl;
}

void setup_cs5534() {
	//CS5532_Init();
	uint8_t aux = 0;
	long auxl = 0;
	uint8_t res = 0;

	setup_pins();
	// reset spi
	Inic_CS_Line();
	CS5532_LONG(0XFFFFFFFF);
	CS5532_LONG(0XFFFFFFFF);
	CS5532_LONG(0XFFFFFFFF);
	CS5532_LONG(0XFFFFFFFE);
	End_CS_Line();
	vTaskDelay(pdMS_TO_TICKS(5));
	//// reset CS5534

	auxl = CS5532_CMD_REG(CMDL_Single_Access | CMDL_Write | CMDL_Config,
			CS5532_Conf_Reg() | CMD_Conf_RV_Active);
	vTaskDelay(pdMS_TO_TICKS(50));
	auxl = CS5532_CMD_REG(CMDL_Single_Access | CMDL_Read | CMDL_Config,
			CS5532_Conf_Reg());
	// check reset
	if ((auxl & CMD_Conf_RV_Active) != 0) {
		res = 1;
		printf("CS5534 Reset Done");
	} else {
		res = 0;
		printf("CS5534 Reset Fail");
	}
	if (res == 1) { // reset done
		// setup

		vCMD_Setup1_Channel = 0;
		vCMD_Setup2_Channel = 0;
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH1 | CMDL_Write | CMDL_Config,
				CS5532_Conf_Reg() | CMD_Conf_VRef_10_25);
		CS5532_CMD_REG(
				CMDL_Single_Access | CMDL_CH1 | CMDL_Write | CMDL_Chan_Setup,
				CS5532_Channel_Setup_Regiter12());

		vCMD_Setup1_Channel = 1;
		vCMD_Setup2_Channel = 1;
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH2 | CMDL_Write | CMDL_Config,
				CS5532_Conf_Reg() | CMD_Conf_VRef_10_25);
		CS5532_CMD_REG(
				CMDL_Single_Access | CMDL_CH2 | CMDL_Write | CMDL_Chan_Setup,
				CS5532_Channel_Setup_Regiter12());

		vCMD_Setup1_Channel = 2;
		vCMD_Setup2_Channel = 2;
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH3 | CMDL_Write | CMDL_Config,
				CS5532_Conf_Reg() | CMD_Conf_VRef_10_25);
		CS5532_CMD_REG(
				CMDL_Single_Access | CMDL_CH3 | CMDL_Write | CMDL_Chan_Setup,
				CS5532_Channel_Setup_Regiter12());

		vCMD_Setup1_Channel = 3;
		vCMD_Setup2_Channel = 3;
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH4 | CMDL_Write | CMDL_Config,
				CS5532_Conf_Reg() | CMD_Conf_VRef_10_25);
		CS5532_CMD_REG(
				CMDL_Single_Access | CMDL_CH4 | CMDL_Write | CMDL_Chan_Setup,
				CS5532_Channel_Setup_Regiter12());

		//Inic_CS_Line();
		//CS5532_Auto_Gain_Calibration(1);
		//End_CS_Line();
		//vTaskDelay(pdMS_TO_TICKS(5));

		//Inic_CS_Line();
		//CS5532_Auto_OffSet_Calibration(1);
		//End_CS_Line();
		//vTaskDelay(pdMS_TO_TICKS(5));

		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH1 | CMDL_Write | CMDL_OffSet,
				vOffSetCH);
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH1 | CMDL_Write | CMDL_Gain,
				vGainCH1);

		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH2 | CMDL_Write | CMDL_OffSet,
				vOffSetCH);
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH2 | CMDL_Write | CMDL_Gain,
				vGainCH2);

		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH3 | CMDL_Write | CMDL_OffSet,
				vOffSetCH);
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH3 | CMDL_Write | CMDL_Gain,
				vGainCH3);

		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH4 | CMDL_Write | CMDL_OffSet,
				vOffSetCH);
		CS5532_CMD_REG(CMDL_Single_Access | CMDL_CH4 | CMDL_Write | CMDL_Gain,
				vGainCH4);

		vCMD_Setup1_Channel = 0;
		vCMD_Setup2_Channel = 0;
	}
}

void light_sensor_task(void *pvParameter) {
	// end setup
	// read
	while (1) {
		read_channel(0);
		read_channel(1);
		read_channel(2);
		read_channel(3);

		vTaskDelay(pdMS_TO_TICKS(500));
		printf("---------------------\r\n");
	}

}

void light_sensor_setup() {
	// Setup do Sensor
	setup_cs5534();
}

void light_sensor_main() {
	xTaskCreate(light_sensor_task, "light_sensor", configMINIMAL_STACK_SIZE * 4,
	NULL, 5, NULL);
}

#endif // CONFIG_ADC_CHIP_CS5534
