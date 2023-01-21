// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file lockfree-tree.hpp
/// \brief Intrusive tree template class

#pragma once

#include <atomic>
#include <iterator>

namespace util
{

/// \brief A thread-safe, lock-free, intrusive tree data structure.
///
/// Implements a tree data structure. Each tree node has a list of
/// child nodes, and links to its parent node and its next sibling.
///
/// This is an \e intrusive data structure, i.e., the tree node
/// elements are members of the host data structure. Hosts must add a
/// \c LockfreeIntrusiveTree<Host>::Node element and point a tree
/// object to it.  The host can be part of multiple independent trees.
/// Example:
///
/// \code
/// struct Host {
///     LockfreeIntrusiveTree<Host>::Node tree_a;
///     LockfreeIntrusiveTree<Host>::Node tree_b;
///     // other host data structure members
/// };
///
/// // use the tree:
/// Host root, a_child, b_child;
/// LockfreeIntrusiveTree<Host>(&root, &Host::tree_a).append(&a_child);
/// LockfreeIntrusiveTree<Host>(&root, &Host::tree_b).append(&b_child);
/// \endcode
///
/// The tree can be safely used from multiple threads without locking
/// (however, note that this applies only to the tree structure
/// itself; other data elements in the host structure are not
/// protected).  Because of its lock-free nature, there are
/// restrictions on the operations that can be
/// performed. Specifically, tree nodes can only be added, but not
/// moved or removed.

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

        while(!n.head.compare_exchange_weak(node(sub).next, sub,
                                            std::memory_order_release,
                                            std::memory_order_relaxed))
            ;
    }
};

} // namespace util
