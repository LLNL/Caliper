/// @file atomic-tree.hpp
/// @brief Intrusive tree template class

#include <atomic>
#include <iterator>

namespace util
{

template<typename T> 
class AtomicIntrusiveTree {
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
    AtomicIntrusiveTree<T>::Node T::*m_node;

    // --- private methods

    static AtomicIntrusiveTree<T>::Node& node(T* t, AtomicIntrusiveTree<T>::Node T::*node) { return (t->*node); }

    AtomicIntrusiveTree<T>        tree(T* t) const { return AtomicIntrusiveTree<T>(t, m_node); }
    AtomicIntrusiveTree<T>::Node& node(T* t) const { return node(t, m_node); }

public:


    AtomicIntrusiveTree(T* me, AtomicIntrusiveTree<T>::Node T::*nodeptr) 
        : m_me(me), m_node(nodeptr)
    { }

    T* parent()       const { return node(m_me).parent;     }
    T* first_child()  const { return node(m_me).head.load(std::memory_order_relaxed); }
    T* next_sibling() const { return node(m_me).next;       }

    void append(T* sub) {
        AtomicIntrusiveTree<T>::Node& n = node(m_me);
        
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
        AtomicIntrusiveTree<T>::Node T::*m_n;

    public:

        depthfirst_iterator(T* t, AtomicIntrusiveTree<T>::Node T::*n)
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

    AtomicIntrusiveTree<T>::depthfirst_iterator begin() {
        return depthfirst_iterator(m_me, m_node);
    }

    AtomicIntrusiveTree<T>::depthfirst_iterator end() {
        return AtomicIntrusiveTree<T>::depthfirst_iterator(0, m_node);
    }
};

} // namespace util
