#include "graphics/render_queue.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"

RenderQueue g_default_render_queue = {0};

void render_queue_init(RenderQueue *queue) {
    if (!queue) return;
    queue->head = NULL;
    queue->tail = NULL;
    queue->total_commands = 0;
}

void render_queue_destroy(RenderQueue *queue) {
    if (!queue) return;
    render_queue_clear(queue);
}

RenderCommand* render_queue_push(RenderQueue *queue) {
    if (!queue) return NULL;
    
    if (!queue->tail || queue->tail->count >= CHUNK_SIZE) {
        RenderQueueChunk *new_chunk = kmalloc(sizeof(RenderQueueChunk));
        if (!new_chunk) return NULL; /* OOM */
        
        memset(new_chunk, 0, sizeof(RenderQueueChunk));
        
        if (!queue->head) {
            queue->head = new_chunk;
            queue->tail = new_chunk;
        } else {
            queue->tail->next = new_chunk;
            queue->tail = new_chunk;
        }
    }
    
    RenderCommand *cmd = &queue->tail->commands[queue->tail->count++];
    queue->total_commands++;
    
    // Clear the command
    memset(cmd, 0, sizeof(RenderCommand));
    return cmd;
}

void render_queue_clear(RenderQueue *queue) {
    if (!queue) return;
    
    RenderQueueChunk *curr = queue->head;
    while (curr) {
        RenderQueueChunk *next = curr->next;
        kfree(curr);
        curr = next;
    }
    
    queue->head = NULL;
    queue->tail = NULL;
    queue->total_commands = 0;
}
