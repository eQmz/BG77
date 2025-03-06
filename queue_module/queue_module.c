#include "../../BG77/queue_module/queue_module.h"


void print_queue(qData_t data)
{
    //LOG_QUEUE("dequeue: ");
    switch (data.type)
    {
    case T_INT:
        LOG_QUEUE("%d\n", data.data.i);
        break;

    case T_CHAR:
        LOG_QUEUE("%c\n", data.data.c);
        break;

    case T_FLOAT:
        LOG_QUEUE("%.2f\n", data.data.f);
        break;

    case T_U8T:
        LOG_QUEUE("%d\n", data.data.u8);
        break;

     case T_U16T:
        LOG_QUEUE("%ld\n", data.data.u16);
        break;

    case T_U32T:
        LOG_QUEUE("%ld\n", data.data.u32);
        break;

    case T_STRING:
        LOG_QUEUE("%s\n", data.data.str);
        break;

    case T_BG_URC:
        LOG_QUEUE("\nUrcBuff: %s\nUrcLen:%ld\nUrcType:%d\n", data.data.urc.buff,\
             data.data.urc.len, data.data.urc.type);
        break;
    
    default:
        LOG_QUEUE("UNSUPPORTED\n");
        break;
    }
}

void create_queue(Cola_t *cola)
{
    cola->init_queue = inicializarCola; 
    cola->is_empty = estaVacia;
    cola->is_full = estaLlena;
    cola->put = enqueue;
    cola->pop = dequeue;
    cola->peek = frente;

    cola->init_queue(cola);
}

static void inicializarCola(Cola_t *cola)
{
    cola->front = cola->rear = -1;
    for(int i = 0; i < MAX_ELEM_QUEUE; i++)
    {
        cola->elem[i].data.u32 = 0;
        cola->elem[i].type = T_UNSUPPORTED;
    }   
}

static int estaVacia(Cola_t *cola)
{
    return (cola->front == -1) ? 1 : 0;
}

static int estaLlena(Cola_t *cola)
{
    return (cola->front == ((cola->rear + 1) % MAX_ELEM_QUEUE)) ? 1 : 0;
}

static int enqueue(Cola_t *cola, qData_t valor)
{
    if(cola->is_full(cola))
    {
        LOG_QUEUE("The queue is full\n");
        return -1;
    }

    if(cola->is_empty(cola))
         cola->front = 0;
    
    cola->rear = (cola->rear + 1) % MAX_ELEM_QUEUE;

    cola->elem[cola->rear] = valor;

    LOG_QUEUE("Inserting: ");
    print_queue(valor);
    return 0;
}

static qData_t dequeue(Cola_t *cola)
{
    qData_t valor = {.data = 0, .type = T_UNSUPPORTED};

    if(estaVacia(cola))
    {
        LOG_QUEUE("The queue is empty\n");
        return valor;
    }

    valor = cola->elem[cola->front];
    
    if(cola->front == cola->rear)
        cola->init_queue(cola);

    else
        cola->front = (cola->front + 1) % MAX_ELEM_QUEUE;

    return valor;
}

static qData_t frente(Cola_t *cola)
{
    qData_t valor = {.data = 0, .type = T_UNSUPPORTED};
    if(cola->is_empty(cola))
    {
        LOG_QUEUE("The queue is empty\n");
        return valor;
    }

    return cola->elem[cola->front];
}

