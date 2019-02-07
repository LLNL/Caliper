#include <stdlib.h>

#include <iostream>
#include <vector>
#include <set>
using namespace std;

#include <Instruction.h>
// #include <Operation.h>
#include <Expression.h>
using namespace Dyninst;
using namespace InstructionAPI;

size_t getReadSize(const Instruction& ins)
{
    // Get all read operands and their expressions
    vector<Operand> opds;
    ins.getOperands(opds);
    set<Expression::Ptr> memAccessors;
    for(int i=0; i<opds.size(); i++)
    {
        if(opds[i].readsMemory())
        {
            opds[i].addEffectiveReadAddresses(memAccessors);
        }
    }

    // Sum up the size of all expressions read
    size_t readsz = 0;
    set<Expression::Ptr>::iterator it;
    for(it=memAccessors.begin(); it!=memAccessors.end(); it++)
    {
        readsz += (*it)->size();
    }
    return readsz;
}

size_t getWriteSize(const Instruction& ins)
{
    // Get all read operands and their expressions
    vector<Operand> opds;
    ins.getOperands(opds);
    set<Expression::Ptr> memAccessors;
    for(int i=0; i<opds.size(); i++)
    {
        if(opds[i].readsMemory())
        {
            opds[i].addEffectiveWriteAddresses(memAccessors);
        }
    }

    // Sum up the size of all expressions read
    size_t writesz = 0;
    set<Expression::Ptr>::iterator it;
    for(it=memAccessors.begin(); it!=memAccessors.end(); it++)
    {
        writesz += (*it)->size();
    }
    return writesz;
}

