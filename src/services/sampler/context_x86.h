#pragma once

#include <ucontext.h>

#define CALI_SAMPLER_GET_PC(ctx) ((ucontext_t*) (ctx))->uc_mcontext.gregs[REG_RIP]
