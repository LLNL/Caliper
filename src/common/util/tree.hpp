/// @file tree.hpp
/// @brief Intrusive tree template class

#ifndef UTIL_TREE_HPP
#define UTIL_TREE_HPP

#include <iterator>
#include <queue>

namespace util
{

template<typename T> 
class IntrusiveTree {
public:

    struct Node {
        T* parent;
        T* child_head;
        T* child_tail;
        T* next;         // next sibling
        T* prev;         // prev sibling
        Node()
            : parent(0), child_head(0), child_tail(0), next(0), prev(0) { }
    };

private:

    // --- private data 

    T* m_me;
    IntrusiveTree<T>::Node T::*m_node;

    // --- private methods

    static IntrusiveTree<T>::Node& node(T* t, IntrusiveTree<T>::Node T::*node) { return (t->*node); }

    IntrusiveTree<T>        tree(T* t) const { return IntrusiveTree<T>(t, m_node); }
    IntrusiveTree<T>::Node& node(T* t) const { return node(t, m_node); }

public:


    IntrusiveTree(T* me, IntrusiveTree<T>::Node T::*nodeptr) 
        : m_me(me), m_node(nodeptr)
    { }

    T* root() const {
        T* r = m_me;

        while ( r && node(r).parent )
            r = node(r).parent;
        while ( r && node(r).prev   )
            r = node(r).prev;

        return r;
    }

    T* parent()       const { return node(m_me).parent;     }
    T* first_child()  const { return node(m_me).child_head; }
    T* next_sibling() const { return node(m_me).next;       }

    void unlink_subtree(T* subtree) {
        if (!subtree)
            return;

        IntrusiveTree<T>::Node& n = node(m_me);
        IntrusiveTree<T>::Node& s = node(subtree);

        if (subtree == n.child_head)
            n.child_head = s.next;
        if (subtree == n.child_tail)
            n.child_tail = s.prev;

        if (s.prev)
            node(s.prev).next = s.next;
        if (s.next)
            node(s.next).prev = s.prev;

        s.parent = 0;
        s.next   = 0;
        s.prev   = 0;
    }

    void append(T* sub) {
        // assert( node != 0 );
        // assert( node->*m_node->m_parent == 0 );

        IntrusiveTree<T>::Node& n = node(m_me);

        if (!n.child_head)
            n.child_head = sub;
        if (n.child_tail) {
            node(n.child_tail).next = sub;
            node(sub).prev = n.child_tail;
        }

        node(sub).parent = m_me;

        for ( ; node(sub).next ; sub = node(sub).next )
            node(sub).parent = m_me;

        n.child_tail = sub;
    }

    void unlink() {
        IntrusiveTree<T>::Node n = node(m_me);
        T* parent = n.parent;

        if (parent) {
            tree(parent).unlink_subtree(m_me);

            if (n.child_head)
                tree(parent).append(n.child_head);
        } else {
            // unlink from sibling list
            if (n.prev)
                node(n.prev).next = n.next;
            if (n.next)
                node(n.next).prev = n.prev;

            n.next = 0;
            n.prev = 0;
        }
    }


    // 
    // --- Iterators ---------------------------------------------------------
    //

    class depthfirst_iterator : public std::iterator<std::input_iterator_tag, T> {
        T* m_t;
        IntrusiveTree<T>::Node T::*m_n;

    public:

        depthfirst_iterator(T* t, IntrusiveTree<T>::Node T::*n)
            : m_t(t), m_n(n) { }

        depthfirst_iterator& operator++() {
            if (node(m_t, m_n).child_head)
                m_t = node(m_t, m_n).child_head;
            else if (node(m_t, m_n).next)
                m_t = node(m_t, m_n).next;
            else {
                // find first parent node with a sibling
                while (node(m_t, m_n).parent && !node(m_t, m_n).next)
                    m_t = node(m_t, m_n).parent;

                m_t = node(m_t, m_n).next;    
            }

            return *this;
        }

        depthfirst_iterator operator++(int) { 
            depthfirst_iterator tmp(*this); ++(*this); return tmp; 
        }

        bool operator == (const depthfirst_iterator& rhs) { return m_t == rhs.m_t; }
        bool operator != (const depthfirst_iterator& rhs) { return m_t != rhs.m_t; }
        T&   operator * () { return *m_t; } 
    };

    IntrusiveTree<T>::depthfirst_iterator begin() {
        return depthfirst_iterator(root(), m_node);
    }

    IntrusiveTree<T>::depthfirst_iterator end() {
        return IntrusiveTree<T>::depthfirst_iterator(0, m_node);
    }


    class breadthfirst_iterator : public std::iterator<std::input_iterator_tag, T> {
        T* m_t;
        IntrusiveTree<T>::Node T::*m_n;
        std::queue< T* > m_q;

    public:

        breadthfirst_iterator(T* t, IntrusiveTree<T>::Node T::*node)
            : m_t(t), m_n(node) { }

        breadthfirst_iterator& operator++() {
            if (node(m_t, m_n).child_head)
                m_q.push(node(m_t, m_n).child_head);

            if (node(m_t, m_n).next)
                m_t = node(m_t, m_n).next;
            else if (!m_q.empty()) {
                m_t = m_q.front();
                m_q.pop();
            } else {
                m_t = 0;
            }

            return *this;
        }

        breadthfirst_iterator operator++(int) { 
            breadthfirst_iterator tmp(*this); ++(*this); return tmp; 
        }

        bool operator == (const breadthfirst_iterator& rhs) { return m_t == rhs.m_t; }
        bool operator != (const breadthfirst_iterator& rhs) { return m_t != rhs.m_t; }
        T&   operator * () { return *m_t; } 
    };

    IntrusiveTree<T>::breadthfirst_iterator begin_breadthfirst() {
        return breadthfirst_iterator(root(), m_node);
    }

    IntrusiveTree<T>::breadthfirst_iterator end_breadthfirst() {
        return IntrusiveTree<T>::breadthfirst_iterator(0, m_node);
    }
};

} // namespace util

#endif
