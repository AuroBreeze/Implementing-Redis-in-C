
#include <stdint.h>
struct AVLNode{
    AVLNode* parent = nullptr;
    AVLNode* left = nullptr;
    AVLNode* right = nullptr;
    uint32_t height = 0; //subtree height
    uint32_t cnt = 0; // subtree size
};

inline void avl_init(AVLNode* node){
    node->left = nullptr;
    node->right = nullptr;
    node->parent = nullptr;
    node->height = 1;
    node->cnt = 1;
}

//helps
inline uint32_t avl_height(AVLNode* node){
    return node ? node->height : 0;
}

inline uint32_t avl_cnt(AVLNode* node){
    return node ? node->cnt : 0;
}

//API
AVLNode* avl_fix(AVLNode* node);
AVLNode* avl_del(AVLNode* node);