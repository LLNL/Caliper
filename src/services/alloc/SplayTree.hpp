// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#pragma once

namespace util
{

template<class T, class Cmp>
class SplayTree {
    enum HAND {
        LEFT = -1, NA = 0, RIGHT = 1
    };

    struct STNode {
        STNode* parent;
        STNode* left;
        STNode* right;

        T       val;

        HAND    handedness;

        STNode(STNode* p, const T& v, HAND h)
            : parent(p), left(nullptr), right(nullptr), val(v), handedness(h)
        { }

        STNode* insert(const T& v) {
            Cmp cmp;
            if (cmp(v, val) < 0) {
                if (left)
                    return left->insert(v);
                else {
                    left  = new STNode(this, v, LEFT);
                    return left;
                }   
            } else if (cmp(v, val) > 0) {
                if (right)
                    return right->insert(v);
                else {
                    right = new STNode(this, v, RIGHT);
                    return right;
                }
            } else {
                val = v;
                return this;
            }
        }

        template<class ThreeWayPred> 
        STNode* find(const ThreeWayPred& p) {
            int res = p(val);

            if      (res < 0)
                return left  ? left->find(p)  : nullptr;
            else if (res > 0) 
                return right ? right->find(p) : nullptr;

            return this;
        }

        STNode* findMax() {
            if (right)
                return right->findMax();
            return this;
        }
    };

    STNode* m_root;

    void rotate_left(STNode *node) {
        if (node->left) {
            node->left->parent = node->parent;
            node->left->handedness = RIGHT;
        }

        STNode *npp = node->parent->parent;
        HAND np_hand = node->parent->handedness;

        node->parent->parent = node;
        node->parent->handedness = LEFT;
        node->parent->right = node->left;

        node->left = node->parent;
        node->parent = npp;
        node->handedness = (npp ? np_hand : NA);
    }

    void rotate_right(STNode *node) {
        if (node->right) {
            node->right->parent = node->parent;
            node->right->handedness = LEFT;
        }

        STNode *npp = node->parent->parent;
        HAND np_hand = node->parent->handedness;

        node->parent->parent = node;
        node->parent->handedness = RIGHT;
        node->parent->left = node->right;

        node->right = node->parent;
        node->parent = npp;
        node->handedness = (npp ? np_hand : NA);
    }

    void splay(STNode *node) {
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
        m_root = node;
    }

    SplayTree(STNode* p)
        : m_root(p)
    { }
    
public:
    
    constexpr SplayTree()
        : m_root(nullptr)
    { }

    ~SplayTree()
    { }

    void insert(const T& v) {
        if (!m_root)
            m_root = new STNode(nullptr, v, NA);
        else {
            STNode* node = m_root->insert(v);
            splay(node);
        }
    }

    void remove(SplayTree<T,Cmp>& ref) {
        STNode* node = ref.m_root;

        splay(node);

        SplayTree<T,Cmp> ltree(node->left);
        
        if (ltree) {
            ltree.m_root->parent = nullptr;
            ltree.m_root->handedness = NA;
            STNode *lMax = ltree.m_root->findMax();
            ltree.splay(lMax);
            m_root = lMax;
            m_root->right = node->right;
            if (m_root->right)
                m_root->right->parent = m_root;
        } else if (node->right) {
            m_root = node->right;
            m_root->parent = nullptr;
            m_root->handedness = NA;
        } else {
            m_root = nullptr;
        }

        delete node;
    }

    template<class ThreeWayPred>
    SplayTree<T, Cmp> find(const ThreeWayPred& p) {
        STNode* ret = nullptr;

        if (m_root)
            ret = m_root->find(p);
        
        return SplayTree<T,Cmp>(ret);
    }

    T& operator * () {
        return m_root->val;
    }

    operator bool () const {
        return m_root != nullptr;
    }
};

}
