#include "avl.h"
#include <cassert>
#include <stdint.h>

static uint32_t max(uint32_t lhs, uint32_t rhs) {
  return lhs < rhs ? rhs : lhs;
}

static void avl_update(AVLNode *node) {
  node->height =
      1 + max(avl_height(node->left), avl_height(node->right));
  node->cnt = 1 + avl_cnt(node->left) + avl_cnt(node->right);
}

//   B                                D
// ┌─┴───┐      rotate-left       ┌───┴─┐
// a ┆   D     ─────────────►     B   ┆ e
// ┆ ┆ ┌─┴─┐   ◄─────────────   ┌─┴─┐ ┆ ┆
// ┆ ┆ c ┆ e    rotate-right    a ┆ c ┆ ┆
// ┆ ┆ ┆ ┆ ┆                    ┆ ┆ ┆ ┆ ┆
// a B c D e                    a B c D e

static AVLNode *rot_left(AVLNode *node) {
  AVLNode *parent = node->parent;  // null
  AVLNode *new_node = node->right; // D
  AVLNode *inner = new_node->left; // C

  // node <-> inner
  node->right = inner; // D change to C
  if (inner) {
    inner->parent = node; // B become C's parent
  }
  // parent <- new_node
  new_node->parent = parent; // null become D's parent
  // new_node <-> node
  new_node->left = node;   // D change to B
  node->parent = new_node; // D become B's parent
  // auxiliary data
  avl_update(node);
  avl_update(new_node);
  return new_node;
}

static AVLNode *rot_right(AVLNode *node) {
  AVLNode *parent = node->parent;
  AVLNode *new_node = node->left;
  AVLNode *inner = new_node->right;

  node->left = inner;
  if (inner) {
    inner->parent = node;
  }
  new_node->parent = parent;
  new_node->right = node;
  node->parent = new_node;

  avl_update(node);
  avl_update(new_node);

  return new_node;
}

// the left substree is taller by 2
static AVLNode *avl_fix_left(AVLNode *node) {
  if (avl_height(node->left->left) < avl_height(node->left->right)) {
    node->left = rot_left(node->left);
  }
  return rot_right(node);
}

static AVLNode *avl_fix_right(AVLNode *node) {
  if (avl_height(node->right->right) < avl_height(node->right->left)) {
    node->right = rot_right(node->right);
  }
  return rot_left(node);
}

AVLNode *avl_fix(AVLNode *node) {
  while (true) {
    AVLNode **from = &node; // save the fixed subtree here
    AVLNode *parent = node->parent;
    if (parent) {
      from = parent->left == node ? &parent->left : &parent->right;
    }

    // auxiliary data
    avl_update(node);
    // fix the height difference of 2
    uint32_t l = avl_height(node->left);
    uint32_t r = avl_height(node->right);

    if (l == r + 2) {
      *from = avl_fix_left(node);
    } else if (l + 2 == r) {
      *from = avl_fix_right(node);
    }

    // root node, stop
    if (!parent) {
      return *from;
    }

    node = parent;
  }
}

static AVLNode* avl_del_easy(AVLNode* node){
  assert(!node->left || !node->right); // at most one child
  AVLNode *child = node->left ? node->left : node->right;
  AVLNode* parent = node->parent;
  //update the child's parent pointer
  if(child){
    child->parent = parent;
  }
  // attach the child to the gendparent
  if(!parent){
    return child; // removing the root node
  }
  AVLNode* *from = parent->left == node ? &parent->left : &parent->right;
  *from = child;
  //rebalance the update tree
  return avl_fix(parent);    
}

//detach a node and returns the new root of the tree
AVLNode* avl_del(AVLNode* node){
  // the easy case of 0 or 1 child 
  if(!node->left || !node->right){
    return avl_del_easy(node);
  }
  // find the successor
  AVLNode* victim = node->right;
  while(victim->left){
    victim = victim->left;
  }
  // detach the successor
  AVLNode* root = avl_del_easy(victim);
  // swap with the successor
  // fix the pointers to make the child parent's pointer point to the successor
  *victim = *node; // left, right, parent
  // this approach changes the memory, leading to the need to fix the child's pointer

  if(victim->left){
    victim->left->parent = victim;
  }
  if(victim->right){
    victim->right->parent = victim;
  }
  // attach the successor to the parent. or update the root pointer
  AVLNode* *from = &root;
  AVLNode* parent = node->parent;
  if(parent){
    from = parent->left == node ? &parent->left : &parent->right;
  }
  *from = victim;
  return root;
}


AVLNode* avl_offset(AVLNode* node, int64_t offset){
  int64_t pos = 0; // the rank of difference from the starting node
  while(offset != pos){
    if(pos < offset && pos+avl_cnt(node->right) >= offset){
      // the target is inside the right subtree
      node = node->right;
      pos += avl_cnt(node->left) + 1;
    }
    else if(pos > offset && pos - avl_cnt(node->left) <= offset){
      // the target is inside the left subtree
      node = node->left;
      pos -= avl_cnt(node->right) + 1;
    }
    else{
      // go to the parent
      AVLNode* parent = node->parent;
      if(!parent){
        return nullptr;
      }
      if(parent->right == node){
        pos -= avl_cnt(parent->left)+1;
      }
      else{
        pos += avl_cnt(parent->right)+1;
      }
      node = parent;
    }
  }
  return node;
}