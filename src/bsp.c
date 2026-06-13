#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <hermit/bsp.h>
#include <hermit/output.h>
#include <hermit/view.h>
#include <hermit/workspace.h>
#include <hermit/server.h>
#include <hermit/logger.h>

struct hermit_bsp_node *bsp_leaf_create(struct hermit_view *view) {
    struct hermit_bsp_node *node = calloc(1, sizeof(*node));
    if (!node) return NULL;
    node->type        = BSP_LEAF;
    node->leaf.view   = view;
    view->bsp_node    = node;
    return node;
}

struct hermit_bsp_node *bsp_split_create(struct hermit_bsp_node *left, struct hermit_bsp_node *right,
                                         enum bsp_split_dir dir, float ratio) {
    struct hermit_bsp_node *node = calloc(1, sizeof(*node));
    if (!node) return NULL;
    node->type        = BSP_SPLIT;
    node->split.dir   = dir;
    node->split.ratio = ratio; // <-- FIX: Initialize the ratio property!
    node->split.left  = left;
    node->split.right = right;
    left->parent      = node;
    right->parent     = node;
    return node;                                             
}

void bsp_destroy(struct hermit_bsp_node *node) {
    if (!node) return;
    if (node->type == BSP_SPLIT) {
        bsp_destroy(node->split.left);
        bsp_destroy(node->split.right);
    } else {
        if (node->leaf.view)
            node->leaf.view->bsp_node = NULL;
    }
    free(node);
}

void bsp_apply_layout(struct hermit_bsp_node *node, struct wlr_box box, int gaps_inner, int gaps_outer, bool is_root) {
    if (!node) return;

    if (is_root) {
        box.x      += gaps_outer;
        box.y      += gaps_outer;
        box.width  -= gaps_outer * 2;
        box.height -= gaps_outer * 2;
    }

    node->box = box;
    
    if (node->type == BSP_LEAF) {
        struct hermit_view *view = node->leaf.view;
        if (!view || !view->xdg_toplevel) return;
        
        if (box.width < 50)  box.width = 50;
        if (box.height < 50) box.height = 50;

        wlr_xdg_toplevel_set_size(view->xdg_toplevel, box.width, box.height);
        wlr_scene_node_set_position(&view->scene_tree->node, box.x, box.y);
        return;
    }

    struct wlr_box left_box = box;
    struct wlr_box right_box = box;
    int gap = gaps_inner / 2;

    if (node->split.dir == BSP_SPLIT_HORZ) {
        int split = (int)(box.width * node->split.ratio);
        
        if (split - gap < 50) split = 50 + gap;
        if (box.width - split - gap < 50) split = box.width - 50 - gap;

        left_box.width        = split - gap;
        right_box.x           = box.x + split + gap;
        right_box.width       = box.width - split - gap;
    } else {
        int split = (int)(box.height * node->split.ratio);
        
        if (split - gap < 50) split = 50 + gap;
        if (box.height - split - gap < 50) split = box.height - 50 - gap;

        left_box.height       = split - gap;
        right_box.y           = box.y + split + gap;
        right_box.height      = box.height - split - gap;
    }

    bsp_apply_layout(node->split.left, left_box, gaps_inner, gaps_outer, false);
    bsp_apply_layout(node->split.right, right_box, gaps_inner, gaps_outer, false);
}

struct hermit_bsp_node *bsp_find_leaf(struct hermit_bsp_node *root, struct hermit_view *view) {
    if (!root) return NULL;
    if (root->type == BSP_LEAF)
        return root->leaf.view == view ? root : NULL;
    struct hermit_bsp_node *found = bsp_find_leaf(root->split.left, view);
    return found ? found : bsp_find_leaf(root->split.right, view);
}

struct hermit_bsp_node *bsp_find_neighbor(struct hermit_bsp_node *node, enum bsp_split_dir dir, bool forward) {
    if (!node || !node->parent) return NULL;
    
    struct hermit_bsp_node *child = node;
    struct hermit_bsp_node *parent = node->parent;
    
    while (parent) {
        if (parent->split.dir == dir) {
            if (forward && parent->split.left == child) {
                struct hermit_bsp_node *n = parent->split.right;
                while (n->type == BSP_SPLIT) n = n->split.left;
                return n;
            }
            if (!forward && parent->split.right == child) {
                struct hermit_bsp_node *n = parent->split.left;
                while (n->type == BSP_SPLIT) n = n->split.right;
                return n;
            }
        }
        child = parent;
        parent = parent->parent;
    }
    return NULL;
}

void bsp_insert(struct hermit_workspace *ws, struct hermit_view *view) {
    struct hermit_bsp_node *new_leaf = bsp_leaf_create(view);
    struct hermit_server *server = ws->output->server;
    
    if (!ws->bsp_root) {
        ws->bsp_root = new_leaf;
        
        struct wlr_box output_box;
        wlr_output_layout_get_box(server->output_layout, ws->output->wlr_output, &output_box);
        bsp_apply_layout(ws->bsp_root, output_box, server->config->gaps_inner, server->config->gaps_outer, true);
        return;
    }
    
    struct hermit_bsp_node *target = NULL;
    if (server->focused_view && server->focused_view != view)
        target = bsp_find_leaf(ws->bsp_root, server->focused_view);
    if (!target)
        target = ws->bsp_root;
        
    int depth = 0;
    struct hermit_bsp_node *p = target->parent;
    while (p) { depth++; p = p->parent; }
    enum bsp_split_dir dir = (depth % 2 == 0) ? BSP_SPLIT_HORZ : BSP_SPLIT_VERT;
    float ratio = server->config->split_ratio;
    
    struct hermit_bsp_node *old_parent = target->parent;
    struct hermit_bsp_node *split = bsp_split_create(target, new_leaf, dir, ratio);
    
    split->parent = old_parent;
    if (!old_parent) {
        ws->bsp_root = split;
    } else {
        if (old_parent->split.left == target)
            old_parent->split.left = split;
        else
            old_parent->split.right = split;
    }

    struct wlr_box output_box;
    wlr_output_layout_get_box(server->output_layout, ws->output->wlr_output, &output_box);
    bsp_apply_layout(ws->bsp_root, output_box, server->config->gaps_inner, server->config->gaps_outer, true);
}

void bsp_remove(struct hermit_workspace *ws, struct hermit_view *view) {
    struct hermit_bsp_node *leaf = bsp_find_leaf(ws->bsp_root, view);
    if (!leaf) return;
    
    view->bsp_node = NULL;
    
    if (!leaf->parent) {
        free(leaf);
        ws->bsp_root = NULL;
        return;
    }
    
    struct hermit_bsp_node *parent = leaf->parent;
    struct hermit_bsp_node *sibling = (parent->split.left == leaf) ? parent->split.right : parent->split.left;
    
    sibling->parent = parent->parent;
    if (!parent->parent) {
        ws->bsp_root = sibling;
    } else {
        if (parent->parent->split.left == parent)
            parent->parent->split.left = sibling;
        else
            parent->parent->split.right = sibling;
    }
    free(leaf);
    free(parent);

    // Refresh layout for remaining windows
    if (ws->bsp_root) {
        struct wlr_box output_box;
        wlr_output_layout_get_box(ws->output->server->output_layout, ws->output->wlr_output, &output_box);
        bsp_apply_layout(ws->bsp_root, output_box, ws->output->server->config->gaps_inner, ws->output->server->config->gaps_outer, true);
    }
}

void bsp_swap(struct hermit_bsp_node *a, struct hermit_bsp_node *b) {
    if (!a || !b || a == b) return;
    struct hermit_view *tmp = a->leaf.view;
    a->leaf.view = b->leaf.view;
    b->leaf.view = tmp;
    if (a->leaf.view) a->leaf.view->bsp_node = a;
    if (b->leaf.view) b->leaf.view->bsp_node = b;
}

void bsp_resize(struct hermit_bsp_node *node, enum bsp_split_dir dir, float delta) {
    if (!node) return;
    struct hermit_bsp_node *parent = node->parent;
    while (parent) {
        if (parent->split.dir == dir) {
            parent->split.ratio += delta;
            if (parent->split.ratio < 0.1f) parent->split.ratio = 0.1f;
            if (parent->split.ratio > 0.9f) parent->split.ratio = 0.9f;
            return;
        }
        parent = parent->parent;
    }
}

void bsp_build_from_workspace(struct hermit_workspace *ws, float ratio) {
    struct hermit_view *view;
    wl_list_for_each(view, &ws->views, link) {
        view->float_box.x = view->scene_tree->node.x;
        view->float_box.y = view->scene_tree->node.y;
        wlr_surface_get_extents(view->xdg_toplevel->base->surface, &view->float_box);
        bsp_insert(ws, view);
    }
}

void bsp_clear(struct hermit_workspace *ws) {
    bsp_destroy(ws->bsp_root);
    ws->bsp_root = NULL;
}