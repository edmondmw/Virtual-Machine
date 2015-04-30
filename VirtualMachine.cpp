#include "VirtualMachine.h"
#include "Machine.h"
#include <unistd.h>
#include <iostream>
#include <vector>

extern "C"
{    

    typedef struct{
        TVMThreadEntry entry;    //for the tread entry function 
        SMachineContext context;//for the context to switch to/from the thread
        TVMThreadID Thread_ID;    //to hold ID
        TVMThreadPriority ThreadPriority;    //for thread priority
        TVMThreadState ThreadState;    //for thread stack
        TVMMemorySize MemorySize;    //for stack size
        uint8_t *BaseStack;        //pointer for base of stack
        void *ThreadParameter;    // for thread entry parameter
        TVMTick ticks;            // for the ticcks that thread needs to wait
    }TCB; 

    volatile TVMTick GlobalTick;

    std::vector<TCB*> ThreadIDVector;

    TVMMainEntry VMLoadModule(const char *module);

    void AlarmRequestCallback(void *var)
    {
        GlobalTick--;
    }

    TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[])
	{    
        //declare it
		TVMMainEntry MainEntry;

        //load the module
		MainEntry = VMLoadModule(argv[0]);	

        MachineInitialize(machinetickms);
        MachineRequestAlarm(tickms*1000,(TMachineAlarmCallback)AlarmRequestCallback,NULL);

        //if valid address
        if(MainEntry != NULL)
        {
            MainEntry(argc, argv);
            return VM_STATUS_SUCCESS;
        }
        else
        {
            return VM_STATUS_FAILURE;
        }

	}

    TVMStatus VMThreadSleep(TVMTick tick)
    {
        if(tick == VM_TIMEOUT_INFINITE)
        {
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        //add if VM_TIMEOUT_ERROR

        GlobalTick = tick;

        //thread is sleeping until globalTick is set to zero
        while(0 != GlobalTick)
        {    
        }  
        return VM_STATUS_SUCCESS;
    }


    TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
    {
        if(data == NULL || length == NULL)
        {
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        else if(write(filedescriptor, data, *length) < 0) 
        {
            return VM_STATUS_FAILURE;
        }

        else
        {
            return VM_STATUS_SUCCESS;
        }
    }    

    TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
    {
        if(tid == NULL || entry == NULL)
        {
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);
        TCB ATCB;
        TCB *OneTCB = &ATCB;
        std::cout<<ThreadIDVector.size()<<std::endl;
        //thread id is equal to the size of the vector so it can be added to the end
        OneTCB->Thread_ID = ThreadIDVector.size();
        std::cout<<"after set tcb tid"<<std::endl;
        *tid = OneTCB->Thread_ID;
        std::cout<<"after tid"<<std::endl;
        OneTCB->entry = entry;
        std::cout<<"before param"<<std::endl;
        OneTCB->ThreadParameter = param;
        std::cout<<"after param"<<std::endl;
        OneTCB->MemorySize = memsize;
        OneTCB->ThreadPriority = prio;
        OneTCB->ThreadState = VM_THREAD_STATE_DEAD;
        std::cout<<"BEFORE new"<<std::endl;
        OneTCB->BaseStack = new uint8_t[memsize];
        std::cout<<"AFTER new"<< std::endl;
        ThreadIDVector.push_back(OneTCB);
        std::cout<<"after push"<<std::endl;
        MachineResumeSignals(&OldState);
        std::cout<<"after resume"<<std::endl;
        return VM_STATUS_SUCCESS;
    }
}
