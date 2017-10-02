#include "AllocTracker.h"

#include <iostream>

AllocNode* 
AllocNode::insert(Allocation *allocation) {
    if (allocation->start_address < key) {
        if (left)
            return left->insert(allocation);
        else {
            AllocNode *newNode = new AllocNode(allocation, this, nullptr, nullptr, LEFT);
            left = newNode;
            return newNode;
        }
    }
    else if (allocation->start_address > key) {
        if (right)
            return right->insert(allocation);
        else {
            AllocNode *newNode = new AllocNode(allocation, this, nullptr, nullptr, RIGHT);
            right = newNode;
            return newNode;
        }
    }
    else {
        std::cerr << "Duplicate allocation not inserted!" << std::endl;
        return this;
        //delete this->allocation;
        //this->allocation = allocation;
        //return this;
    }
}

AllocNode* 
AllocNode::find_allocation_containing(uint64_t address) {
    if (address < key) {
        if (left)
            return left->find_allocation_containing(address);
        else 
            return nullptr;
    }
    else { // if (allocation.start_address >= key) {
        if (allocation->contains(address))
            return this;
        else if (right)
            return right->find_allocation_containing(address);
        else 
            return nullptr;
    }
}

AllocNode*
AllocNode::findMin() {
    if (left)
        return left->findMin();
    return this;
}

AllocNode*
AllocNode::findMax() {
    if (right)
        return right->findMax();
    return this;
}

void
AllocTree::rotate_left(AllocNode *node) {
    if (node->left) {
        node->left->parent = node->parent;
        node->left->handedness = RIGHT;
    }

    AllocNode *npp = node->parent->parent;
    HAND np_hand = node->parent->handedness;

    node->parent->parent = node;
    node->parent->handedness = LEFT;
    node->parent->right = node->left;

    node->left = node->parent;
    node->parent = npp;
    node->handedness = (npp ? np_hand : NA);
}

void
AllocTree::rotate_right(AllocNode *node) {
    if (node->right) {
        node->right->parent = node->parent;
        node->right->handedness = LEFT;
    }

    AllocNode *npp = node->parent->parent;
    HAND np_hand = node->parent->handedness;

    node->parent->parent = node;
    node->parent->handedness = RIGHT;
    node->parent->left = node->right;

    node->right = node->parent;
    node->parent = npp;
    node->handedness = (npp ? np_hand : NA);
}

void 
AllocTree::splay(AllocNode *node) {

    while (node->parent) {
        if (node->parent->parent == nullptr) {
            // Case: single zig
            if (node->handedness == RIGHT) {
                // Case zig left
                rotate_left(node);
            } else if (node->handedness == LEFT) {
                // Case zig right
                rotate_right(node);
            }
        } else if (node->handedness == RIGHT && node->parent->handedness == RIGHT) {
            // Case zig zig left
            rotate_left(node);
            rotate_left(node);
        } else if (node->handedness == LEFT && node->parent->handedness == LEFT) {
            // Case zig zig right
            rotate_right(node);
            rotate_right(node);
        } else if (node->handedness == RIGHT && node->parent->handedness == LEFT) {
            // Case zig zag left
            rotate_left(node);
            rotate_right(node);
        } else if (node->handedness == LEFT && node->parent->handedness == RIGHT) {
            // Case zig zag right
            rotate_right(node);
            rotate_left(node);
        }
    }

    // Splayed, root is now node
    root = node;
}

AllocNode*
AllocTree::insert(Allocation *allocation) {
    if (root == nullptr) {
        root = new AllocNode(allocation, nullptr, nullptr, nullptr, NA);
    }
    else {
        AllocNode *newNode = root->insert(allocation);
        splay(newNode);
    }
    return root;
}

void
AllocTree::remove(uint64_t start_address) {
    if (root == nullptr) {
        std::cerr << "Tree empty, nothing to remove!" << std::endl;
    }
    else {
        AllocNode *node = root->find_allocation_containing(start_address);
        if (node) {
            splay(node);
            AllocTree leftTree(node->left);
            if (leftTree.root) {
                leftTree.root->parent = nullptr;
                leftTree.root->handedness = NA;
                AllocNode *lMax = leftTree.root->findMax();
                leftTree.splay(lMax);
                root = lMax;
                root->right = node->right;
                if (root->right)
                    root->right->parent = root;
            } else if (node->right) {
                root = node->right;
                root->parent = nullptr;
                root->handedness = NA;
            } else {
                root = nullptr;
            }
            delete node->allocation;
            delete node;
        }
    }
}

Allocation*
AllocTree::find_allocation_containing(uint64_t address) {
    if (root == nullptr) {
        std::cerr << "Tree empty, nothing to find!" << std::endl;
        return nullptr;
    }

    AllocNode *node = root->find_allocation_containing(address);
    if (node) {
        splay(node);
        return node->allocation;
    }

    return nullptr;
}
