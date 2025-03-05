/**
 * @file BG77.c
 * @author EM2
 * @date 2025-02-06
 */
#include "../BG77/BG77.h"

//-----------------------------------Declaracion de tipos de variable, variables con alcance local-----------------
/**
 * @brief Este tipo de variable hace referencia a los valores de estado de modo transparente. 
 * Su proposito es tener valores estandarizados para conocer el estado del modo transparente
 * (activado o desactivado). 
 * 
 */
typedef enum bg_statusTM_t bg_statusTM_t;

/**
 * @brief Este tipo de variable contiene el estado del modo transparente y el ultimo numero de 
 * conexion (connectID) en modo transparente.
 * 
 * Su proposito es almacenar y proporcionar la informacion necesaria del modo transparente 
 * (estado y numero de conexion)
 */
typedef struct bg_infoTM_t bg_infoTM_t;
//-----------------------------------Declaracion de tipos de variable, variables con alcance local end-------------


//-----------------------------------Declaracion funciones static----------------------
/**
 * @brief Esta funcion realiza la configuracion del modulo de comunicacion.
 * (Formatos de respuesta y configurar URC que se contemplan).
 * 
 * @return bg_err_t 
 */
static bg_err_t bg_config_module(void);

/**
 * @brief Limpia el contador de segundos de la libreria y activa el conteo.
 * 
 */
static void bg_start_timeout(void);

/**
 * @brief Detiene el conteo y devuelve el ultimo valor que se conto.
 * 
 * @return uint32_t es el ultimo valor que se conto.
 */
static uint32_t bg_stop_timeout(void);

/**
 * @brief Funcion que detecta si ocurrio un URC
 * 
 * @param buff Puntero a funcion que contiene el mensaje donde se dertecta el URC.
 * @param len Tamaño en bytes del mensaje recibido en buff.
 */
static void bg_detect_urc(uint8_t *buff, uint16_t len);

/**
 * @brief Obtiene la instancia del Singleton del estado de modo transparente.
 * 
 * @return bg_infoTM_t* Devuelve la instancia que almacena la informacion del estado
 * de modo transparente.
 */
static bg_infoTM_t *bg_getter_instance_TM(void);

/**
 * @brief Es el getter para obtener el valor actual del estado de modo transparente.
 * 
 * @return bg_infoTM_t Devuelve el valor actual del estado de modo transparente.
 */
static bg_infoTM_t bg_getter_transparentMode(void);

/**
 * @brief Es el setter para establecer el valor actual del estado de modo transparente.
 * 
 * @param value Es el valor que se va a establecer o escribir en la instancia.
 */
static void bg_setter_transparentMode(bg_infoTM_t value);
//-----------------------------------Declaracion funciones static end-----------------


//-----------------------------------Definicion de tipos de variable, variables con alcance local-----------------
enum bg_statusTM_t
{
	BG_TM_INACTIVE,
	BG_TM_ACTIVE
};

struct bg_infoTM_t
{
	bg_statusTM_t statusTM;
	bg_noCarrierTM_t statusNoCarrier;
	uint8_t connectID;
};
//-----------------------------------Definicion de tipos de variable, variables con alcance local end-------------


//-----------------------------------Flags--------------------------------------------
#define flg_uart_bg flag.f0 //1: se recibio un mensjae por interrupcion de uart.
#define flg_tout_bg flag.f1 //1: bandera que indica que el tiempo de espera de respuesta de comando fue superado
#define NOMBRE_USR_4 flag.f3 
#define NOMBRE_USR_5 flag.f4
#define NOMBRE_USR_6 flag.f5
#define NOMBRE_USR_7 flag.f6
#define NOMBRE_USR_8 flag.f7
static flags_t flag = {0}; //banderas de 1 bit
//-----------------------------------Flags end----------------------------------------


//-----------------------------------Buffers------------------------------------------
//uBg es el buffer el cual se reciben las respuestas del modulo de comunicaciones.
//uBgUrc es el buffer el cual se utiliza para hacer el tratado de URC.
static uartBuff_t uBg = {.buff = {'\0'}, .len = 0},\
		   uBgUrc = {.buff = {'\0'}, .len = 0};
//-----------------------------------Buffers end--------------------------------------


//-----------------------------------Counters-----------------------------------------
static volatile uint32_t count_interrp;		//cuenta 1000 interrupciones para generar 1s
static volatile uint32_t count_sec_bg;		//contador de segundos para espera de timeout
static volatile uint32_t count_sec_bg_ok;	//contador de segundos para timeout en respuesta esperada del modulo bg
//-----------------------------------Counters end-------------------------------------


//----------------------------------Queue--------------------------------------------
Cola_t bgUrcQueue; //cola para eventos URC detectados
//----------------------------------Queue end----------------------------------------


//-----------------------------------BSP----------------------------------------------
//puntero a funcion para funcion de transmision por UART en puerto BG
static uartFun_t bgUartTx;
//puntero a funcion para escribir estado en pin de salida gpio (BG_PWRKEY_PIN, BG_VBAT_PIN, BG_RESET_PIN, BG_PON_TRIG_PIN) 
static gpioWriteFun_t bgGpioWrite;
//puntero a funcion para generar delays en ms
static delayFun_t bgDelay;
//puntero a funcion para reiniciar al MCU
static resetFun_t bgResetMCU;

void bg_set_bsp(bg_bspFun_t bspFun)
{
	bgUartTx = bspFun.uartTx;
	bgGpioWrite = bspFun.gpioWrite;
	bgDelay = bspFun.msDelay;
	bgResetMCU = bspFun.resetMCU;
	flg_tout_bg = 1;
	flg_uart_bg = 0;
}
//-----------------------------------BSP end-----------------------------------


//---------------------------Callbacks MCU para LIB----------------------------
void bg_callback_ms(void)
{
	count_interrp = (count_interrp + 1)%1001;

	if (!flg_tout_bg)
			count_sec_bg = (count_sec_bg + count_interrp/1000)%MAX_SEC;
}

void bg_uartCallback(uint8_t *buff, uint16_t nBytes)
{
	uBg.len = uBgUrc.len = nBytes;

	memcpy(uBg.buff, buff, uBg.len);
	memcpy(uBgUrc.buff, buff, uBgUrc.len);

	bg_infoTM_t infoTM = bg_getter_transparentMode();

	if(infoTM.statusTM == BG_TM_ACTIVE)
	{
		if(strstr(uBgUrc.buff, "NO CARRIER"))
		{
			bg_callback_closed_TM();
			bg_infoTM_t valueTM = {.statusTM = BG_TM_INACTIVE, .statusNoCarrier = BG_TM_NO_CARRIER_SET,\
				.connectID = infoTM.connectID};
			bg_setter_transparentMode(valueTM);
			LOG_BG(LE, "*NO CARRIER URC*\n");

			urcRawData_t tmExitNC = {.buff = "no carrier", .len = 10, .type = BG_URC_NO_CARRIER};
			bg_queue_put(tmExitNC);
		}
		else
			bg_callback_receive_TM(uBgUrc.buff, uBg.len);

		memset(uBgUrc.buff, '\0', sizeof(uBgUrc.buff));
	}

	else
	{
		if(strstr(uBgUrc.buff, "NO CARRIER"))
		{
			bg_callback_closed_TM();
			bg_infoTM_t valueTM = {.statusTM = BG_TM_INACTIVE, .statusNoCarrier = BG_TM_NO_CARRIER_SET,\
				.connectID = infoTM.connectID};
			bg_setter_transparentMode(valueTM);
			LOG_BG(LE, "-NO CARRIER URC-\n");

			urcRawData_t tmExitNC = {.buff = "no carrier", .len = 10, .type = BG_URC_NO_CARRIER};
			bg_queue_put(tmExitNC);
		}

		bg_detect_urc(uBgUrc.buff, uBgUrc.len);
	}	

	LOG_BG(LD, "uBglen: %ld\n", nBytes);
	flg_uart_bg = 1;
}

void bg_mainRICallback(mainRiEdge_t edge)
{
	if(edge == MAIN_RI_EDGE_FALLING)
	{
		bg_infoTM_t infoTM = bg_getter_transparentMode();
		
		if(infoTM.statusTM == BG_TM_INACTIVE) return;

		bg_infoTM_t valueTM = {.statusTM = BG_TM_INACTIVE, .statusNoCarrier = infoTM.statusNoCarrier,\
			.connectID = infoTM.connectID};
		bg_setter_transparentMode(valueTM);

		urcRawData_t tmExit = {.buff = "mainRI", .len = 7, .type = BG_URC_EXIT_TM};
		bg_queue_put(tmExit);
	}

	else if(edge == MAIN_RI_EDGE_RISING)
	{
		
	}
}

static void bg_detect_urc(uint8_t *buff, uint16_t len)
{
	uint8_t *urcStr[] = {"incoming", "recv", "closed", "incoming full", "pdpdeact"};

	bg_urcType_t urc, urcType[] = {BG_URC_INCOMING, BG_URC_RECV, BG_URC_CLOSED,\
		BG_URC_INCOMING_FULL, BG_URC_PDP_DEACT};
	
	for(int i = 0; i < sizeof(urcStr)/sizeof(urcStr[0]) ; i++)
	{
		if(strstr(buff, urcStr[i]))
		{	
			LOG_BG(LE, "URC MATCH [%d]\n", i);

			uint8_t *ptrParse = strstr(buff, urcStr[i]);
			while(ptrParse != NULL)
			{
				urcRawData_t urcDetected = {.buff = '\0', .len = len, .type = urcType[i]};
				strcpy(urcDetected.buff, ptrParse);
				bg_callback_urcDetected(urcDetected);
				ptrParse++;
				ptrParse = strstr(ptrParse, urcStr[i]);
			}
		}
	}
	memset(uBgUrc.buff, '\0', sizeof(uBgUrc.buff));
}

void bg_handle_urc(void)
{
	if(bg_queue_is_empty()) 
	{
		LOG_BG(LD, "queue is empty\n");

		bg_infoTM_t infoTM = bg_getter_transparentMode();

		if(infoTM.statusTM == BG_TM_INACTIVE)
		{
			bg_infoTM_t infoTM = bg_getter_transparentMode();
			bg_callback_TM_Inactive(infoTM.connectID, infoTM.statusNoCarrier);
		}

		return;
	}

	bg_infoTM_t infoTM = bg_getter_transparentMode();
	if(infoTM.statusTM == BG_TM_ACTIVE)
		if(bg_exit_transparent_mode() == BG_OK_EXIT_TRANSPARENT_MODE)
			LOG_BG(LE, "*Salida de TM Exitosa*\n");

	urcRawData_t urcPop;
	bg_queue_pop(&urcPop);

	LOG_BG(LE,"\nlen:%ld\ntype:%d\nbuff: %s", urcPop.len, urcPop.type, urcPop.buff);
	urcInfoData_t infoUrc;
	switch(urcPop.type)
	{
		case BG_URC_EXIT_TM:
			if(bg_exit_transparent_mode() == BG_OK_EXIT_TRANSPARENT_MODE)
			{
				LOG_BG(LE, "Salida de TM Exitosa\n");
			}
			else
			{	
				bg_infoTM_t infoTM = bg_getter_transparentMode();

				if(infoTM.statusNoCarrier == BG_TM_NO_CARRIER_SET)
					LOG_BG(LE, "Salida de TM por desconexion NO CARRIER\n");
				// else
				// {
				// 	LOG_BG(LE, "Salida de TM NO Exitosa\n");
				// 	bg_infoTM_t valueTM = {.statusTM = BG_TM_ACTIVE, .statusNoCarrier = infoTM.statusNoCarrier,\
				// 		 .connectID = infoTM.connectID};
				// 	bg_setter_transparentMode(valueTM);
				// }
			}
		break;

		case BG_URC_CLOSED:
			COPY_PARSE_STR(infoUrc.buff, sizeof(infoUrc.buff), urcPop.buff, ',', '\n');
			infoUrc.connectID = atoi(infoUrc.buff);
			infoUrc.type = urcPop.type;
			bg_urc_parsed_callback(infoUrc);
		break;

		case BG_URC_INCOMING:
			COPY_PARSE_STR(infoUrc.buff, sizeof(infoUrc.buff), urcPop.buff, ',', ',');
			infoUrc.connectID = atoi(infoUrc.buff);

			uint8_t *ptrAux = strchr(urcPop.buff, ',');

			if(ptrAux == NULL)
				return;	

			ptrAux = strchr(++ptrAux, ',');

			COPY_PARSE_STR(infoUrc.buff, sizeof(infoUrc.buff), ptrAux, ',', ',');
			infoUrc.serverID = atoi(infoUrc.buff);

			infoUrc.type = urcPop.type;
			bg_urc_parsed_callback(infoUrc);
		break;

		case BG_URC_RECV:
			COPY_PARSE_STR(infoUrc.buff, sizeof(infoUrc.buff), urcPop.buff, ',', '\n');
			infoUrc.connectID = atoi(infoUrc.buff);
			urcInfoData_t aux = infoUrc;
			memset(infoUrc.buff, '\0', sizeof(infoUrc.buff));

			if(bg_receive_buffAMode(infoUrc.connectID, infoUrc.buff, &infoUrc.len) != BG_OK_RECEIVE)
				return;

			infoUrc.type = urcPop.type;

			if(infoUrc.len > 1024) infoUrc.len = 1024; 

			bg_urc_parsed_callback(infoUrc);
		break;

		case BG_URC_INCOMING_FULL:
			LOG_BG(LE, "URC INCOMMING_FULL\n");
			infoUrc.type = urcPop.type;
			bg_urc_parsed_callback(infoUrc);
			//solo notificar evento
		break;

		case BG_URC_PDP_DEACT:
			LOG_BG(LE, "URC PDP_DEACT\n");
			COPY_PARSE_STR(infoUrc.buff, sizeof(infoUrc.buff), urcPop.buff, ',', '\n');
			infoUrc.contextID = atoi(infoUrc.buff);
			infoUrc.type = urcPop.type;
			bg_urc_parsed_callback(infoUrc);
		break;

		case BG_URC_NO_CARRIER:
			{
				bg_infoTM_t infoTM = bg_getter_transparentMode();
				bg_close_sckt(infoTM.connectID);
			}
		break;
	}
}
//---------------------------Callbacks MCU para LIB end----------------------------


//--------------------------Funciones para conteo de tiempo------------------------
static void bg_start_timeout(void)
{
	flg_tout_bg = 0;
	count_sec_bg = 0;
}

static uint32_t bg_stop_timeout(void)
{
	flg_tout_bg = 1;
	return count_sec_bg;
}
//--------------------------Funciones para conteo de tiempo------------------------


//------------------Funciones basicas y de configuracion de modulo------------------
bg_err_t bg_send(uint32_t timeout, uint8_t enablePrint, const char *fmt, ...) {
	int format_result = -1, err = -1;
	char tmp[512] = {'\0'};
	static uint32_t seq = 0;
	uint8_t *frame[] = {"************************\n", "~~~~~~~~~~~~~~~~~~~~~~~\n"};

	va_list args;
	va_start(args, fmt);
	format_result = vsnprintf(tmp, sizeof tmp, fmt, args);
	va_end(args);

	if (0 > format_result)
		return err;

	else
	{
		tmp[format_result] = '\r';
		tmp[format_result + 1] = '\n';

		flg_uart_bg = 0;
		memset(uBg.buff, '\0', sizeof(uBg.buff));

		LOG_BG(enablePrint, "%s", (seq%2) ? frame[0] : frame[1]);
		LOG_BG(enablePrint, "MCU[%ld] > \n%s\n", seq, tmp);
		if(bgUartTx(tmp, strlen(tmp)))
		{
			LOG_BG(enablePrint, "[BG_ERR] ERROR MCU TX UART\n");
			return BG_ERR_MCU_TX_UART;
		}

		bg_start_timeout();
		while(!flg_uart_bg && count_sec_bg < timeout)
			__asm__("nop");

		if(timeout <= bg_stop_timeout())
		{
			LOG_BG(enablePrint, "[BG_ERR] TIMEOUT RESPUESTA BG\n");
			LOG_BG(enablePrint, "%s\n", (seq%2) ? frame[0] : frame[1]);
			seq++;
			return BG_ERR_TIMEOUT_ANS;
		}


		LOG_BG(enablePrint, "BG[%ld] > \n", seq);
		LOG_BG(enablePrint, "%s\nlen: %ld\n", &uBg.buff[2], uBg.len);
		LOG_BG(enablePrint, "%s\n", (seq%2) ? frame[0] : frame[1]);

		seq++;
		memset(uBgUrc.buff, '\0', sizeof(uBgUrc.buff));
		return BG_OK;
	}

}

bg_err_t bg_power_on(void)
{
	bg_err_t err = BG_OK;
	bgGpioWrite(BG_PWRKEY_PIN, 1);
	bgGpioWrite(BG_RESET_PIN, 1);
	bgDelay(2000);
	bgGpioWrite(BG_PWRKEY_PIN, 0);
	bgGpioWrite(BG_RESET_PIN, 0);
	bgDelay(6000);

	err = bg_send(5, LE, "AT");
	CHECK_BG_ERR(err);

	CHECK_DESIRED_ANSW(uBg.buff, "OK", BG_TIMEOUT_ANSW_OK);
	
	return BG_OK;
}

bg_err_t bg_power_off(void)
{
	bg_err_t err = BG_OK;

	bgDelay(500); //1000 1s
	
	err = bg_send(5, LE, "AT+QPOWD");
	CHECK_BG_ERR(err);

	CHECK_POWDWN_ANSW(uBg.buff, BG_TIMEOUT_ANSW_OK);

	bgDelay(3000); //10000 10s
	return BG_OK;
}

static bg_err_t bg_config_module(void){

	static uint8_t configDone = 0;

	if(configDone)
		return BG_OK;

	uint32_t timeout[] = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
	uint8_t *atCmd[] = {"ATE0",//Deshabilita el modo echo del modulo 
		"AT+QURCCFG=\"urcport\",\"uart1\"",//Configura UART1 para salida de URC
		"AT+QCFG=\"risignaltype\",\"respective\"",//Se activa señal en pin MAIN_RI
		"AT+QCFG=\"urc/ri/ring\",\"off\"",//Se desactiva URC de llamadas
		"AT+QCFG=\"urc/ri/smsincoming\",\"off\"",//Se desactiva URC SMS
		"AT+QCFG=\"urc/ri/other\",\"pulse\",80,1",//Configura un unico pulso de MAIN_RI a 80ms
		"AT+QCFG=\"urc/delay\",100",//No hay retardo (0 en vez de 100 )despues del pulso para salida de URC en UART
		"AT+CEREG=2",//Activa URC de estado de registro en red y ubicacion
		"AT+QCFG=\"nwscanseq\",020301",//Especifica secuencia de busqueda LTE
		"AT+QCFG=\"iotopmode\",2,1",//Configura la categoria de busqueda
		"AT+QCFG=\"band\", 0,800000A,1",//Habilita busqueda de bandas B2, B4 y B28
		"AT+COPS=3, 2",// Configura respuesta de formato de COPS a numerica
		"AT&W0"//Guarda la configuracion
	};

	bg_err_t err = BG_OK;

	for(int i = 0; i < sizeof(atCmd) / sizeof(atCmd[0]); i++)
	{
		err = bg_send(timeout[i], LE, atCmd[i]);
		CHECK_BG_ERR(err);
	}

	configDone = 1;

	return err;
}

bg_err_t bg_init_module(void)
{	
	static uint8_t flgInit = 0;
	
	if(flgInit) return BG_OK;

	while(bg_power_on() != BG_OK)
	{
		uint8_t attmp = 0;
		printf("POWER ON... ATTEMP: %d\n", attmp);
		if(attmp++ > 2)
		  bgResetMCU();
	}

	bg_power_off();

	while(bg_power_on() != BG_OK)
	{
		uint8_t attmp = 0;
		printf("POWER ON... ATTEMP: %d\n", attmp);
		if(attmp++ > 2)
		  bgResetMCU();
	}

	printf("|--- POWER ON... OK ---|\n\n");

	create_queue(&bgUrcQueue);

	bg_err_t err = bg_config_module();
	CHECK_BG_ERR(err);

	return bg_send(10, LE, "AT+QRFTESTMODE=0");
}
//------------------Funciones basicas y de configuracion de modulo------------------


//---------------------------------Funciones de consulta-----------------------------
bg_err_t bg_data_module(void)
{
	uint8_t FW[50] = {'\0'}, ICCID[50] = {'\0'}, IMEI[50] = {'\0'}, *parsePtr;
	uint8_t *dataPtr[] = {FW, ICCID, IMEI};	
	size_t sizeBuff[] = {sizeof(FW), sizeof(ICCID), sizeof(IMEI)};

	uint32_t timeout[] = {5, 5, 5};
	uint8_t *atCmd[] = {"AT+GMR",//Consulta el Fw del modulo
		"AT+QCCID",//Consulta del ICCID del SIM
		"AT+GSN"//Consulta IMEI
	};

	uint8_t chrDelim[] = {'\n', ' ', '\n'};

	bg_err_t err = BG_OK;

	for(int i = 0; i < sizeof(atCmd) / sizeof(atCmd[0]); i++)
	{
		err = bg_send(timeout[i], LE, atCmd[i]);
		CHECK_BG_ERR(err);

		COPY_PARSE_STR(dataPtr[i], sizeBuff[i], uBg.buff, chrDelim[i], '\n');
	}

	LOG_BG(LE, "FW: %s\nICCID: %s\nIMEI: %s\n", FW, ICCID, IMEI);

	return err;
}

bg_err_t bg_check_sim(void)
{
	bg_err_t err;

	for(size_t attmp = 0; attmp <= 3; attmp++)
	{
		if(attmp >= 3) return BG_ERR_SIM_NO_OK;

		err = bg_send(5, LE, "AT+CPIN?");
		CHECK_BG_ERR(err);

		if(strstr(uBg.buff, "READY")) break;
	}

	return BG_OK_SIM;
}

bg_err_t bg_check_attach(void)
{
	bg_err_t err = bg_send(10, LE, "AT+CEREG?");
	CHECK_BG_ERR(err);

	uint8_t *parsePtr = strchr(uBg.buff, ',');

	if(parsePtr == NULL) return BG_ERR_PARSE;

	if(*(parsePtr + 1) == '5' || *(parsePtr + 1) == '1') return BG_OK_ATTACH; //registrado 1 o 5 +CEREG: 0,5,

	return BG_ERR_ATTACH_NO_OK;
}

bg_err_t bg_query_cops(void)
{
	bg_err_t err = bg_send(10, LE, "AT+COPS?");
	CHECK_BG_ERR(err);

	uint8_t *parsePtr = uBg.buff;

	if(strchr(parsePtr, ',') == NULL) return BG_ERR_NO_OPER;

	uint8_t mode[10] = {'\0'}, oper[20] = {'\0'}, accessTech[10] = {'\0'};
	uint8_t *dataBuff[] = {mode, NULL, oper, accessTech};
	size_t sizeBuff[] = {sizeof(mode), 0, sizeof(oper), sizeof(accessTech)};
	uint8_t chrDelim1[] = {' ', ',', ',', ','};
	uint8_t chrDelim2[] = {',', ',', ',', '\n'};

	for(int i = 0; i < sizeof(dataBuff) / sizeof(dataBuff[0]); i++)
	{
		parsePtr = strchr(parsePtr, chrDelim1[i]);
		if(parsePtr)
		{
			COPY_PARSE_STR(dataBuff[i], sizeBuff[i], parsePtr, chrDelim1[i], chrDelim2[i]);
			parsePtr++;
		}
	}
	
	uint8_t *strMode[] = {"Auto", "Manual", "Desregistrado Manual", "", "Manual-Auto"},\
			*strAccss[] = {"GSM", "", "", "", "", "", "", "E-ULTRAN", "eMTC", "MB-IoT"};

	printf("mode: %s\noper: %s\naccss: %s\n", strMode[atoi(mode)], oper,\
			strAccss[atoi(accessTech)]);

	return BG_OK;
}

bg_err_t bg_query_conf_pdp(uint8_t contextID)
{
	if(contextID > BG_CONTEXT_ID_MAX || contextID < BG_CONTEXT_ID_MIN)
		return BG_ERR_CTXT_ID_UNSUPPORTED;

	bg_err_t err = bg_send(40, LD, "AT+QICSGP=%d", contextID);
	CHECK_BG_ERR(err);

	uint8_t *parsePtr = uBg.buff;

	if(strchr(parsePtr, ',') == NULL) return BG_ERR_NO_OPER;

	uint8_t apn[256] = {'\0'}, usr[100] = {'\0'}, passw[100] = {'\0'}, auth[100] = {'\0'};
	uint8_t *dataBuff[] = {apn, usr, passw, auth};
	size_t sizeBuff[] = {sizeof(apn), sizeof(usr), sizeof(passw), sizeof(auth)};
	uint8_t chrDelim1[] = {',', ',', ',', ','};
	uint8_t chrDelim2[] = {',', ',', ',', '\n'};

	for(int i = 0; i < sizeof(dataBuff) / sizeof(dataBuff[0]); i++)
	{
		parsePtr = strchr(parsePtr, ',');
		
		if(parsePtr)
		{
			COPY_PARSE_STR(dataBuff[i], sizeBuff[i], parsePtr, chrDelim1[i], chrDelim2[i]);
			parsePtr++;
		}
	}
	
	LOG_BG(LE, "APN: %s\nUser: %s\nPassword: %s\nAuth: %s\n", apn, usr, passw, auth);

	return BG_OK;
}

bg_err_t bg_check_pdp(uint8_t ctxtID)
{
	if(ctxtID > BG_CONTEXT_ID_MAX || ctxtID < BG_CONTEXT_ID_MIN) return BG_ERR_CTXT_ID_UNSUPPORTED;
	bg_err_t err = bg_send(10, LD, "AT+QIACT?");
	CHECK_BG_ERR(err);
	uint8_t *parsePtr = uBg.buff;
	uint8_t *buffAux = NULL;
	sprintf(buffAux, "+QIACT: %d", ctxtID); 
	if(!strstr(parsePtr, buffAux)) return BG_OK_PDP_DEACT;


	uint8_t ctxState[20] = {'\0'}, ctxtType[20] = {'\0'}, ip[30] = {'\0'};
	uint8_t *dataBuff[] = {ctxState, ctxtType, ip};
	size_t sizeBuff[] = {sizeof(ctxState), sizeof(ctxtType), sizeof(ip)};
	uint8_t chrDelim1[] = {',', ',', ','};
	uint8_t chrDelim2[] = {',', ',', '\n'};

	for(int i = 0; i < sizeof(dataBuff) / sizeof(dataBuff[0]); i++)
	{
		parsePtr = strchr(parsePtr, ',');
		
		if(parsePtr)
		{
			COPY_PARSE_STR(dataBuff[i], sizeBuff[i], parsePtr, chrDelim1[i], chrDelim2[i]);
			parsePtr++;
		}
	}

	uint8_t *strCtxtState[] = {"Desactivado", "Activado"},\
		*strCtxtType[] = {"", "IPv4", "IPv6", "IPv4v6"};

	LOG_BG(LE, "ctxState: %s\nctxtType: %s\nip: %s\n", strCtxtState[atoi(ctxState)], strCtxtType[atoi(ctxtType)], ip);

	if(!strchr(ctxState, '1')) return BG_OK_PDP_DEACT;

	return BG_OK_PDP_ACT;
}

bg_err_t bg_check_sckt(uint8_t connectID)
{
	if(connectID > BG_CONNECT_ID_MAX) return BG_ERR_CONNECT_ID_UNSUPORTED;
	bg_err_t err = bg_send(10, LE, "AT+QISTATE=1,%d", connectID);
	CHECK_BG_ERR(err);

	uint8_t strConnectID[10] = {'\0'};
	sprintf(strConnectID, " %d", connectID);
	
	uint8_t *parsePtr = uBg.buff;

	if(!strstr(parsePtr, strConnectID)) return BG_OK_CONNECT_ID_CLOSED;//BG_ERR_CONNECT_ID_NOT_USED;

	//+QISTATE: <connectID>,<service_type>,<IP_address>,<remote_port>,<local_port>,<socket_state>,<contextID>,<serverID>,<access_mode>,<AT_port>

	uint8_t serviceType[20] = {'\0'}, ip[30] = {'\0'}, remotePort[10] = {'\0'}, localPort[10] = {'\0'},\
			scktState[10] = {'\0'};
	uint8_t *dataBuff[] = {serviceType, ip, remotePort, localPort, scktState};
	size_t sizeBuff[] = {sizeof(serviceType), sizeof(ip), sizeof(remotePort),\
			sizeof(localPort), sizeof(scktState)};
	uint8_t chrDelim1[] = {',', ',', ',', ',',','};
	uint8_t chrDelim2[] = {',', ',', ',', ',',','};

	for(int i = 0; i < sizeof(dataBuff) / sizeof(dataBuff[0]); i++)
	{
		parsePtr = strchr(parsePtr, ',');
		
		if(parsePtr)
		{
			COPY_PARSE_STR(dataBuff[i], sizeBuff[i], parsePtr, chrDelim1[i], chrDelim2[i]);
			parsePtr++;
		}
	}

	uint8_t *strScktState[] = {"Inicial", "Abriendo", "Conectado", "Escuchando",\
			"Cerrando"};
	printf("serviceType: %s\nip: %s\nremotePort: %s\nlocalPort: %s\nscktState: %s\n", serviceType,\
		ip, remotePort, localPort, strScktState[atoi(scktState)]);

	if(strchr(scktState, '4') || strchr(scktState, '0')) return BG_OK_CONNECT_ID_CLOSED; 

	return BG_OK_CONNECT_ID_OPENNED;
}

bg_err_t bg_query_signal(void)
{
	bg_err_t err = bg_send(10, LE, "AT+QCSQ");
	CHECK_BG_ERR(err);

	uint8_t *parsePtr = uBg.buff;

	uint8_t sysMode[20] = {'\0'}, rssi[30] = {'\0'}, rsrp[10] = {'\0'}, sinr[10] = {'\0'},\
			rsrq[10] = {'\0'};
	uint8_t *dataBuff[] = {sysMode, rssi, rsrp, sinr, rsrq};
	size_t sizeBuff[] = {sizeof(sysMode), sizeof(rssi), sizeof(rsrp),\
			sizeof(sinr), sizeof(rsrq)};
	uint8_t chrDelim1[] = {' ', ',', ',', ',',','};
	uint8_t chrDelim2[] = {',', ',', ',', ',','\n'};

	for(int i = 0; i < sizeof(dataBuff) / sizeof(dataBuff[0]); i++)
	{
		parsePtr = strchr(parsePtr, chrDelim1[i]);
		
		if(parsePtr)
		{
			COPY_PARSE_STR(dataBuff[i], sizeBuff[i], parsePtr, chrDelim1[i], chrDelim2[i]);
			parsePtr++;
		}
	}

	int rssi_int = atoi(rssi);
	int rsrp_int = atoi(rsrp);

	float sinr_f = (float) atoi(sinr);
	sinr_f /= 2;
	sinr_f -= 23.5;
	int sinr_int = sinr_f;

	int rsrq_int = atoi(rsrq);


	printf("SysMode: %s\nRSSI: %ld\nRSRP: %ld\nSINR: %ld\nRSRQ: %ld\n", sysMode,\
		rssi_int, rsrp_int, sinr_int, rsrq_int);

	//if(rsrp_int >= -115 && rsrq_int >= -15 && sinr_int >= 0)
	if(rsrp_int >= -115 && sinr_int >= 0)
		return BG_OK_SIGNAL;

	return BG_ERR_SIGNAL;
}
//---------------------------------Funciones de consulta end-------------------------


//-------------------------Funciones de Attach y activacion PDP----------------------
bg_err_t bg_attach(uint8_t *operator)
{
	bg_err_t err = BG_ERR_ATTACH_NO_OK;
	for(int i = 0; i <= 3; i++)
	{
		if(i == 3) return err;
  
		if((err = bg_send(180, LE, "AT+COPS=4,2,\"%s\",8", operator)) != BG_OK) continue;//intenta registrarse en la red de un operador para EG915ULA el Act (access technology es 7 E-ULTRAN)
		
		if(bg_check_attach() == BG_OK_ATTACH) break;

		err = BG_ERR_ATTACH_NO_OK;
	}

	return BG_OK_ATTACH;
}

bg_err_t bg_conf_pdp(bg_ctxPdp_t contextPDP)
{
	if(contextPDP.ctxtID > BG_CONTEXT_ID_MAX || contextPDP.ctxtID < BG_CONTEXT_ID_MIN)
	 return BG_ERR_CTXT_ID_UNSUPPORTED;

	bg_err_t err = bg_send(40, LE, "AT+QICSGP=%d,%d,\"%s\",\"%s\",\"%s\",%d",contextPDP.ctxtID,\
		contextPDP.contextType, contextPDP.apn, contextPDP.usr, contextPDP.psw, contextPDP.auth);

	CHECK_BG_ERR(err);

	if(strstr(uBg.buff, "OK")) return BG_OK_CONF_PDP;

	return BG_ERR_CONF_PDP;
}

bg_err_t bg_pdp_activation(uint8_t ctxtID, bg_actPdp_t act)
{
	if(ctxtID > BG_CONTEXT_ID_MAX || ctxtID < BG_CONTEXT_ID_MIN) 
	return BG_ERR_CTXT_ID_UNSUPPORTED;

	if(act > BG_PDP_ACT_UNSUPORTED) return BG_ERR_ACT_PDP;

	uint8_t *atCmd[] = {"AT+QIACT=", "AT+QIDEACT="};

	bg_err_t err = bg_send(30, LE, "%s%d", atCmd[act], ctxtID);
	CHECK_BG_ERR(err);

	return bg_check_pdp(ctxtID);
}
//-------------------------Funciones de Attach y activacion PDP end------------------


//------------------------Funciones de apertura de socket---------------------------
bg_err_t bg_open_sckt(bgSckt_t sckt)
{
	bg_err_t err;
	uint8_t *strServiceType[] = {"TCP LISTENER", "TCP"};

	if(sckt.serviceType == BG_OPEN_CLIENT)
		err = bg_send(30, LE, "AT+QIOPEN=%d,%d,\"%s\",\"%s\",%ld,%ld,%d", sckt.ctxtID, sckt.connectID,\
		strServiceType[sckt.serviceType], sckt.ip, sckt.remotePort, BG_OPEN_CLIENT_LOCAL_PORT, sckt.accssMode);

	else
		err = bg_send(30, LE, "AT+QIOPEN=%d,%d,\"%s\",\"%s\",%ld,%ld,%d", sckt.ctxtID, sckt.connectID,\
			strServiceType[sckt.serviceType], BG_OPEN_SERVER_IP, BG_OPEN_SERVER_REMOTE_PORT, sckt.localPort, sckt.accssMode);

	CHECK_BG_ERR(err);

	uint8_t openAnsw[20] = {'\0'};
	sprintf(openAnsw, "+QIOPEN: %d,0", sckt.connectID);

	//CHECK_DESIRED_ANSW(uBg.buff, openAnsw, BG_TIMEOUT_ANSW_OK);
	CHECK_OPEN_SCKT(uBg.buff, openAnsw, BG_TIMEOUT_ANSW_OK);
	
	return bg_check_sckt(sckt.connectID);
}

bg_infoTM_t *bg_getter_instance_TM(void)
{
	static bg_infoTM_t infoTM = {.statusTM = BG_TM_INACTIVE, .statusNoCarrier = BG_TM_NO_CARRIER_RESET,
		 .connectID = 200}; 
	return &infoTM;

	//NOTA: el valor de 200 se escoge para no inicializar en alguna conexion soportada (0-11) 
}

void bg_setter_transparentMode(bg_infoTM_t value)
{
	bg_infoTM_t *infoTM = bg_getter_instance_TM();
	*infoTM = value;
}

bg_infoTM_t bg_getter_transparentMode(void)
{
	bg_infoTM_t *infoTM = bg_getter_instance_TM();
	return *infoTM;
}

bg_err_t bg_transparent_mode(uint8_t connectID)
{
	if(connectID > BG_CONNECT_ID_MAX) return BG_ERR_CONNECT_ID_UNSUPORTED;

	bg_infoTM_t infoTM = bg_getter_transparentMode();

	if(infoTM.statusTM == BG_TM_ACTIVE) return BG_OK_TRANSPARENT_MODE;

	bg_err_t err = bg_send(10, LE, "AT+QICFG=\"transwaittm\",%d", connectID); 
	CHECK_BG_ERR(err);

	err = bg_send(10, LE, "AT+QICFG=\"transwaittm\"");
	CHECK_BG_ERR(err);

	err = bg_send(10, LE, "AT+QISWTMD=%d,2", connectID); 
	CHECK_BG_ERR(err);

	CHECK_DESIRED_ANSW(uBg.buff, "CONNECT", BG_TIMEOUT_ANSW_OK);

	bg_infoTM_t valueTM = {.statusTM = BG_TM_ACTIVE, .statusNoCarrier = BG_TM_NO_CARRIER_RESET,\
		 .connectID = connectID};

	bg_setter_transparentMode(valueTM);

	return BG_OK_TRANSPARENT_MODE;
}
//-----------------------Funciones de apertura de socket end------------------------


//-----------------------Funciones de cierre y desactivacion-------------------------
bg_err_t bg_exit_transparent_mode(void)
{
	bgDelay(2000);
	memset(uBg.buff, '\0', sizeof(uBg.buff));
	if(bgUartTx("+++", 3))
	{
		LOG_BG(LE, "[BG_ERR] ERROR MCU TX UART\n");
		return BG_ERR_MCU_TX_UART;
	}
	bgDelay(1000);
	
	CHECK_EXIT_TM_ANSW(uBg.buff, BG_TIMEOUT_ANSW_OK);
	LOG_BG(LE, "EXIT TM CHECKED\n");

	bg_infoTM_t infoTM = bg_getter_transparentMode();
	bg_infoTM_t valueTM = {.statusTM = BG_TM_INACTIVE, .statusNoCarrier = infoTM.statusNoCarrier,\
		.connectID = infoTM.connectID};
	bg_setter_transparentMode(valueTM);

	return BG_OK_EXIT_TRANSPARENT_MODE;
}

bg_err_t bg_close_sckt(uint8_t connectID)
{
	if(connectID > BG_CONNECT_ID_MAX) return BG_ERR_CONNECT_ID_UNSUPORTED;

	bg_err_t err = bg_send(30, LE, "AT+QICLOSE=%d", connectID);
	CHECK_BG_ERR(err);

	return BG_OK_CONNECT_ID_CLOSED;
}

bg_err_t bg_detach(void)
{
	bg_err_t err = bg_send(90, LE, "AT+COPS=2");
	CHECK_BG_ERR(err);

	CHECK_DESIRED_ANSW(uBg.buff, "OK", BG_TIMEOUT_ANSW_OK);

	return BG_OK_DETACH;
}
//-----------------------Funciones de cierre y desactivacion end---------------------


//----------------------Funciones de transmision recepcion de mensajes---------------
bg_err_t bg_transmit_buffAMode(uint8_t connectID, uint8_t *data, uint16_t len)
{
	if(connectID > BG_CONNECT_ID_MAX) return BG_ERR_CONNECT_ID_UNSUPORTED;

	if(data == NULL) return BG_ERR_MCU_PTR_NULL;

	bg_err_t err = BG_OK;
	err = bg_send(5, LE, "AT+QISEND=%d,%d", connectID, len);
	CHECK_BG_ERR(err);

	//espera respuesta del modulo para enviar mensaje
	CHECK_DESIRED_ANSW(uBg.buff, ">", BG_TIMEOUT_ANSW_OK);

	//transmite el mensaje
	if(bgUartTx(data, len))
	{
		LOG_BG(LE, "[BG_ERR] ERROR MCU TX UART\n");
		return BG_ERR_MCU_TX_UART;
	}

	//espera confirmacion de envio de mensaje
	CHECK_DESIRED_ANSW(uBg.buff, "SEND OK", BG_TIMEOUT_ANSW_OK_LONG);

	printf("%s\n",uBg.buff);
	return BG_OK_TRANSMIT;
}

bg_err_t bg_receive_buffAMode(uint8_t connectID, uint8_t *buff, uint16_t *len)
{
	if(connectID > BG_CONNECT_ID_MAX) return BG_ERR_CONNECT_ID_UNSUPORTED;

	if(buff == NULL || len == NULL) return BG_ERR_MCU_PTR_NULL;

	uint8_t flgOverFlow = 0;

	bg_err_t err = bg_send(15, LE, "AT+QIRD=%d,1500", connectID);
	CHECK_BG_ERR(err);

	uint8_t *parsePtr = strchr(uBg.buff, ' ');

	uint8_t auxLen[10] = {'\0'};

	if(parsePtr)
	{
		COPY_PARSE_STR(auxLen, sizeof(auxLen), parsePtr, ' ', '\n');
		*len = atoi(auxLen);
		
		if(*len > 1024) 
		{
			*len = 1024;
			flgOverFlow = 1;
		}
		
		LOG_BG(LD,"RECVlen: %ld\n", *len);
		parsePtr++;
	}
	
	parsePtr = strchr(parsePtr, '\n');

	if(parsePtr)
	{
		parsePtr++;
		memcpy(buff, parsePtr, *len);
	}

	//TODO: Hacer una estrategia en caso de que se reciban mas de 1024Bytes ya que el buffer del modulo guardara el resto y se deberia limpiar
	if(flgOverFlow)
	{
		bg_close_sckt(connectID);
	}

	return BG_OK_RECEIVE;
}

bg_err_t bg_transmit_TM(uint8_t *data, uint16_t len)
{
	if(data == NULL) return BG_ERR_MCU_PTR_NULL;

	bg_infoTM_t infoTM = bg_getter_transparentMode();

	if(infoTM.statusTM == BG_TM_ACTIVE)
		bgUartTx(data, len);
	
	return BG_OK_TRANSMIT;
}
//----------------------Funciones de transmision recepcion de mensajes end-----------


//---------------------------Callbacks LIB para MCU----------------------------
__bg_weak__ void bg_callback_urcDetected(urcRawData_t urcData)
{
	urcRawData_t urcDetected = urcData;

	if(urcData.type < BG_URC_UNSUPPORTED)
		bg_queue_put(urcDetected);
}

__bg_weak__ void bg_urc_parsed_callback(urcInfoData_t infoUrc)
{
	switch(infoUrc.type)
	{
		case BG_URC_CLOSED:
			LOG_BG(LE, "PARSED URC CLOSED\n");
			bg_infoTM_t infoTM = bg_getter_transparentMode();
			bg_close_sckt(infoUrc.connectID);

			if(infoTM.connectID == infoUrc.connectID)
			{
				bg_infoTM_t valueTM = {.statusTM = infoTM.statusTM, .statusNoCarrier = BG_TM_NO_CARRIER_SET,\
					.connectID = infoTM.connectID};
				bg_setter_transparentMode(valueTM);
			}
			//cerrar connectID
		break;

		case BG_URC_INCOMING:
			LOG_BG(LE, "PARSED URC INCOMMING\n");
			bg_incomming_callback(infoUrc.serverID, infoUrc.connectID);
		break;

		case BG_URC_RECV:
			LOG_BG(LE, "PARSED URC RECV\n");
			LOG_BG(LD, "buff: %s\n", infoUrc.buff);
			LOG_BG(LD, "buff[HEX]: ");
			for(int i = 0; i < infoUrc.len; i++)
				LOG_BG(LD, "0X%02X ", infoUrc.buff[i]);
			LOG_BG(LD, "\n");

			LOG_BG(LD, "connectID: %d\n", infoUrc.connectID);

			bg_recv_callback(infoUrc.buff, infoUrc.len, infoUrc.connectID);

		break;

		case BG_URC_INCOMING_FULL:
			LOG_BG(LE, "PARSED URC INCOMMING_FULL\n");
		break;

		case BG_URC_PDP_DEACT:
			LOG_BG(LE, "PARSED URC PDP_DEACT\n");
			bg_pdp_activation(infoUrc.contextID, BG_PDP_ACT);
			bg_pdp_activation_callback(infoUrc.contextID);
			//activar contexto PDP
		break;
	}
}

__bg_weak__ void bg_recv_callback(uint8_t *buff, uint16_t len, uint8_t connectID)
{
	LOG_BG(LE, "buff: %s\n", buff);
	LOG_BG(LE, "buff[HEX]: ");
	for(int i = 0; i < len; i++)
		LOG_BG(LE, "0X%02X ", buff[i]);
	LOG_BG(LE, "\n");

	LOG_BG(LE, "len: %ld\nconnectID: %d\n", len, connectID);
}

__bg_weak__ void bg_incomming_callback(uint8_t serverID, uint8_t connectID)
{
	LOG_BG(LE, "serverID: %ld\nconnectID: %d\n", serverID, connectID);
}

__bg_weak__ void bg_callback_receive_TM(uint8_t *buff, uint16_t nBytes)
{
	LOG_BG(LE, "buff: %s\nlen: %ld\n", buff, nBytes);
}

__bg_weak__ void bg_callback_closed_TM(void)
{
	LOG_BG(LE, "Conexion remota cerro, fuera de TM\n");
}

__bg_weak__ void bg_pdp_activation_callback(uint8_t contextID)
{
	LOG_BG(LE, "Se reactivo ContextID: %d\n", contextID);
}

__bg_weak__ void bg_callback_TM_Inactive(uint8_t connectID, bg_noCarrierTM_t statusNoCarrier)
{
	if(statusNoCarrier == BG_TM_NO_CARRIER_SET)
		LOG_BG(LE, "Salida por NO CARRIER\n");
	
	if(bg_check_sckt(connectID) == BG_OK_CONNECT_ID_OPENNED)
		if(bg_transparent_mode(connectID) == BG_OK_TRANSPARENT_MODE)
			LOG_BG(LE, "Modo TM exitoso\n");
}
//---------------------------Callbacks LIB para MCU end------------------------


//-------------------------------------Funciones de cola----------------------------
void bg_queue_put(urcRawData_t urc)
{	
	qData_t item = {.data.urc = urc, .type = T_BG_URC};
	bgUrcQueue.put(&bgUrcQueue, item);
}

void bg_queue_pop(urcRawData_t *urc)
{
	qData_t item = bgUrcQueue.pop(&bgUrcQueue);

	if(item.type == T_BG_URC)
		*urc = item.data.urc;
}

uint8_t bg_queue_is_empty(void)
{
	return bgUrcQueue.is_empty(&bgUrcQueue);
}

uint8_t bg_queue_is_full(void)
{
	return bgUrcQueue.is_full(&bgUrcQueue);
}
//-------------------------------------Funciones de cola end------------------------
