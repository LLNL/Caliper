// Copyright (c) 2015-2024, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file AsyncEvent.h
/// %Caliper C++ interface for asynchronous events

#pragma once
#ifndef CALI_ASYNC_EVENT_H
#define CALI_ASYNC_EVENT_H

#include <chrono>
#include <memory>

namespace cali
{

class Node;

class TimedAsyncEvent
{
    using clock = std::chrono::steady_clock;

    Node* end_tree_node_;
    std::chrono::time_point<clock> start_time_;

    explicit TimedAsyncEvent(Node* node) : end_tree_node_ { node }, start_time_ { clock::now() } { }

public:

    constexpr TimedAsyncEvent() : end_tree_node_ { nullptr } { }

    void end();

    static TimedAsyncEvent begin(const char* message);
};

} // namespace cali


#endif
