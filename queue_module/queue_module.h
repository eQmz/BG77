#ifndef QUEUE_MODULE_H
#define QUEUE_MODULE_H

/**
 * @file queue_module.h
 * @author E_MtSnZ 
 * @brief This library contains a basic queque structure which has its own methods and constructor
 * @version 1.0
 * @date 2024-10-15
 * @code
    #include "queue_module.h"

    int main(int argc, char const *argv[])
    {
        Cola_t cola;
        create_queue(&cola);

        qData_t item[6] = {{.data.i = 7, .type = T_INT}, {.data.c = 'Q', .type = T_CHAR}, \
        {.data.f = 10.7, .type = T_FLOAT}, {.data.u8 = 254, .type = T_U8T},\
        {.data.str = "hola", .type = T_STRING},  {.data.u16 = 1024, .type = T_U16T}};

        for(int i = 0; i < 6; i++)
            cola.put(&cola, item[i]);
        
        for(int i = 0; i < 6; i++)
        {
            printf("Dequeued: " );
            print_queue(cola.pop(&cola));
        }

        return 0;
    }
    
    Output
    Inserting: 7
    Inserting: Q
    Inserting: 10.70
    Inserting: 254
    Inserting: hola
    The queue is full
    Dequeued: 7
    Dequeued: Q
    Dequeued: 10.70
    Dequeued: 254
    Dequeued: hola
    Dequeued: The queue is empty
    UNSUPPORTED
 * @endcode
 * @copyright Copyright (c) 2024
 * 
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdint.h"


//#define _QUEUE_DEBUG_

#ifdef _QUEUE_DEBUG_
#define LOG_QUEUE(...)\
    printf(__VA_ARGS__)
#else
#define LOG_QUEUE(...)
#endif

/**
 * @brief This MACRO is the max number of elements in the queue
 * 
 */
#define MAX_ELEM_QUEUE 20 // Tamaño máximo de la cola

/**
 * @brief This MACRO is the size of array type char
 * 
 */
#define SIZE_STR 10 

/**
 * @brief This MACRO is the size of the URC buffer 
 * 
 */
#define SIZE_URC_BUFF 1024

/** 
 * @brief This is the types enum definition for identify each item of the queue
 */
typedef enum
{
    T_INT,
    T_CHAR,
    T_FLOAT,
    T_U8T,
    T_U16T,
    T_U32T,
    T_STRING,
    T_BG_URC,
    T_UNSUPPORTED
}typeData_t;

typedef struct{
    uint8_t buff[SIZE_URC_BUFF];
    uint16_t len;
    uint8_t type;
}urcRawData_t;

/** 
 * @brief This is the union definition to create a data variable to support different data types
 */
typedef union
{
    int i;
    char c;
    float f;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    char str[SIZE_STR];
    urcRawData_t urc;
}varData_t;

/**
 * @brief This is the struct definition to define the type of item data for the queue
 * 
 */
typedef struct 
{
    /**
     * @brief this is the type of the item data
     * 
     */
    typeData_t type;

    /**
     * @brief this is the variable to allocate the value of item data
     * 
     */
    varData_t data;
}qData_t;

/**
 * @brief This is the queue typedef
 * 
 */
typedef struct Cola Cola_t;

/**
 * @brief This is the definition for a function pointer with Cola_t parameter and return of void type
 * 
 */
typedef void(* tf0_t)(Cola_t *cola);

/**
 * @brief This is the definition for a function pointer with Cola_t parameter and return of int type
 * 
 */
typedef int(* tf1_t)(Cola_t *cola);

/**
 * @brief This is the definition for a function pointer with Cola_t and qData_t parameters and return of int type
 * 
 */
typedef int(* tf2_t)(Cola_t *cola, qData_t valor);

/**
 * @brief This is the definition for a function pointer with Cola_t parameter and return of qData_t type
 * 
 */
typedef qData_t(* tf3_t)(Cola_t *cola);


/**
 * @brief This is the queue structure definition
 * 
 */
struct Cola {

    /**
     * @brief elements array of the queue
     * 
     */
    qData_t elem[MAX_ELEM_QUEUE];

    /**
     * @brief front of the queue
     * 
     */
    int8_t front;

    /**
     * @brief rear or end of the queue
     * 
     */
    int8_t rear;

    /**
     * @brief Initializes the attributes of the queue instance
     * 
     * @param cola This is the queue instance
    */
    tf0_t init_queue; 

    /**
     * @brief Checks if the queue is empty
     * 
     * @param cola The queue instance
     * @return int if the queue is empty returns 1 otherwise returns 0
     */
    tf1_t is_empty;

    /**
     * @brief Checks if the queue is full
     * 
     * @param cola The queue instance
     * @return int if the queue is full returns 1 otherwise returns 0
     */
    tf1_t is_full;

    /**
     * @brief Puts a new item or element in the queue
     * 
     * @param cola queue instance
     * @param valor item value
     * @return int if the queue is full returns 1 otherwise returns 0
     */
    tf2_t put;

    /**
     * @brief pulls an item from the queue
     * 
     * @param cola queue instance
     * @return int if the queue is empty returns -1 otherwise returns the item value
     */
    tf3_t pop;
    
    /**
     * @brief Shows the last item of the queue and it does not pull the last item from the queue
     * 
     * @param cola queue instance
     * @return int if the queue is empty returns -1 otherwise returns the item value
     */
    tf3_t peek;
};

/**
 * @brief Initializes the attributes of the queue instance
 * 
 * @param cola This is the queue instance
 */
static void inicializarCola(Cola_t *cola);

/**
 * @brief Checks if the queue is empty
 * 
 * @param cola The queue instance
 * @return int if the queue is empty returns 1 otherwise returns 0
 */
static int estaVacia(Cola_t *cola);

/**
 * @brief Checks if the queue is full
 * 
 * @param cola The queue instance
 * @return int if the queue is full returns 1 otherwise returns 0
 */
static int estaLlena(Cola_t *cola);

/**
 * @brief Puts a new item or element in the queue
 * 
 * @param cola queue instance
 * @param valor item value
 * @return int if the queue is full returns 1 otherwise returns 0
 */
static int enqueue(Cola_t *cola, qData_t valor);

/**
 * @brief pulls an item from the queue
 * 
 * @param cola queue instance
 * @return int if the queue is empty returns -1 otherwise returns the item value
 */
static qData_t dequeue(Cola_t *cola);

/**
 * @brief Shows the last item of the queue and it does not pull the last item from the queue
 * 
 * @param cola queue instance
 * @return int if the queue is empty returns -1 otherwise returns the item value
 */
static qData_t frente(Cola_t *cola);

/**
 * @brief This is the constructor that initializes the methods pointers and the queue attributes
 * 
 * @param cola queue instance
 */
void create_queue(Cola_t *cola);

/**
 * @brief Prints an item data of type qData_t
 * 
 */
void print_queue(qData_t data);

#endif // QUEUE_MODULE_H
