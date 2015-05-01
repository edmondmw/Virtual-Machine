#include "VirtualMachine.h"
#include "Machine.h"
#include <unistd.h>
#include <iostream>
#include <vector>
#include <queue>

using namespace std;

extern "C"
{    

    typedef struct
    {
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
    volatile TVMThreadID CurrentThreadIndex;

    vector<TCB*> ThreadIDVector;
    queue<TCB*> LowQueue;
    queue<TCB*> NormalQueue;
    queue<TCB*> HighQueue;
    queue<TCB*> WaitingQueue;


    TVMMainEntry VMLoadModule(const char *module);

    void AlarmRequestCallback(void *var)
    {
        GlobalTick--;
    }

    void IdleEntry(void *param)
    {
        while(true)
        {
        }
    }

    void SkeletonFunction(void *param)
    {
        MachineEnableSignals();
        ThreadIDVector[CurrentThreadIndex]->entry(ThreadIDVector[CurrentThreadIndex]->ThreadParameter);
        VMThreadTerminate(CurrentThreadIndex);
    }


    void PlaceIntoQueue(TVMThreadID thread)
    {
        if(ThreadIDVector[thread]->ThreadState == VM_THREAD_STATE_READY)
        {
            switch(ThreadIDVector[thread]->ThreadPriority)
            {
                case VM_THREAD_PRIORITY_LOW:    
                    LowQueue.push(ThreadIDVector[thread]);
                    break;
                case VM_THREAD_PRIORITY_NORMAL:
                    NormalQueue.push(ThreadIDVector[thread]);
                    break;
                case VM_THREAD_PRIORITY_HIGH:
                    HighQueue.push(ThreadIDVector[thread]);
                    break;
            }
        }

        else if(ThreadIDVector[thread]->ThreadState == VM_THREAD_STATE_WAITING)
        {
            WaitingQueue.push(ThreadIDVector[thread]);
        }

    }

    void scheduler()
    {
        TVMThreadID tid;
        TVMThreadID Original = CurrentThreadIndex;

        if(ThreadIDVector[CurrentThreadIndex]->ThreadState == VM_THREAD_STATE_RUNNING)
        {
            if(!HighQueue.empty()&&(VM_THREAD_PRIORITY_HIGH > ThreadIDVector[CurrentThreadIndex]->ThreadPriority))
            {
                tid = HighQueue.front()->Thread_ID;
                HighQueue.pop();
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                ThreadIDVector[Original]->ThreadState = VM_THREAD_STATE_READY;
                PlaceIntoQueue(Original);
                MachineContextSwitch(&ThreadIDVector[Original]->context,&ThreadIDVector[tid]->context);

            }
            else if(!NormalQueue.empty()&&(VM_THREAD_PRIORITY_NORMAL > ThreadIDVector[CurrentThreadIndex]->ThreadPriority))
            {
                tid = NormalQueue.front()->Thread_ID;
                NormalQueue.pop();
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                ThreadIDVector[Original]->ThreadState = VM_THREAD_STATE_READY;
                PlaceIntoQueue(Original);
                MachineContextSwitch(&ThreadIDVector[Original]->context,&ThreadIDVector[tid]->context);

            }

            else if(!LowQueue.empty()&&(VM_THREAD_PRIORITY_LOW > ThreadIDVector[CurrentThreadIndex]->ThreadPriority))
            {
                tid = LowQueue.front()->Thread_ID;
                LowQueue.pop();
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                ThreadIDVector[Original]->ThreadState = VM_THREAD_STATE_READY;
                PlaceIntoQueue(Original);
                MachineContextSwitch(&ThreadIDVector[Original]->context,&ThreadIDVector[tid]->context);

            }
            /* for idle
            else
            {
                CurrentThreadIndex = 1;
                ThreadIDVector[1]->ThreadState = VM_THREAD_STATE_RUNNING;
                MachineContextSwitch(ThreadIDVector[Original]->context, ThreadIDVector[1]->context);
            }*/
        }    

        //current thread is not running, so just find the highest priority thread
        else
        {
            if(!HighQueue.empty())
            {
                tid = HighQueue.front()->Thread_ID;
                HighQueue.pop();
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                MachineContextSwitch(&ThreadIDVector[Original]->context,&ThreadIDVector[tid]->context);

            }
            else if(!NormalQueue.empty())
            {
                tid = NormalQueue.front()->Thread_ID;
                NormalQueue.pop();
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                MachineContextSwitch(&ThreadIDVector[Original]->context,&ThreadIDVector[tid]->context);

            }

            else if(!LowQueue.empty())
            {
                tid = LowQueue.front()->Thread_ID;
                LowQueue.pop();
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                MachineContextSwitch(&ThreadIDVector[Original]->context,&ThreadIDVector[tid]->context);

            }

            else
            {
                CurrentThreadIndex = 1;
                ThreadIDVector[1]->ThreadState = VM_THREAD_STATE_RUNNING;
                MachineContextSwitch(&ThreadIDVector[Original]->context, &ThreadIDVector[1]->context);
            }
        }
    }


    TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[])
	{    
        //declare it
		TVMMainEntry VMMain;

        //load the module
		VMMain = VMLoadModule(argv[0]);	

        MachineInitialize(machinetickms);
        MachineRequestAlarm(tickms*1000,(TMachineAlarmCallback)AlarmRequestCallback,NULL);

        //create main thread
        TCB *MainThread = new TCB;
        MainThread->Thread_ID = 0;
        MainThread->ThreadPriority = VM_THREAD_PRIORITY_NORMAL;
        MainThread->ThreadState = VM_THREAD_STATE_RUNNING;
        ThreadIDVector.push_back(MainThread);
        CurrentThreadIndex = MainThread->Thread_ID;

        TCB *IdleThread = new TCB;
        IdleThread->entry = IdleEntry;
        IdleThread->Thread_ID = 1;
        IdleThread->ThreadState = VM_THREAD_STATE_READY;
        IdleThread->ThreadPriority = 0x00;
        IdleThread->MemorySize = 0x100000;
        IdleThread->BaseStack = new uint8_t[IdleThread->MemorySize];
        ThreadIDVector.push_back(IdleThread);


        //if valid address
        if(VMMain != NULL)
        {
            VMMain(argc, argv);
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
        //thread id is equal to the size of the vector so it can be added to the end
        OneTCB->Thread_ID = ThreadIDVector.size();
        *tid = OneTCB->Thread_ID;
        OneTCB->entry = entry;
        OneTCB->ThreadParameter = param;
        OneTCB->MemorySize = memsize;
        OneTCB->ThreadPriority = prio;
        OneTCB->ThreadState = VM_THREAD_STATE_DEAD;
        OneTCB->BaseStack = new uint8_t[memsize];
        ThreadIDVector.push_back(OneTCB);
        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;
    }

    TVMStatus VMThreadActivate(TVMThreadID thread)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        MachineContextCreate(&(ThreadIDVector[thread]->context), SkeletonFunction, ThreadIDVector[thread]->ThreadParameter,ThreadIDVector[thread]->BaseStack, ThreadIDVector[thread]->MemorySize);

        ThreadIDVector[thread]->ThreadState = VM_THREAD_STATE_READY;

        //switch statement to determine which priority queue to place thread into
        PlaceIntoQueue(thread);

        scheduler();

        MachineResumeSignals(&OldState);

        return VM_STATUS_SUCCESS;
    }

    

    TVMStatus VMThreadTerminate(TVMThreadID thread)
    {
        if(thread >= ThreadIDVector.size() || thread < 0)
        {
            return VM_STATUS_ERROR_INVALID_ID;
        }
        if(ThreadIDVector[thread]->ThreadState == VM_THREAD_STATE_DEAD)
        {
            return VM_STATUS_ERROR_INVALID_STATE;
        }

        ThreadIDVector[thread]->ThreadState = VM_THREAD_STATE_DEAD;
        return VM_STATUS_SUCCESS;

    }

    TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
    {
        if(thread >= ThreadIDVector.size()|| thread<0)
        {
            return VM_STATUS_ERROR_INVALID_ID;
        }

        if(stateref == NULL)
        {
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        stateref = &(ThreadIDVector[thread]->ThreadState);
        return VM_STATUS_SUCCESS;

    }


}
