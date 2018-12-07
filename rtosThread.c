#include "thread.h"

#ifdef FREE_RTOS

typedef portSTACK_TYPE StackType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;
typedef unsigned int uint32_t;


#define semBINARY_SEMAPHORE_QUEUE_LENGTH	( ( uint8_t ) 1U )
#define semSEMAPHORE_QUEUE_ITEM_LENGTH		( ( uint8_t ) 0U )
#define semGIVE_BLOCK_TIME					( ( TickType_t ) 0U )
	typedef uint32_t TickType_t;

#define queueQUEUE_TYPE_MUTEX 				( ( uint8_t ) 1U )
#define xSemaphoreCreateMutex() xQueueCreateMutex( queueQUEUE_TYPE_MUTEX )
QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType );

#define xSemaphoreTake( xSemaphore, xBlockTime )		xQueueGenericReceive( ( QueueHandle_t ) ( xSemaphore ), NULL, ( xBlockTime ), pdFALSE )
BaseType_t xQueueGenericReceive( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait, const BaseType_t xJustPeeking );

#define xSemaphoreGive( xSemaphore )		xQueueGenericSend( ( QueueHandle_t ) ( xSemaphore ), NULL, semGIVE_BLOCK_TIME, queueSEND_TO_BACK )
BaseType_t xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition );

#define vSemaphoreDelete( xSemaphore ) vQueueDelete( ( QueueHandle_t ) ( xSemaphore ) )
void vQueueDelete( QueueHandle_t xQueue );



int thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
	return thrd_success;
}

int thrd_detach(thrd_t *thr)
{
	return thrd_success;
}

int thrd_join(thrd_t thr, int *res)
{
	return thrd_success;
}

int mtx_init(mtx_t *mtx, int type)
{
	*mtx = xSemaphoreCreateMutex();
	return thrd_success;
}

void mtx_destroy(mtx_t *mtx)
{
	vSemaphoreDelete(*mtx);
}

int mtx_lock(mtx_t *mtx)
{
	xSemaphoreTake(*mtx, portMAX_DELAY);
	return thrd_success;
}

int mtx_unlock(mtx_t *mtx)
{
	xSemaphoreGive(*mtx);
	return thrd_success;
}

int cnd_init(cnd_t *cond)
{
	return thrd_success;
}

void cnd_destroy(cnd_t *cond)
{
}

int cnd_signal(cnd_t *cond)
{
	return thrd_success;
}

int cnd_broadcast(cnd_t *cond)
{
	return thrd_success;
}

int cnd_wait(cnd_t *cond, mtx_t *mtx)
{
	return thrd_success;
}

#endif

