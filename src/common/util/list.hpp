/// @file tree.hpp
/// @brief Intrusive tree template class

#ifndef UTIL_LIST_HPP
#define UTIL_LIST_HPP

#include <iterator>

namespace util
{

template<typename T> 
class IntrusiveList {
public:

    struct Node {
        T* next;         // next sibling
        T* prev;         // prev sibling
        Node()
            : next { 0 }, prev { 0 } { }
    };

private:

    // --- private data 

    T* m_me;
    IntrusiveList<T>::Node T::*m_node;

    // --- private methods

    static IntrusiveList<T>::Node& node(T* t, IntrusiveList<T>::Node T::*node) { return (t->*node); }

    IntrusiveList<T>        tree(T* t) const { return IntrusiveList<T>(t, m_node); }
    IntrusiveList<T>::Node& node(T* t) const { return node(t, m_node); }

public:


    IntrusiveList(T* me, IntrusiveList<T>::Node T::*nodeptr) 
        : m_me(me), m_node(nodeptr)
    { }

    T* root() const {
        T* r = m_me;

        while ( r && node(r).prev   )
            r = node(r).prev;

        return r;
    }

    T* prev()                     { return node(m_me).prev; }   
    T* next()                     { return node(m_me).next; }

    const T* prev() const         { return node(m_me).prev; }   
    const T* next() const         { return node(m_me).next; }

    void insert(T* ins) {
        // assert( node != 0 );
        // assert( node->*m_node->m_parent == 0 );

        ins->unlink();

        IntrusiveList<T>::Node& n = node(m_me);

        if (n.next)
            node(n.next).prev = ins;

        node(ins).prev = m_me;
        n.next = ins;
    }

    void unlink() {
        IntrusiveList<T>::Node n = node(m_me);

        // unlink from sibling list
        if (n.prev)
            node(n.prev).next = n.next;
        if (n.next)
            node(n.next).prev = n.prev;

        n.next = 0;
        n.prev = 0;
    }


    // 
    // --- Iterators ---------------------------------------------------------
    //

    class iterator : public std::iterator<std::input_iterator_tag, T> {
        T* m_t;
        IntrusiveList<T>::Node T::*m_n;

    public:

        iterator(T* t, IntrusiveList<T>::Node T::*n)
            : m_t(t), m_n(n) { }

        iterator& operator++() {
            m_t = node(m_t, m_n).next;
            return *this;
        }

        iterator operator++(int) { 
            iterator tmp(*this); ++(*this); return tmp; 
        }

        bool operator == (const iterator& rhs) { return m_t == rhs.m_t; }
        bool operator != (const iterator& rhs) { return m_t != rhs.m_t; }
        T&   operator * () { return *m_t; } 
    };

    IntrusiveList<T>::iterator begin() {
        return iterator(root(), m_node);
    }

    IntrusiveList<T>::iterator end() {
        return IntrusiveList<T>::iterator(0, m_node);
    }
};

} // namespace util

#endif
