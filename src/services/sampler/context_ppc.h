#pragma once

#include <ucontext.h>

#define _PPC_REG_PC 32

#if __WORDSIZE == 32
#define CALI_SAMPLER_GET_PC(ctx) ( ((ucontext_t*) (ctx))->uc_mcontext.uc_regs->gregs[_PPC_REG_PC] )
#else
#define CALI_SAMPLER_GET_PC(ctx) ( ((ucontext_t*) (ctx))->uc_mcontext.gp_regs[_PPC_REG_PC] )
#endif
