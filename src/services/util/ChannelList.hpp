// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

#pragma once

#include <caliper/Caliper.h>

namespace util
{

//
//   ChannelList implements a simple linked list of Channels for
// function wrapper services. It is used in cases where we need
// a global / static list of channels in a service module, and we
// can't rely on complex types or STL containers because these
// are destroyed in arbitrary order during program
// finalization.
//
//   This must remain a POD type!
//
struct ChannelList {
    cali::Channel* channel;
    
    ChannelList* next;
    ChannelList* prev;

    inline void unlink() {
        if (next)
            next->prev = prev;
        if (prev)
            prev->next = next;
    }

    inline static void add(ChannelList** head, cali::Channel* chn) {
        ChannelList* ret = new ChannelList { chn, nullptr, nullptr };

        if (*head)
            (*head)->prev = ret;

        ret->next = *head;
        *head = ret;
    }

    inline static void remove(ChannelList** head, cali::Channel* chn) {
        ChannelList* p = *head;
        
        while (p && p->channel->id() != chn->id())
            p = p->next;

        if (p) {
            if (p == *head)
                *head = p->next;

            p->unlink();
            delete p;
        }        
    }
};

} // namespace util
