#include "libhtml/dom.h"
#include "kernel/heap.h"
#include "zarqaa/string.h"

dom_node_t *dom_create_node(tag_type_t type) {
    dom_node_t *node = (dom_node_t *)kmalloc(sizeof(dom_node_t));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(dom_node_t));
    node->type = type;
    
    /* Default style */
    node->style.text_color = 0xFFFFFFFF; /* White */
    node->style.bg_color = 0x00000000;   /* Transparent */
    
    return node;
}

void dom_add_child(dom_node_t *parent, dom_node_t *child) {
    if (!parent || !child) return;
    if (parent->child_count >= 64) return; /* Limit reached */
    
    parent->children[parent->child_count++] = child;
    child->parent = parent;
}

void dom_free(dom_node_t *root) {
    if (!root) return;
    for (int i = 0; i < root->child_count; i++) {
        dom_free(root->children[i]);
    }
    kfree(root);
}
