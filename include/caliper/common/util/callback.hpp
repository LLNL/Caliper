// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file callback.hpp
/// \brief Callback manager template class

#ifndef UTIL_CALLBACK_HPP
#define UTIL_CALLBACK_HPP

#include <functional>
#include <vector>

namespace util
{

template<class F>
class callback
{
    std::vector< std::function<F> > mCb;

public:

    void connect(std::function<F> f) {
        mCb.push_back(f);
    } 

    bool empty() const {
        return mCb.empty();
    }

    template<class... Args>
    void operator()(Args&&... a) {
        for ( auto& f : mCb )
            f(std::forward<Args>(a)...);
    }

    template<class Op, class R, class... Args>
    R accumulate(Op op, R init, Args&&... a) {
        for ( auto& f : mCb )
            init = Op(init, f(std::forward<Args>(a)...));

        return init;
    }
};

}

#endif
