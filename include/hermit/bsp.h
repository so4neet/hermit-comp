#pragma once

#include <stdbool.h>
#include <wlr/types/wlr_output_layout.h>

struct hermit_view;
struct hermit_workspace;
struct hermit_window;
struct hermit_server;

enum bsp_node_type {
    BSP_LEAF,
    BSP_SPLIT,
};

enum bsp_split_dir {
    BSP_SPLIT_HORZ,
    BSP_SPLIT_VERT,
};

struct hermit_bsp_node {
    enum bsp_node_type      type;
    struct hermit_bsp_node *parent;
    struct wlr_box          box;
    
    union {
        struct {
            enum bsp_split_dir          dir;
            float                       ratio;
            struct hermit_bsp_node      *left;
            struct hermit_bsp_node      *right;
        } split;
        struct {
            struct hermit_view          *view;
        } leaf;
    };
};

struct hermit_bsp_node *bsp_leaf_create(struct hermit_view *view);
struct hermit_bsp_node *bsp_split_create(struct hermit_bsp_node *left, struct hermit_bsp_node *right, enum bsp_split_dir dir, float ratio);
void bsp_destroy(struct hermit_bsp_node *node);
void bsp_insert(struct hermit_workspace *ws, struct hermit_view *view);
void bsp_remove(struct hermit_workspace *ws, struct hermit_view *view);
void bsp_apply_layout(struct hermit_bsp_node *node, struct wlr_box box, int gaps_inner, int gaps_outer, bool is_root);
struct hermit_bsp_node *bsp_find_leaf(struct hermit_bsp_node *root, struct hermit_view *view);
struct hermit_bsp_node *bsp_find_neighbor(struct hermit_bsp_node *node, enum bsp_split_dir dir, bool forward);
void bsp_swap(struct hermit_bsp_node *a, struct hermit_bsp_node *b);
void bsp_resize(struct hermit_bsp_node *node, enum bsp_split_dir dir, float delta);
void bsp_build_from_workspace(struct hermit_workspace *ws, float ratio);
void bsp_clear(struct hermit_workspace *ws);