#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

#include "FreeRTOS.h"

struct StaticQueue_t
{
    std::mutex mutex;
    std::uint8_t* storage = nullptr;
    UBaseType_t capacity = 0;
    UBaseType_t count = 0;
    UBaseType_t head = 0;
    std::size_t item_size = 0;
};
using QueueHandle_t = StaticQueue_t*;
inline void (*host_queue_before_send)() = nullptr;

inline QueueHandle_t xQueueCreateStatic(UBaseType_t capacity, UBaseType_t item_size, std::uint8_t* storage,
                                        StaticQueue_t* queue)
{
    queue->storage = storage;
    queue->capacity = capacity;
    queue->item_size = item_size;
    return queue;
}

inline BaseType_t xQueueReset(QueueHandle_t queue)
{
    std::lock_guard lock(queue->mutex);
    queue->count = 0;
    queue->head = 0;
    return pdPASS;
}

inline BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t wait)
{
    if (wait != 0)
    {
        return pdFAIL;
    }
    if (host_queue_before_send != nullptr)
    {
        const auto hook = host_queue_before_send;
        host_queue_before_send = nullptr;
        hook();
    }
    std::lock_guard lock(queue->mutex);
    if (queue->count == queue->capacity)
    {
        return pdFAIL;
    }
    const auto tail = (queue->head + queue->count) % queue->capacity;
    std::memcpy(queue->storage + tail * queue->item_size, item, queue->item_size);
    ++queue->count;
    return pdPASS;
}

inline BaseType_t xQueueReceive(QueueHandle_t queue, void* item, TickType_t wait)
{
    std::lock_guard lock(queue->mutex);
    if (wait != 0 || queue->count == 0)
    {
        return pdFAIL;
    }
    std::memcpy(item, queue->storage + queue->head * queue->item_size, queue->item_size);
    queue->head = (queue->head + 1) % queue->capacity;
    --queue->count;
    return pdPASS;
}
