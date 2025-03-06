/**
 ******************************************************************************************************
 * @file BG77.h
 * @author EM2
 * @brief Esta es una libreria base para implementar funcionalidades basicas del modulo BG77
 * (consulta de estados, apertura de conexiones, transmision y recepcion de datos).
 * @version 1.0
 * @date 2025-02-06
 ******************************************************************************************************
 * @example Ejemplo_Libreria
 * El codigo de ejemplo a continuacion, muestra como utilizar la libreria.
 * De manera general, se necesita:
 * 1. Generar el BSP que son punteros a funcion. 
 * 		1.1 Funcion transmision de UART. 
 * 		1.2 Funcion escritura de GPIOS.
 * 		1.3 Funcion de delay.
 * 		1.4 Funcion de reinicio de MCU. 
 * 2. Llamar los callbacks de UART, Temporizador y Flancos del pin MAIN RI en las interrupciones
 * correspondientes.
 * 3. Establecer o dar el BSP a la libreria.
 * 4. Inicializar la libreria.
 * 5. Realizar proceso de verificacion.
 * 		5.1 Verificar SIM.
 * 		5.2 Verificar el registro del modulo (attach).
 * 		5.3 Verificar la seleccion de operadora (COPS).
 * 		5.4 Verificar contexto PDP.
 * 6. Registrar o attachar el modulo.
 * 		6.1 Desactivar contexto PDP.
 * 		6.2 Registrar (attachar) seleccionando la operadora (COPS).
 * 7. Configurar y activar el contexto PDP.
 * 8. Configurar y abrir sockets (SERVIDOR o CLIENTE).
 * 9. Colocar funcion de manejo de URC en loop de la aplicacion principal.
 * 
 * 10. Recepcion y transmision de mensajes.
 * 		10.1 Dependiendo del modo de acceso usar la funcion de transmision adecuada.
			@code
 			bg_transmit_buffAMode(uint8_t connectID, uint8_t *data, uint16_t len)
 			bg_transmit_TM(uint8_t *data, uint16_t len)
 			@endcode
 * 
 * 		10.2 Dependiendo del modo de acceso usar el callback de recepcion adecuada.
			@code
			void bg_callback_receive_TM(uint8_t *buff, uint16_t nBytes)
			{
			 
			}
			void bg_recv_callback(uint8_t *buff, uint16_t len, uint8_t connectID)
			{
			
			}
			@endcode

 * 11. Usar Callbacks de notificacion de URC 
 *		11.1 Callback de conexion entrante.
 			@code
			void bg_incomming_callback(uint8_t serverID, uint *8_t connectID)
			{
			
			}
			@endcode
 *		11.2 Callback de salida de modo transparente.
 			@code
			void bg_callback_closed_TM(void)
			{
			
			}
			@endcode
 *		11.3 Callback de reactivacion de contexto PDP.
 			@code
			void bg_pdp_activation_callback(uint8_t contextID)
			{
			
			}
			@endcode
 *		11.4 Callback de modo transparente inactivo. El proposito es notificar al usuario
 *			para que pueda regresar a modo transparente.
 			@code
			void bg_callback_TM_Inactive(void)
			{
			
			}
			@endcode
 *
 * Los pasos numero 10 y 11 son opcionales en este codigo de ejemplo, ya que, la libreria llama a
 * funciones de notificacion callback por defecto si no se crean los de usuario.
 * 
 * @code
 * 
 	#include "main.h"
	#include "BG77.h"

	//-----------------------------BSP--------------------------------------
	bg_err_t uart_tx(uint8_t *data, uint16_t len);
	bg_err_t gpioWrite(bgPin_t pin, uint8_t state);
	void msDelay(uint32_t mDelay);
	void resetMCU(void);
	
	bg_err_t uart_tx(uint8_t *data, uint16_t len)
	{
		bg_err_t err = BG_OK;
		if(HAL_UART_Transmit(&UART_BG, data, len, 1000) != HAL_OK)
			err = BG_ERR_MCU_TX_UART;
		return err;
	}

	bg_err_t gpioWrite(bgPin_t pin, uint8_t state)
	{
		if(pin >= BG_UNSUPORTED_PIN)
			return BG_ERR_UNSUPPORTED_PIN;
			
		bg_err_t err = BG_OK;

		uint32_t bgPin[] = {BG_PWRKEY_Pin,  BG_VBAT_Pin, BG_RESET_Pin, BG_PON_TRIG_Pin};
		uint32_t bgPort[] = {BG_PWRKEY_GPIO_Port, BG_VBAT_GPIO_Port, BG_RESET_GPIO_Port,\
			BG_PON_TRIG_GPIO_Port}; 

			HAL_GPIO_WritePin(bgPort[pin], bgPin[pin], state);

			return BG_OK;
	}

	void msDelay(uint32_t mDelay)
	{
		HAL_Delay(mDelay);
	}

	void resetMCU(void)
	{
		HAL_NVIC_SystemReset();
	}

	bg_bspFun_t bspFun = {.uartTx = uart_tx, .gpioWrite = gpioWrite, .msDelay = msDelay,\
	.resetMCU = resetMCU};
	//-----------------------------BSP end--------------------------------------


	//-----------------------------Funcion de recepcion de interrupcion de UART---------------------

	//Creacion e inicialzacion de buffer del tipo uartBuff_t (buffer de 2KB de la libreria)
	uartBuff_t uBg = {.buff = {'\0'}, .len = 0}; 

	//--------------------------------Variables para manejo de UART---------------------
	#define UART_BG huart7 //instancia de UART de modulo de comunicacion

	typedef enum {
	RX_LOG,
	RX_BG,
	RX_NO_SUPPORTED
	}rxUart_t;

	rxUart_t keyUart = RX_NO_SUPPORTED;

	#define flg_uart flg_main.f0 //1: llego una recepcion de UART por interrupcion.
	flags_t flg_main = {0}; //banderas de 1 bit.
	//--------------------------------Variables para manejo de UART end-----------------

	void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
	{
		if(huart == &UART_BG)
		{	
			//-----------Callback de UART para libreria-----------
			bg_uartCallback(uBg.buff, Size);

			HAL_UARTEx_ReceiveToIdle_IT(&UART_BG, uBg.buff, sizeof(uBg.buff));
			keyUart = RX_BG;
			uBg.len = Size;
			flg_uart = 1;
		}
	}
	//-----------------------------Funcion de recepcion de interrupcion de UART end-----------------
	
	//-----------------------------------------------Funcion main-----------------------------------
	int main(void)
	{
		//----------------------Configuracion e inicializacion de MCU----------------------------------
		HAL_Init();
		SystemClock_Config();
		MX_GPIO_Init();
		MX_UART4_Init();
		MX_UART7_Init();
		MX_USART2_UART_Init();
		//----------------------Configuracion e inicializacion de MCU end------------------------------
		
		//---------------------------------------Configurando o estableciendo BSP----------------------
		bg_set_bsp(bspFun);

		//--------------------------Activando la Recepcion de interrupcion de UART----------------------
		HAL_UARTEx_ReceiveToIdle_IT(&UART_BG, uBg.buff, sizeof(uBg.buff));
		flg_uart = 0;
		

		//-------------------------------Inicializando el modulo (libreria)-----------------------------
		bg_init_module();
		bg_data_module();

		//-------------------------------Verificacion de SIM-------------------------------
		if(bg_check_sim() == BG_OK_SIM)
			printf("SIM OK\n");
		else
			printf("SIM NO OK\n");

		//--------------------Verificacion si el modulo esta registrado--------------------
		if(bg_check_attach() == BG_OK_ATTACH)
			printf("ATTACH OK\n");
		else
			printf("ATTACH NO OK\n");

		//------------------------------Verificacion de COPS--------------------------------
		if(bg_query_cops() == BG_ERR_NO_OPER)
			printf("No se ha seleccionado operadora\n");

		//--------------------Consulta la configuracion del contexto PDP--------------------
		if(bg_query_conf_pdp(1) == BG_ERR_NO_CONF_PDP)
		printf("No se ha configurado contexto PDP\n");

		//--------------------Se verifica si esta activado el contexto PDP------------------
		if(bg_check_pdp(1) == BG_OK_PDP_DEACT)
			printf("No esta activado el contexto PDP 1\n");

		//---------------------Se realiza el proceso de registro en la red------------------
		if(bg_attach(COPS_LF) == BG_OK_ATTACH)
			printf("Modulo ATTACHADO\n");

		//----------------------Verifica si se el modulo esta registrado-------------------
		if(bg_check_attach() == BG_OK_ATTACH)
			printf("ATTACH OK\n");
		else
			printf("ATTACH NO OK\n");

		//-----------------------------Configura conterxto PDP-----------------------------
		bg_ctxPdp_t contextPDP_1 ={.ctxtID = 1, .contextType = BG_CTXT_IPV4,\
			.apn = APN_LINKS_FIELD, .usr = "", .psw = ""};
		bg_conf_pdp(contextPDP_1);

		//-------------------------------Activa contexto PDP------------------------------
		bg_pdp_activation(1, BG_PDP_ACT);

		//------------------------------Verificacion de COPS--------------------------------
		if(bg_query_cops() == BG_ERR_NO_OPER)
			printf("No se ha seleccionado operadora\n");

		//---------------------Creacion y apertura de socket 1 SERVIDOR----------------------
		bgSckt_t mySckt = {.ctxtID = 1, .connectID = 0, .ip = BG_OPEN_SERVER_IP,\
		.accssMode = BG_OPEN_BUFF_ACCSS_MODE, .serviceType = BG_OPEN_SERVER,\
		.remotePort = BG_OPEN_SERVER_REMOTE_PORT, .localPort = 2000};
		
		bg_open_sckt(mySckt);
		//---------------------Creacion y apertura de socket 1 SERVIDOR end--------------------

		//----------------------Creacion y apertura de socket 2 CLIENTE------------------------
		bgSckt_t mySckt2 = {.ctxtID = 1, .connectID = 1, .ip = IP_METERCAD,\
			.accssMode = BG_OPEN_BUFF_ACCSS_MODE, .serviceType = BG_OPEN_CLIENT,\
			.remotePort = 2001, .localPort = BG_OPEN_CLIENT_LOCAL_PORT};

		if(bg_open_sckt(mySckt2) == BG_OK_CONNECT_ID_OPENNED)
			printf("Conexion myScket2 Abierta\n");
		//----------------------Creacion y apertura de socket 2 CLIENTE end---------------------

		//--------------Transmision de mensajes de socket 2 en Buffer access mode---------------
		if(bg_transmit_buffAMode(mySckt2.connectID, "hello DOGO \x00\xFF", 13) == BG_OK_TRANSMIT)
			printf("Mensaje enviado\n");

		if(bg_transmit_buffAMode(mySckt2.connectID, "hello COW ", 10) == BG_OK_TRANSMIT)
			printf("Mensaje enviado\n");
		//--------------Transmision de mensajes de socket 2 en Buffer access mode end------------

		//-----------Cambio de modo de transmision a modo transparente del socket 2 y transmision de mensaje-----------
		if(bg_transparent_mode(mySckt2.connectID) == BG_OK_TRANSPARENT_MODE)
		{
			printf("Modo transparente exitoso\n");
			HAL_UART_Transmit(&UART_BG, "hello server", 12, 1000);
			//bg_transmit_TM("hello server", 12); //o tambien se puede usar esta funcion
		}

		//-------------------Salida de modo transparente y cierre del socket 2------------------
		if(bg_exit_transparent_mode() == BG_OK_EXIT_TRANSPARENT_MODE)
		{
				printf("salida de TM Exitosa\n");
				bg_check_pdp(1);
				bg_close_sckt(mySckt2.connectID);
			
				if(bg_check_sckt(mySckt2.connectID) == BG_OK_CONNECT_ID_CLOSED)
					printf("Socket cerrado\n");
		}

		else
			printf("Salida de TM no exitosa\n");
		//-------------------Salida de modo transparente y cierre del socket 2 end------------------
			
		//--------------------------------Consulta del estado de señal------------------------------  
		if(bg_query_signal() == BG_OK_SIGNAL)
			printf("Intensidad de señal aceptable\n");

		else
			printf("Intensidad de señal NO aceptable\n");

		//--------------------------------Desregistro de la red y termina el codigo------------------------------ 
		//if(bg_detach() == BG_OK_DETACH)
			//printf("El Dispositivo esta desregistrado de la red\n");
		// while(1);
		//--------------------------------Desregistro de la red y termina el codigo end-------------------------- 

		//------------------------Apertura y cambio a modo transparente de socket 2 CLIENTE---------------------- 
		if(bg_open_sckt(mySckt2) == BG_OK_CONNECT_ID_OPENNED)
			printf("Conexion mySocket2 Abierta\n");

		if(bg_transparent_mode(mySckt2.connectID) == BG_OK_TRANSPARENT_MODE)
		{
			printf("Modo transparente exitoso\n");
			bg_transmit_TM("hello serverDog", 15);
		}
		//------------------------Apertura y cambio a modo transparente de socket 2 CLIENTE---------------------- 

		//-----------------------------Loop del codigo o hilo principal----------------------------
		while(1)
		{
			//--------------------Funcion de manejo de URC-----------------------
			bg_handle_urc();

			//---------------Transmision de mensaje cada 2 segundos---------------
			bg_transmit_TM("hello dog", 9);
			HAL_Delay(2000);
		}
		//-----------------------------Loop del codigo o hilo principal----------------------------	
	}
	//-----------------------------------------------Funcion main end-------------------------------


	//---------------------------------Funcion de interrupcion de temporizador----------------------
	void callback_systic(void)
	{	
		//-----------Callback de temporizador de 1ms para libreria-----------
		bg_callback_ms();
	}
	//---------------------------------Funcion de interrupcion de temporizador end------------------


	//---------------------------------Funcion de interrupcion de temporizador (Systick)----------------------
	void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
	{
		if(GPIO_Pin == INT_MAIN_RI_Pin)
		{
			if(HAL_GPIO_ReadPin(INT_MAIN_RI_GPIO_Port, INT_MAIN_RI_Pin))
			{
				//-----------Callback de flanco de subida de pin MAIN RI-----------
				bg_mainRICallback(MAIN_RI_EDGE_RISING);
			}

			else
			{
				//-----------Callback de flanco de bajada de pin MAIN RI-----------
				bg_mainRICallback(MAIN_RI_EDGE_FALLING);
			}
		}
	}
	//---------------------------------Funcion de interrupcion de temporizador  (Systick) end------------------
 * @endcode
 * @copyright Copyright (c) 2025
 *********************************************************************************************************
 */
#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"
#include "string.h"
#include "stdarg.h"

#include "../BG77/queue_module/queue_module.h"

#ifndef _BG77_H_
#define _BG77_H_

#define LD 0	//Sirve para indicar que NO se imprima mensaje en la MACRO LOG_BG
#define LE 1	//Sirve para indicar que SI se imprima mensaje en la MACRO LOG_BG
#define _BG_DEBUG_
#ifdef _BG_DEBUG_

/**
 * @brief MACRO para imprimir log.
 * Este puede desactivarse en tiempo de ejecucion.
 * 
 */
#define LOG_BG(enable, ...)\
do{\
	if(enable) \
		printf(__VA_ARGS__);\
}while(0)

/**
 * @brief Es un Log derivado que rastrea la ruta y la linea en donde se esta imprimiendo el mensaje.
 * 
 */
#define LOG_BG_TRACE(enable, ...)\
do{\
	if(enable) \
	{\
		printf("[%s: %d]", __FILE__, __LINE__);\
		printf(__VA_ARGS__);\
	}\
}while(0)
#else
#define LOG_BG(...)
#define LOG_BG_TRACE(...)
#endif

/**
 * @brief MACRO para generar funciones del tipo weak 
 * 
 */
#define __bg_weak__ __attribute__((weak))

#define MAX_SEC 0XFFFFFFFFUL

#define BG_TIMEOUT_ANSW 10UL 			//timeout de espera de respuesta de comando BG
#define BG_TIMEOUT_ANSW_OK 20UL 		//timeout de espera de respuesta esperada del modulo BG
#define BG_TIMEOUT_ANSW_OK_LONG 40UL 	//timeout de espera de respuesta esperada del modulo BG para tiempos largos

#define IP_METERCAD  "192.168.4.58"		//Direccion IP de maquina virtual metercad
#define IP_PROXYGAMMA "192.168.4.57" 	//Direccion IP de maquina virtual proxygamma
#define IP_TCP_LISTENER "127.0.0.1"		//Direccion IP de por defecto para abrir socket en modo servidor

#define APN_LINKS_FIELD "cantilever.1"	//APN de linksfield


#define COPS_LF "334020"	//Seleccion de operadora linksfield
#define COPS_ATT_1 "334090" //Seleccion de operadora ATT 1
#define COPS_ATT_2 "334050" //Seleccion de operadora ATT 2

#define BG_CONTEXT_ID_MAX 7	//Es el maximo numero de contextID que soporta el modulo BG77
#define BG_CONTEXT_ID_MIN 1	//Es el minimo numero de contextID que soporta el modulo BG77

#define BG_CONNECT_ID_MAX 11 //Es el maximo numero de connectID que soporta el modulo BG77 (rango 0-11)
#define BG_CONNECT_ID_MIN 0  //Es el minimo numero de connectID que soporta el modulo BG77 (rango 0-11)

/**
 * @brief Esta macro es una funcion inline que evalua que no se presento algun error del tipo bg_err_t.
 * Su proposito es mantener la legibilidad del codigo.
 * @note Se debe de alimentar con una variable del tipo bg_err_t que almacene el resultado de una funcion
 * que devuelva un codigo de error bg_err_t
 * 
 */
#define CHECK_BG_ERR(err) do{ if(err != BG_OK) return err; } while(0)

/**
 * @brief Remplaza la primera ocurrencia de un caracter especificado en un array por otro deseado.
 * El proposito de esta macro es sustituir el primer caracter encontrado por otro deseado y su utilidad
 * es para copiar informacion que debe ser parseada.
 * 
 * @note Si el caracter no es encontrado no sucede nada.
 * 
 * @param rempSrcBuff Es el array al que se quiere remplazar un caracter.
 * @param chrRemplaced Es el caracter que se quiere remplazar por otro deseado.
 * @param chrDesired Es el caracter deseado.
 * 
 * @example Uso_de_REMPLACE_CHAR
 * En este ejemplo se intercambia el caracter 'u' del buffer fooBuff por el caracter 'a'
 * @code
	uint8_t fooBuff[] = "cut";
	REMPLACE_CHAR(fooBuff, 'u', 'a');
 * @endcode
 */
#define REMPLACE_CHAR(rempSrcBuff, chrRemplaced, chrDesired)\
do{\
	uint8_t *_remplacePtr = strchr(rempSrcBuff, chrRemplaced);\
	if(_remplacePtr != NULL)\
		*_remplacePtr = chrDesired;\
}while(0)


/**
 * @brief Copia una cadena que se encuentra entre dos caracteres desde un buffer origen a otro buffer de destino.
 * El proposito de esta MACRO es copiar cadenas de informacion que debe ser parseada por delimitadores como '\n', ' ', '=', ',' 
 * 
 * @param dstBuff Es el buffer de destino.
 * @param dstBuffSize Es el tamaño total de dstBuff. Sirve para evaluar el desbordamiento de memoria.
 * @param srcBuff Es el buffer origen.
 * @param chr1 Es el caracter que indica desde donde se empezar a copiar. Se empieza a copiar una posicion despues de este caracter.
 * @param chr2 Es el caracter hasta el cual se va a copiar. Este ultimo caracter se cambia a nulo '\0' para delimitar la cadena.  
 * 
 * @example Uso_de_COPY_PARSE_STR
 * En este ejemplo se copia la cadena "125" proveniente de dataBuff a parseBuff
 * @code
	uint8_t dataBuff[] = "vol=125v,pot=12w";
	uint8_t parseBuff[30] = {'\0'};
	
	COPY_PARSE_STR(sizeof(parseBuff), parseBuff, dataBuff, '=', 'v');
 * @endcode
 */
#define COPY_PARSE_STR(_dstBuff, dstBuffSize, srcBuff, chr1, chr2)\
do { \
	uint8_t *dstBuff = _dstBuff;\
    uint8_t *_parsePtr = strchr(srcBuff, chr1); \
    if(_parsePtr == NULL || dstBuff == NULL) break; \
    _parsePtr++; \
	uint8_t *_endPtr = NULL;\
	if(chr1 == chr2)\
		_endPtr = strchr(_parsePtr + 1, chr2); \
	else\
    	_endPtr = strchr(_parsePtr, chr2); \
    if(_endPtr == NULL) break; \
    size_t len = _endPtr - _parsePtr; \
    if(len >= dstBuffSize) break;\
    for(size_t i = 0; i < len; i++) dstBuff[i] = _parsePtr[i];\
    dstBuff[len] = '\0'; /* Agrega el terminador nulo */ \
} while(0)

/**
 * @brief Verifica que una cadena deseada se encuentre dentro de un array o buffer durante un tiempo especificado. 
 * 
 * @param srcBuff Es el buffer o array en donde se va a buscar la cadena deseada.
 * @param desiredAnsw Es la cadena deseada que se quiere buscar dentro del array.
 * @param _timeot Es el tiempo de espera en el que tratara de encontrar la cadena deseada.
 * 
 */
#define CHECK_DESIRED_ANSW(srcBuff, desiredAnsw, _timeout)\
do{\
	bg_start_timeout();\
	while(!strstr(srcBuff, desiredAnsw) && count_sec_bg < _timeout)\
		__asm__("nop");\
	if(_timeout <= bg_stop_timeout())\
	{\
		LOG_BG(LE,"[BG_ERR] TIMEOUT RESPUESTA DESEADA\n");\
		return BG_ERR_TIMEOUT_ANS_DESIRED;\
	}\
}while(0)

/**
 * @brief Verifica que el comando de Power down sea exitoso. 
 * 
 * @param srcBuff Es el buffer o array en donde esta la respuesta al comando de power down.
 * @param _timeot Es el tiempo de espera en el que tratara de verificar el comando power down.
 * 
 */
#define CHECK_POWDWN_ANSW(srcBuff, _timeout)\
do{\
	bg_start_timeout();\
	while(!strstr(srcBuff, "OK") && !strstr(srcBuff, "RDY") && count_sec_bg < _timeout)\
		__asm__("nop");\
	if(_timeout <= bg_stop_timeout())\
	{\
		LOG_BG(LE,"[BG_ERR] TIMEOUT RESPUESTA DESEADA\n");\
		return BG_ERR_TIMEOUT_ANS_DESIRED;\
	}\
}while(0)

/**
 * @brief Verifica que la salida de modo transparente sea exitosa. 
 * 
 * @param srcBuff Es el buffer o array en donde esta la respuesta a la salida de modo transparente.
 * @param _timeot Es el tiempo de espera en el que tratara de verificar la salida de modo transparente.
 * 
 */
#define CHECK_EXIT_TM_ANSW(srcBuff, _timeout)\
do{\
	bg_start_timeout();\
	while(!strstr(srcBuff, "OK") && !strstr(srcBuff, "QIURC") && count_sec_bg < _timeout)\
		__asm__("nop");\
	if(_timeout <= bg_stop_timeout())\
	{\
		LOG_BG(LE,"[BG_ERR] TIMEOUT RESPUESTA DESEADA\n");\
		return BG_ERR_TIMEOUT_ANS_DESIRED;\
	}\
}while(0)


/**
 * @brief Verifica que la salida de modo transparente sea exitosa.
 *
 * @param srcBuff Es el buffer o array en donde esta la respuesta a la salida de modo transparente.
 * @param _timeot Es el tiempo de espera en el que tratara de verificar la salida de modo transparente.
 *
 */
#define CHECK_OPEN_SCKT(srcBuff, desiredAnsw, _timeout)\
do{\
	bg_start_timeout();\
	while(!strstr(srcBuff, desiredAnsw) && !strstr(srcBuff, ",0") && count_sec_bg < _timeout)\
		__asm__("nop");\
	if(_timeout <= bg_stop_timeout())\
	{\
		LOG_BG(LE,"[BG_ERR] TIMEOUT RESPUESTA DESEADA\n");\
		return BG_ERR_TIMEOUT_ANS_DESIRED;\
	}\
}while(0)

/**
 * @brief MACRO generadora de tipos de buffer.
 * La utilidad de esta MACRO es poder generar variables de buffers con diferente tamaño agrupando su tamaño de contenido.
 * @param x: es el tamaño del buffer
 * @param name: es el nombre del tipo de variable
 * @example Uso_de_CREATE_BUFF
 * En este ejemplo se genera un tipo de variable llamado buff256_t el cual sirve para generar buffers de 256 bytes
   CREATE_BUFF(256, buff256)
 * @code
	buff256_t uart = {.buff = {'\0'}, .len = 0};
	strcpy(uart.buff, "hello uart");
	len = strlen(uart.buff);

	buff256_t sensor = {.buff = {'\0'}, .len = 0};
	strcpy(sensor.buff, "hello sensor");
	len = strlen(sensor.buff);

	buff256_t aux = {.buff = {'\0'}, .len = 0};
	strcpy(aux.buff, "hello aux");
	len = strlen(aux.buff);
 * @endcode
 */
#define CREATE_BUFF(x, name) \
typedef struct \
{ \
	uint8_t buff[(x)]; \
	uint16_t len; \
} name##_t;

#define SIZE_BG_BUFF 2048
/**
 * @brief Se crea un tipo de variable llamado uartBuff_t para generar buffers de uart de tamaño 1024By
 * 
 */
CREATE_BUFF(SIZE_BG_BUFF, uartBuff);

/**
 * @brief tipo de variable que almacena el catalogo de errores.
 */
typedef enum{
	BG_ERR_TIMEOUT_ANS = -255, //Error de timeout al no recibir respuesta del modulo BG
	BG_ERR_TIMEOUT_ANS_DESIRED, 		//Error de timeout al no recibir respuesta esperada del modulo BG
	BG_ERR_MCU_TX_UART,			//Ocurrio un error en la transmision por UART al BG
	BG_ERR_MCU_PTR_NULL,	//El puntero que se esta entregando apunta a NULL
	BG_ERR_UNSUPPORTED_PIN, //Se quiere cambiar el estado de un pin no soportado o mapeado (PWR_KEY, VBAT, RESET, PON_TRIG)
	BG_ERR_SIM_NO_OK,			//No se detecta SIM
	BG_ERR_PARSE,			//No se encontro el delimitador con  funcion strchr()
	BG_ERR_ATTACH_NO_OK,			//El modulo NO esta registrado en la red
	BG_ERR_NO_OPER,			//No se ha seleccionado una operadora en COPS
	BG_ERR_NO_CONF_PDP,		//No se ha configurado el contexto PDP
	BG_ERR_CTXT_ID_UNSUPPORTED, //Se esta tratando de utilizar un contextID no soportado rango 1-7 para BG77
	BG_ERR_CONNECT_ID_UNSUPORTED,	//Se esta tratando de utilizar un connectID no soportado rango 0-11 para BG77
	BG_ERR_CONNECT_ID_NOT_USED,	//Connect ID no utilizado
	BG_ERR_CONF_PDP,	//Error en la configuracion de contexto PDP 
	BG_ERR_ACT_PDP, 	//Error en la activacion PDP
	BG_ERR_SIGNAL,		//No se tiene una intensidad de señal aceptable
	BG_OK = 0,				//No hay error
	BG_OK_SIM,				//Se detecto SIM
	BG_OK_ATTACH,			//El modulo esta registrado en la red
	BG_OK_PDP_ACT,				//Contexto PDP activado
	BG_OK_PDP_DEACT,			//Contexto PDP desactivado
	BG_OK_CONNECT_ID_OPENNED,	//Connect ID abierto
	BG_OK_CONF_PDP,	//Contexto PDP configurado
	BG_OK_TRANSPARENT_MODE, //La conexion se paso correctamente a modo transparente
	BG_OK_EXIT_TRANSPARENT_MODE, //El modulo salio de modo transparente correctamente
	BG_OK_CONNECT_ID_CLOSED,	//Connect ID cerrado
	BG_OK_DETACH,	//El modulo se desrgistro correctamente
	BG_OK_TRANSMIT, //Se consiguio transmitir un mensaje
	BG_OK_RECEIVE,	//Se consiguio recuperar el mensaje recibido
	BG_OK_SIGNAL	//Se tiene una intensidad de señal aceptable (rsrp >= -115 && sinr >= 0)
}bg_err_t;

//-----------------------------------Flags-----------------------------------
/**
 * @brief Tipo de dato para crear variables de banderas de 1 bit (16 bits)
 * 
 */
typedef struct{
	uint16_t f0		: 1;
	uint16_t f1 	: 1;
	uint16_t f2 	: 1;
	uint16_t f3 	: 1;
	uint16_t f4 	: 1;
	uint16_t f5 	: 1;
	uint16_t f6 	: 1;
	uint16_t f7 	: 1;
	uint16_t f8 	: 1;
	uint16_t f9 	: 1;
	uint16_t f10 	: 1;
	uint16_t f11 	: 1;
	uint16_t f12 	: 1;
	uint16_t f13 	: 1;
	uint16_t f14 	: 1;
	uint16_t f15 	: 1;
}flags_t;
//-----------------------------------Flags end-----------------------------------

//-----------------------------------BSP-----------------------------------
/**
 * @brief tipo de dato que enlista todos los pines soportados para la escritura de GPIOs
 * 
 */
typedef enum
{
	BG_PWRKEY_PIN,
	BG_VBAT_PIN,
	BG_RESET_PIN,
	BG_PON_TRIG_PIN,
	BG_UNSUPORTED_PIN
}bgPin_t;

/**
 * @brief tipo de dato para crear un puntero a funciones uart (TX o RX)
 * 
 */
typedef bg_err_t (*uartFun_t)(uint8_t *data, uint16_t len);

/**
 * @brief tipo de dato para crear un puntero a funcion de escritura de GPIOs
 * 
 */
typedef bg_err_t (*gpioWriteFun_t)(bgPin_t pin, uint8_t state);

/**
 * @brief tipo de dato para crear un puntero a funcion de delay (en mili segundos)
 * 
 */
typedef void (*delayFun_t)(uint32_t mDelay);

/**
 * @brief tipo de dato para crear un puntero a funcion de reset de sistema (reset de MCU)
 * 
 */
typedef void (*resetFun_t)(void);

/**
 * @brief Tipo de variable que contiene los punteros a funcion del BSP
 * 
 */
typedef struct
{
	uartFun_t uartTx; 
	gpioWriteFun_t gpioWrite;
	delayFun_t msDelay;
	resetFun_t resetMCU;
}bg_bspFun_t;

/**
 * @brief Funcion para establecer los punteros a función del BSP (UART TX, Delay, GPIO_write, Reset de MCU).
 * 
 * @param bspFun Variable que contiene los punteros a funcion para el BSP.
 */
void bg_set_bsp(bg_bspFun_t bspFun);

//-----------------------------------BSP end-----------------------------------

//---------------------------Callbacks MCU para LIB----------------------------
/**
 * @brief Tipo de variable que indica el flanco que sucedio en el pin MAIN_RI
 * 
 */
typedef enum
{
	MAIN_RI_EDGE_FALLING,
	MAIN_RI_EDGE_RISING,
	MAIN_RI_EDGE_UNSUPPORTED
}mainRiEdge_t;

/**
 * @brief Tipo de variable que contiene la informacion util de URC.
 * 
 */
typedef struct{
    uint8_t buff[1024];
    uint16_t len;
    uint8_t type;
	uint8_t connectID;
	uint8_t contextID;
	uint8_t serverID;
}urcInfoData_t;

/**
 * @brief Esta funcion de callback se tiene que llamar en una funcion de interrupción de temporizador
 * en donde se genere cada 1ms.
 * 
 * Sirve para proveer a la libreria de un conteo de reloj y poder determinar tiempos de espera (timeouts).
 * 
 */
void bg_callback_ms(void);

/**
 * @brief Esta funcion de callback se debe llamar en una funcion de interrupcion de recepcion de uart
 * en la cual se debe pasar el buffer y el tamaño del mensaje recibido por UART.
 * 
 * Su funcion es proveer a la libreria todos los datos de recepcion por interrupcion de UART en el puerto
 * correspondiente al modulo.
 * 
 * @param buff Es la direccion del buffer que contiene los datos recibidos en la interrupcion de UART.
 * @param nBytes Es el numero de bytes recibidos en la interrupcion de UART.
 */
void bg_uartCallback(uint8_t *buff, uint16_t nBytes);

/**
 * @brief Esta funcion de callback se debe llamar en una funcion de interrupcion de pin digital
 * que corresponde al pin MAIN_RI del modulo de comunicaciones en la cual se debe especificar el 
 * flanco generado.
 * 
 * Sirve para proveer a la libreria la deteccion del cambio de estado en la entrada del pin MAIN_RI.
 * 
 * @param edge Es el flanco que se genero en la interrupcion digital (se debe pasar MAIN_RI_EDGE_FALLIN
 * si se genero un flanco de bajada o MAIN_RI_EDGE_RISING si se genero flanco de subida).
 */
void bg_mainRICallback(mainRiEdge_t edge);

/**
 * @brief Esta funcion se encarga de manejar los URC encolados.  
 * Si hay un URC encolado, se procede a parsearlo y a llamar a un callback para entregar informacion parseada al usuario.
 * Por defecto llama a la bg_urc_parsed_callback(urcInfoData_t infoUrc) para indicar el resultado de URC;
 * 
 * NOTE: El usuario puede presendir de esta funcion y hacer su propia estrategia de parseo de URC. Sin embargo, si se
 * quiere utilizar esta funcion, esta se debe de colocar en un ciclo de poleo en el codigo principal de la aplicacion, asi como,
 * usar las funciones bg_callback_urcDetected(urcRawData_t urcData) y bg_urc_parsed_callback(urcInfoData_t infoUrc) que tiene por defecto
 * la libreria (o en su defecto el usuario debera hacer la suyas propias tomando como base estas funciones).
 */
void bg_handle_urc(void);
//---------------------------Callbacks MCU para LIB end----------------------------


//------------------Funciones basicas y de configuracion de modulo------------------
/**
 * @brief Esta funcion permite enviar comandos AT en formato de especificadores de formato (como printf)
 * 
 * @param timeout Es el tiempo de espera para conseguir respuesta del modulo. La unidad de tiempo son segundos. 
 * @param enablePrint Si tiene el valor de 0 no imprime la respuesta del modulo en el Log. Cualquier otro valor, imprime la respuesta.
 * Es recomendable usar las MACROS LE (para imprimir respuesta) y LD (para NO imprimir la respuesta).
 * @param fmt Es el formato del mensaje.
 * @param ... Son los parametros variables que se utilizan en el formato del mensaje generado.
 * @return bg_err_t Retorna el codigo de error basado en el tipo bg_err_t. Si todo esta bien se espera BG_OK
 */
bg_err_t bg_send(uint32_t timeout, uint8_t enablePrint, const char *fmt, ...);

/**
 * @brief Esta funcion realiza la secuencia necesaria de los pines BG_PWRKEY_PIN, BG_VBAT_PIN,
	BG_RESET_PIN para encender al modulo de comunicación.
 * 
 * @return bg_err_t Devuelve el codigo de error basado en el tipo bg_err_t. Si todo esta bien se espera BG_OK
 */
bg_err_t bg_power_on(void);

/**
 * @brief Realiza el apagado del modulo con el comando AT+QPOWD
 * 
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. Si todo esta bien se espera BG_OK
 */
bg_err_t bg_power_off(void);

/**
 * @brief Funcion para inicializar el modulo. 
 *	Sus funciones son encender y configurar el modulo.
 * 
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. Si todo esta bien se espera BG_OK
 */
bg_err_t bg_init_module(void);

//------------------Funciones basicas y de configuracion de modulo end--------------

//------------------------------Funciones de consultas------------------------------
/**
 * @brief Funcion para imprimir la version de FW del modulo, ICCI e IMEI.
 *
 *
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. Si todo esta bien se espera BG_OK
 */
bg_err_t bg_data_module(void);

/**
 * @brief Funcion para detectar SIM.
 *
 *
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. Si todo esta bien se espera BG_OK_SIM
 */
bg_err_t bg_check_sim(void);

/**
 * @brief Funcion para verificar el registro en red.
 *
 *
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. Si el modulo esta registrado
 *  se espera BG_OK_ATTACH
 */
bg_err_t bg_check_attach(void);

/**
 * @brief Funcion que imprime parametros de la consulta de COPS 
 * modo, operador, tecnologia de acceso
 * 
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. 
 * Si se tiene un operador ya registrado regresa BG_OK.
 */
bg_err_t bg_query_cops(void);


/**
 * @brief Funcion que imprime parametros de la configuracion de contexto PDP (APN, USER, PASSWORD, AUTHENTICATION)
 * 
 * @param contextID Es el contexto ID que se quiere consutar
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. Si todo esta bien retorna BG_OK
 */
bg_err_t bg_query_conf_pdp(uint8_t contextID);

/**
 * @brief Verifica que se tenga activado el contexto PDP seleccionado (contextID)
 * 
 * @param ctxtID Es el numero de contexto que se quiere verificar (rango 1-7).
 * 
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. 
 * Si el contexto PDP del contextID esta activado retorna BG_OK_PDP_ACT
 */
bg_err_t bg_check_pdp(uint8_t ctxtID);

/**
 * @brief Verifica el estado de una conexion
 * 
 * @param connectID el numero de conexion (rango 0-11)
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. 
 * Si la conexion esta abierta devuelve BG_OK_CONNECT_ID_OPENNED.
 */
bg_err_t bg_check_sckt(uint8_t connectID);

/**
 * @brief Consulta la intensidad de la señal cuando el modulo ya esta registrado.
 * 
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. 
 * Si intensidad de señal es aceptable regresa BG_OK_SIGNAL.
 */
bg_err_t bg_query_signal(void);
//------------------------------Funciones de consultas end--------------------------


//-------------------------Funciones de Attach y activacion PDP----------------------
/**
 * @brief Es el tipo de variable que define el tipo de autenticacion a utilizar.
 * 
 */
typedef enum
{
	BG_AUTH_NONE, 
	BG_AUTH_PAP, 
	BG_AUTH_CHAP,
	BG_AUTH_PAP_CHAP,
	BG_AUTH_NO_SUPPORTED 
}bgAuth_t;

/**
 * @brief Es el tipo de variable que define el tipo de contexto.
 * 
 */
typedef enum
{
	BG_CTXT_IPV4 = 1, 
	BG_CTXT_IPV6,
	BG_CTXT_IPV4_IPV6,
	BG_CTXT_UNSUPPORTED
}bgCtxtType_t;

/**
 * @brief Es el tipo de variable que define la opcion para activar o desactivar
 * el contexto PDP.
 * 
 */
typedef enum
{
	BG_PDP_ACT, //Opcion para activar el contexto PDP.
	BG_PDP_DEACT, //Opcion para desactivar el contexto PDP.
	BG_PDP_ACT_UNSUPORTED 
}bg_actPdp_t;

/**
 * @brief Es el tipo de variable que define la configuracion de contexto PDP.
 * 
 */
typedef struct
{
	uint8_t ctxtID;
	bgCtxtType_t contextType;
	uint8_t *apn;
	uint8_t *usr;
	uint8_t *psw;
	bgAuth_t auth;
}bg_ctxPdp_t;

/**
 * @brief Funcion para registrar el modulo en la red (attach).
 * 
 * @param operator Es la operadora del SIM.
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. Si el modulo consigue registrarse
 *  se espera BG_OK_ATTACH.
 */
bg_err_t bg_attach(uint8_t *operator);

/**
 * @brief Configura los parametros de contexto PDP que se quiere activar.
 * 
 * @param contextPDP Es una estructura de datos que contiene los parametros de configuracion 
 * del contexto PDP.
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. Si se configuro
 * correctamente el contexto devolvera BG_OK_CONF_PDP.
 */
bg_err_t bg_conf_pdp(bg_ctxPdp_t contextPDP);

/**
 * @brief Activa o desactiva el contexto PDP previamente configurado.
 * 
 * @param ctxtID Numero de contexto PDP que se quiere activar/desactivar (rango 1-7).
 * @param act Opcion que selecciona si se quiere activar o desactivar el contexto PDP especificado.
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. Si se desactivo el contexto PDP
 * correctamente devuelve BG_OK_PDP_DEACT, si se activo el contexto PDP correctamente regresa BG_OK_PDP_ACT.
 */
bg_err_t bg_pdp_activation(uint8_t ctxtID, bg_actPdp_t act);
//-------------------------Funciones de Attach y activacion PDP end------------------


//------------------------Funciones de apertura de socket---------------------------
#define BG_OPEN_SERVER_REMOTE_PORT 0 //Esta MACRO especifica el puerto remoto en modo Servidor.
#define BG_OPEN_SERVER_IP "127.0.0.1" //Esta MACRO especifica la direccion IP en modo Servidor.

#define BG_OPEN_BUFF_ACCSS_MODE 0	//Esta MACRO especifica que se quiere usar el modo de acceso en buffer acces mode.
#define BG_OPEN_TRANSPARENT_MODE 2	//Esta MACRO especifica que se quiere usar el modo de acceso en modo transparente.

#define BG_OPEN_CLIENT_LOCAL_PORT 0	//Esta MACRO especifica el puetro local automatico en modo Cliente.

/**
 * @brief Es el tipo de dato para seleccionar el tipo de servicio en una apertura de conexion (Cliente o Servidor TCP).
 * 
 */
typedef enum
{
	BG_OPEN_SERVER,	//Opcion para abrir conexion en modo Servidor.
	BG_OPEN_CLIENT	//Opcion para abrir conexion en modo Cliente.
}bgServiceType_t;

/**
 * @brief Tipo de dato que contiene todos los parametros de red para abrir un socket.
 * 
 */
typedef struct
{
	uint8_t ctxtID; //contextID del contexto PDP que se activo
	uint8_t connectID; //connectID es el numero de conexion. Sirve para identificar conexiones (rango 0-11)
	bgServiceType_t serviceType; //Es el tipo de conexion a abrir (TCP Cliente o TCP Servidor)
	uint8_t *ip; //Direccion IP de destino cuando la conexion es Cliente
	uint32_t remotePort; //Puerto remoto al que se quiere apuntar cuando la conexion es Cliente
	uint32_t localPort; //Puerto local del dispositivo se utiliza cuando la conexion es Servidor (en modo Cliente por defecto es 0)
	uint32_t accssMode; //Selecciona la opcion de modo de acceso Buffer acces mode o Transparent mode (las conexiones cliente son las que pueden iniciar como Transparent mode)
}bgSckt_t;


/**
 * @brief Abre una conexion (socket) TCP en modo servidor o cliente.
 * 
 * @param sckt Es una estructura de datos que contiene todos los parametros necesarios para abrir una conexion.
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. Si el modulo consigue 
 * abrir la conexion devolvera BG_OK_CONNECT_ID_OPENNED
 */
bg_err_t bg_open_sckt(bgSckt_t sckt);

/**
 * @brief Permite al dispositio cambiar su modo de acceso a modo transparente.
 * 
 * @param connectID Es el numero de conexion que se desea cambiar a modo transparente.
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. Si el modulo consigue entrar
 * en modo transpoarente retornara BG_OK_TRANSPARENT_MODE.
 */
bg_err_t bg_transparent_mode(uint8_t connectID);
//-----------------------Funciones de apertura de socket end------------------------


//-----------------------Funciones de cierre y desactivacion-------------------------
/**
 * @brief Hace que el dispositivo salga de modo transparente
 * 
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. Si el modulo consigue salir
 * de modo transpoarente retornara BG_OK_EXIT_TRANSPARENT_MODE.
 */
bg_err_t bg_exit_transparent_mode(void);

/**
 * @brief Cierra una conexion o socket
 * 
 * @param connectID Es el numero de conexion que se quiere cerrar.
 * @return bg_err_t bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. Si se consigue
 * cerrar la conexion retorna BG_OK_CONNECT_ID_CLOSED.
 */
bg_err_t bg_close_sckt(uint8_t connectID);

/**
 * @brief Sirve para desregistrar el modulo de la red (detach).
 * 
 * @return bg_err_t  Regresa el codigo de error basado en el tipo bg_err_t. Si se consigue
 * desregistrar al modulo de la red devuelve BG_OK_DETACH.
 */
bg_err_t bg_detach(void);

//-----------------------Funciones de cierre y desactivacion end---------------------


//----------------------Funciones de transmision recepcion de mensajes---------------
/**
 * @brief Funcion para transmitir mensajes por un socket que esta en modo buffer access mode
 * 
 * @param connectID numero de conexion a la que se quiere transmitir el mensaje.
 * @param data puntero al buffer que contiene el dato a transmitir.
 * @param len numero de bytes que se quieren transmitir del buffer.
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. Si se consigue
 * enviar el mensaje el modulo devuelve BG_OK_TRANSMIT.
 */
bg_err_t bg_transmit_buffAMode(uint8_t connectID, uint8_t *data, uint16_t len);

/**
 * @brief Funcion que se encarga de recuperar los datos recibidos por una conexion en buffer access mode
 * 
 * @param connectID numero de conexion en la cual se recibio un mensaje.
 * @param buff puntero a buffer en donde se copiara el mensaje recibido.
 * @param len puntero a variable que almacene el numero de bytes recibidos en el mensaje.
 * @return bg_err_t Regresa el codigo de error basado en el tipo bg_err_t. Si se consigue
 * enviar el mensaje el modulo devuelve BG_OK_RECEIVE.
 */
bg_err_t bg_receive_buffAMode(uint8_t connectID, uint8_t *buff, uint16_t *len);

/**
 * @brief Transmite datos en modo transparente. 
 * 
 * NOTE: Si no se tiene habilitado el modo transparente no envia nada por UART.
 * 
 * @param data Es el puntero al buffer que contiene el mensaje a transmitir. 
 * @param len Es el numero de bytes que se quieren transmitir.
 * @return bg_err_t 
 */
bg_err_t bg_transmit_TM(uint8_t *data, uint16_t len);
//----------------------Funciones de transmision recepcion de mensajes end-----------


//---------------------------Callbacks LIB para MCU----------------------------
/**
 * @brief Variable de tipo que sirve para identificar que tipo de URC fue recibido.
 * 
 */
typedef enum
{
	BG_URC_CLOSED,
	BG_URC_RECV,
	BG_URC_INCOMING_FULL,
	BG_URC_INCOMING,
	BG_URC_PDP_DEACT,
	BG_URC_EXIT_TM,
	BG_URC_NO_CARRIER,
	BG_URC_UNSUPPORTED
}bg_urcType_t;

/**
 * @brief Este tipo de variable sirve para indicar si llego un mensaje de NO CARRIER que indica
 * la salida de modo transparente por desconexion.
 * 
 */
typedef enum 
{
	BG_TM_NO_CARRIER_SET,
	BG_TM_NO_CARRIER_RESET
}bg_noCarrierTM_t;

/**
 * @brief Esta funcion corresponde a un callback para el usuario. Esta funcion se llamara cuando se detecte un URC.
 * La libreria tiene su callback por defecto y lo que hace es encolar el URC recibido en una cola interna de la libreria.
 * 
 * NOTE: El Usuario puede prescindir de ella y realizar su estrategia de manejo de URC en este punto.
 * 
 * @param urcData Es una variable que contiene el buffer de informacion no analizada (o datos crudos) y el tipo de URC detectado.
 */
void bg_callback_urcDetected(urcRawData_t urcData);

/**
 * @brief Esta funcion es un callback en donde se le va a entregar al usuario la informacion util de un URC detectado y parseado.
 * La funcion por defecto de la libreria, se dedica a realizar el tratamiento o manejo correspondiente al URC que se presenta. 
 * Ejemplo: 
 * Si se presenta un URC de cierre de socket procede a cerrar dicho socket. 
 * Si se presenta un URC de mensaje RECV procede a recuperarlo y entregarlo en un callback (mensaje, tamaño en bytes, conexion). 
 * 
 * NOTE: Esta funcion se llama en la funcion de manejo de URC bg_handle_urc(void) despues de parsear el URC encolado.
 * Sin embargo, el Usuario puede prescindir de ella y realizar su estrategia de manejo de URC en este punto.
 * @param infoUrc Es una variable que contiene la informacion util del URC.
 */
void bg_urc_parsed_callback(urcInfoData_t infoUrc);

/**
 * @brief Esta funcion es un callback que notifica al usuario que se recibio un mensaje de alguna conexion en modo buffer
 * access mode. Este callback se llama en bg_urc_parsed_callback(urcInfoData_t infoUrc) al detectar y parsear un URC RECV
 * y entrega al usuario el mensaje, tamaño en bytes y el numero de conexion que recibio dicho mensaje.
 * 
 * La funcion por defecto de la libreria solo imprime el mensaje recibido, el tamaño y el numero de connectID
 * @param buff Puntero a funcion que apunta al mensaje recibido.
 * @param len Tamaño en bytes del mensaje recibido.
 * @param connectID Conexion de la cual se recibio el mensaje.
 */
void bg_recv_callback(uint8_t *buff, uint16_t len, uint8_t connectID);

/**
 * @brief Esta funcion corresponde a un callback que notifica al usuario la ocurrencia de una conexion entrante a algun 
 * servidor del modulo. Esta se llama en bg_urc_parsed_callback(urcInfoData_t infoUrc) al detectar y parsear un URC INCOMMING
 * y entrega al usuario el serverID y el connectID de la conexion entrante.
 * 
 * En la libreria la funcion por defecto solo imprime el serverID y el connectID de la conexion entrante.
 * 
 * @param serverID 
 * @param connectID 
 */
void bg_incomming_callback(uint8_t serverID, uint8_t connectID);

/**
 * @brief Esta funcion es un callback que notifica al usuario de un mensaje que se recibe en modo transparente.
 * 
 * @param buff puntero a funcion en donde se tiene el mensaje recibido.
 * @param nBytes numero de bytes recibidos.
 */
void bg_callback_receive_TM(uint8_t *buff, uint16_t nBytes);

/**
 * @brief Esta funcion es un callback que notifica al usuario que la conexion en modo transparente se cerro.
 * 
 * La funcion por defecto imprime el mensaje "Conexion remota cerro, fuera de TM\n"
 */
void bg_callback_closed_TM(void);

/**
 * @brief Este callback notifica al usuario que hubo una reactivacion de contexto PDP debido al URC "pdp deact"
 * LA funcion por defecto imprime un mensaje del contexto que se reactivo
 * 
 * @param contextID Es el numero de contexto que se desactivo
 */
void bg_pdp_activation_callback(uint8_t contextID);

/**
 * @brief Esta funcion de callback notifica al usuario que no hay más URC por atender y se esta fuera de modo transparente.
 * Esta funcion se llama en bg_handle(void) y su proposito es recordar al usuario de que tiene posibilidad de
 * entrar en modo transparente.
 * 
 * La funcion por defecto de la libreria verifica que la ultima conexion de modo transparente este abierta y si es asi,
 * procede a activar el modo transparente en esa conexion.
 * 
 * @param connectID Es el connectID de la ultima conexion en modo transparente.
 * 
 * @param statusNoCarrier Notifica el motivo de salida de modo transparente.
 * Si el valor de la variable es BG_TM_NO_CARRIER_SET, quiere decir que fue por desconexion del punto remoto. 
 * 
 */
void bg_callback_TM_Inactive(uint8_t connectID, bg_noCarrierTM_t statusNoCarrier);
//---------------------------Callbacks LIB para MCU end------------------------


//-------------------------------------Funciones de cola----------------------------
/**
 * @brief Esta funcion hacer uso de la cola interna de URC de la libreria y encola mensajes del tipo urcRawData_t
 * 
 * NOTE: La libreria usa esta cola cuando se usan los calbacks por defecto de la libreria bg_callback_urcDetected(urcRawData_t urcData, 
 * bg_urc_parsed_callback(urcInfoData_t infoUrc) y la funcion de manejo de URC bg_handle_urc(void).
 * @param urc Es la variable que contiene el dato/evento a encolar.
 */
void bg_queue_put(urcRawData_t urc);

/**
 * @brief Esta funcion hacer uso de la cola interna de URC de la libreria y desencola mensajes del tipo urcRawData_t
 * 
 * @param urc Es un puntero a la variable en donde se quiere copiar el mensaje/evento desencolado.
 */
void bg_queue_pop(urcRawData_t *urc);

/**
 * @brief Indica si la cola interna esta vacia.
 * 
 * @return uint8_t Devuelve 1 en caso de que este vacia y 0 en caso contrario.
 */
uint8_t bg_queue_is_empty(void);

/**
 * @brief Indica si la cola interna esta llena.
 * 
 * @return uint8_t Devuelve 1 en caso de que este llena y 0 en caso contrario.
 */
uint8_t bg_queue_is_full(void);

//-------------------------------------Funciones de cola end------------------------
#endif /* _BG77_H_ */
