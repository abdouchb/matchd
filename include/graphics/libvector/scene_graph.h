#pragma once

#include "graphics/libvector/path.h"
#include "graphics/libvector/vector.h"

typedef struct scene_node {
    struct scene_node *parent;
    struct scene_node *first_child;
    struct scene_node *next_sibling;
    
    transform_t local_transform;
    transform_t global_transform; // cached
    
    path_t *path;
    color_f_t fill_color;
    color_f_t stroke_color;
    float stroke_width;
    bool has_fill;
    bool has_stroke;
    
} scene_node_t;

scene_node_t* scene_node_create(void);
void scene_node_destroy(scene_node_t *node);

void scene_node_add_child(scene_node_t *parent, scene_node_t *child);
void scene_node_update_transforms(scene_node_t *node, const transform_t *parent_transform);

void scene_graph_render(scene_node_t *root);
