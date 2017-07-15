/// \file lockfree-tree.hpp
/// \brief Intrusive tree template class

#pragma once

#include <atomic>
#include <iterator>

namespace util
{

/// \brief A thread-safe tree class that allows lockfree appending of nodes.

template<typename T> 
class LockfreeIntrusiveTree {
public:

    struct Node {
        T* parent;
        T* next;
        std::atomic<T*> head;

        Node()
            : parent(0), next(0), head(0)
            { }
    };

private:

    // --- private data 

    T* m_me;
    LockfreeIntrusiveTree<T>::Node T::*m_node;

    // --- private methods

    static LockfreeIntrusiveTree<T>::Node& node(T* t, LockfreeIntrusiveTree<T>::Node T::*node) { return (t->*node); }

    LockfreeIntrusiveTree<T>        tree(T* t) const { return LockfreeIntrusiveTree<T>(t, m_node); }
    LockfreeIntrusiveTree<T>::Node& node(T* t) const { return node(t, m_node); }

public:


    LockfreeIntrusiveTree(T* me, LockfreeIntrusiveTree<T>::Node T::*nodeptr) 
        : m_me(me), m_node(nodeptr)
    { }

    T* parent()       const { return node(m_me).parent;     }
    T* first_child()  const { return node(m_me).head.load(std::memory_order_relaxed); }
    T* next_sibling() const { return node(m_me).next;       }

    void append(T* sub) {
        LockfreeIntrusiveTree<T>::Node& n = node(m_me);
        
        node(sub).parent = m_me;

        // do "conservative" loop update instead of 
        //   while(!n.head.compare_exchange_weak(node(sub).next, sub,
        //                                       std::memory_order_release,
        //                                       std::memory_order_relaxed))
        //       ;
        // because that is not safe on some pre-2014 compilers according to
        // http://en.cppreference.com/w/cpp/atomic/atomic/compare_exchange
        
        T* old_head = n.head.load(std::memory_order_relaxed);
        
        do {
            node(sub).next = old_head;
        } while (!n.head.compare_exchange_weak(old_head, sub,
                                               std::memory_order_release,
                                               std::memory_order_relaxed));
    }

    // 
    // --- Iterators ---------------------------------------------------------
    //

    class depthfirst_iterator : public std::iterator<std::input_iterator_tag, T> {
        T* m_t;
        LockfreeIntrusiveTree<T>::Node T::*m_n;

    public:

        depthfirst_iterator(T* t, LockfreeIntrusiveTree<T>::Node T::*n)
            : m_t(t), m_n(n) { }

        depthfirst_iterator& operator++() {
            if (node(m_t, m_n).head)
                m_t = node(m_t, m_n).head;
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

    LockfreeIntrusiveTree<T>::depthfirst_iterator begin() {
        return depthfirst_iterator(m_me, m_node);
    }

    LockfreeIntrusiveTree<T>::depthfirst_iterator end() {
        return LockfreeIntrusiveTree<T>::depthfirst_iterator(0, m_node);
    }
};

} // namespace util
