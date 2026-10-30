#include "graphics/libvector/scene_graph.h"
#include "graphics/libvector/rasterizer.h"
#include "kernel/heap.h"

scene_node_t* scene_node_create(void) {
    scene_node_t *n = kmalloc(sizeof(scene_node_t));
    if (!n) return NULL;
    
    n->parent = NULL;
    n->first_child = NULL;
    n->next_sibling = NULL;
    
    transform_identity(&n->local_transform);
    transform_identity(&n->global_transform);
    
    n->path = NULL;
    n->has_fill = false;
    n->has_stroke = false;
    n->stroke_width = 1.0f;
    
    return n;
}

void scene_node_destroy(scene_node_t *node) {
    if (!node) return;
    
    scene_node_t *child = node->first_child;
    while (child) {
        scene_node_t *next = child->next_sibling;
        scene_node_destroy(child);
        child = next;
    }
    
    if (node->path) {
        path_destroy(node->path);
    }
    
    kfree(node);
}

void scene_node_add_child(scene_node_t *parent, scene_node_t *child) {
    if (!parent || !child) return;
    
    child->parent = parent;
    
    if (!parent->first_child) {
        parent->first_child = child;
    } else {
        scene_node_t *curr = parent->first_child;
        while (curr->next_sibling) {
            curr = curr->next_sibling;
        }
        curr->next_sibling = child;
    }
}

void scene_node_update_transforms(scene_node_t *node, const transform_t *parent_transform) {
    if (!node) return;
    
    if (parent_transform) {
        transform_multiply(&node->global_transform, parent_transform, &node->local_transform);
    } else {
        node->global_transform = node->local_transform;
    }
    
    scene_node_t *child = node->first_child;
    while (child) {
        scene_node_update_transforms(child, &node->global_transform);
        child = child->next_sibling;
    }
}

void scene_graph_render(scene_node_t *root) {
    if (!root) return;
    
    if (root->path) {
        if (root->has_fill) {
            rasterize_path_fill(root->path, &root->global_transform, root->fill_color);
        }
        if (root->has_stroke) {
            rasterize_path_stroke(root->path, &root->global_transform, root->stroke_color, root->stroke_width);
        }
    }
    
    scene_node_t *child = root->first_child;
    while (child) {
        scene_graph_render(child);
        child = child->next_sibling;
    }
}
