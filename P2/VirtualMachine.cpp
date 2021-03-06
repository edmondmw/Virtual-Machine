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
        int file;
        TVMMutexID MyMutex;
        int MutexPrioIndex;
        int SleepingIndex;
// Possibly hold a list of held mutexes

    }TCB; 

	typedef struct
	{
		TVMMutexID MutexID;
		TVMThreadID OwnerID;
        bool unlocked;
        vector<TCB*> HighPrio;
        vector<TCB*> NormalPrio;
        vector<TCB*> LowPrio;    

	}mutex;

    volatile TVMThreadID CurrentThreadIndex;
	
	vector<mutex*> MutexIDVector;
    vector<TCB*> ThreadIDVector;
    vector<TCB*> LowQueue;
    vector<TCB*> NormalQueue;
    vector<TCB*> HighQueue;
  //  vector<TCB*> WaitingQueue;
    vector<TCB*> SleepingQueue;


    TVMMainEntry VMLoadModule(const char *module);

   

    void IdleEntry(void *param)
    {
        MachineEnableSignals();
        //cerr<<"in idle"<<endl;
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


    void PlaceIntoMutexQueue(TVMThreadID thread, TVMMutexID mutex)
    {
            switch(ThreadIDVector[thread]->ThreadPriority)
            {
                case VM_THREAD_PRIORITY_LOW:    
                //cerr<<"place in low"<<endl;
                    ThreadIDVector[thread]->MutexPrioIndex = MutexIDVector[mutex]->LowPrio.size();
                    MutexIDVector[mutex]->LowPrio.push_back(ThreadIDVector[thread]);
                    break;
                case VM_THREAD_PRIORITY_NORMAL:
                //cerr<<"place into norm"<<endl;
                    ThreadIDVector[thread]->MutexPrioIndex = MutexIDVector[mutex]->NormalPrio.size();
                    MutexIDVector[mutex]->NormalPrio.push_back(ThreadIDVector[thread]);
                    break;
                case VM_THREAD_PRIORITY_HIGH:
                    //cerr<<"place into high"<<endl;
                    ThreadIDVector[thread]->MutexPrioIndex = MutexIDVector[mutex]->HighPrio.size();
                    MutexIDVector[mutex]->HighPrio.push_back(ThreadIDVector[thread]);
                    break;
            }
    }

    void PlaceIntoQueue(TVMThreadID thread)
    {
        if(ThreadIDVector[thread]->ThreadState == VM_THREAD_STATE_READY)
        {
            switch(ThreadIDVector[thread]->ThreadPriority)
            {
                case VM_THREAD_PRIORITY_LOW:    
                    LowQueue.push_back(ThreadIDVector[thread]);
                    break;
                case VM_THREAD_PRIORITY_NORMAL:
                    NormalQueue.push_back(ThreadIDVector[thread]);
                    break;
                case VM_THREAD_PRIORITY_HIGH:
                    HighQueue.push_back(ThreadIDVector[thread]);
                    break;
            }
        }
    }

    void scheduler()
    {
        //cerr<<"in sched"<<endl;
        TVMThreadID tid;
        TVMThreadID Original = CurrentThreadIndex;

        if(ThreadIDVector[CurrentThreadIndex]->ThreadState == VM_THREAD_STATE_RUNNING)
        {
            if(!HighQueue.empty()&&(VM_THREAD_PRIORITY_HIGH > ThreadIDVector[CurrentThreadIndex]->ThreadPriority))
            {
                tid = HighQueue.front()->Thread_ID;
                HighQueue.erase(HighQueue.begin());
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                ThreadIDVector[Original]->ThreadState = VM_THREAD_STATE_READY;
                PlaceIntoQueue(Original);
                //cerr<<"running high"<<endl;

                MachineContextSwitch(&ThreadIDVector[Original]->context,&ThreadIDVector[tid]->context);

            }
            else if(!NormalQueue.empty()&&(VM_THREAD_PRIORITY_NORMAL > ThreadIDVector[CurrentThreadIndex]->ThreadPriority))
            {
                //cerr<<"running n"<<endl;
                tid = NormalQueue.front()->Thread_ID;
                NormalQueue.erase(NormalQueue.begin());
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                ThreadIDVector[Original]->ThreadState = VM_THREAD_STATE_READY;
                PlaceIntoQueue(Original);
                MachineContextSwitch(&(ThreadIDVector[Original]->context),&(ThreadIDVector[tid]->context));

            }

            else if(!LowQueue.empty()&&(VM_THREAD_PRIORITY_LOW > ThreadIDVector[CurrentThreadIndex]->ThreadPriority))
            {
                //cerr<<"running low"<<endl;
                tid = LowQueue.front()->Thread_ID;
                LowQueue.erase(LowQueue.begin());
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                ThreadIDVector[Original]->ThreadState = VM_THREAD_STATE_READY;
                PlaceIntoQueue(Original);
                MachineContextSwitch(&(ThreadIDVector[Original]->context),&(ThreadIDVector[tid]->context));

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
                //cerr<<"high"<<endl;
                tid = HighQueue.front()->Thread_ID;
                HighQueue.erase(HighQueue.begin());
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                MachineContextSwitch(&ThreadIDVector[Original]->context,&ThreadIDVector[tid]->context);

            }
            else if(!NormalQueue.empty())
            {
                //cerr<<"n"<<endl;
                tid = NormalQueue.front()->Thread_ID;
                NormalQueue.erase(NormalQueue.begin());
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                MachineContextSwitch(&ThreadIDVector[Original]->context,&ThreadIDVector[tid]->context);

            }

            else if(!LowQueue.empty())
            {
                //cerr<<"low"<<endl;
                tid = LowQueue.front()->Thread_ID;
                LowQueue.erase(LowQueue.begin());
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                MachineContextSwitch(&ThreadIDVector[Original]->context,&ThreadIDVector[tid]->context);

            }

            else
            {
                //cerr<<"idl"<<endl;
                CurrentThreadIndex = 1;
                ThreadIDVector[1]->ThreadState = VM_THREAD_STATE_RUNNING;
                MachineContextSwitch(&ThreadIDVector[Original]->context, &ThreadIDVector[1]->context);
            }
        }
    }

    void AlarmRequestCallback(void *var)
    {
        for(unsigned  i = 0; i < SleepingQueue.size(); i++)
        {
            SleepingQueue[i]->ticks--;
            //If ticks are at zero remove it from the sleeping queue and place into ready queue
            if(SleepingQueue[i]->ticks == 0)
            {
                SleepingQueue[i]->ThreadState = VM_THREAD_STATE_READY;
                PlaceIntoQueue(SleepingQueue[i]->Thread_ID);
                SleepingQueue.erase(SleepingQueue.begin() + i);
                i--;
            }
        }

      
        //if current thread is running then put it into a ready queue
        if(ThreadIDVector[CurrentThreadIndex]->ThreadState == VM_THREAD_STATE_RUNNING)
        {
            ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_READY;
            PlaceIntoQueue(CurrentThreadIndex);
        }       
        scheduler();
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
        ThreadIDVector.push_back(new TCB);
        ThreadIDVector[0]->Thread_ID = 0;
        ThreadIDVector[0]->ThreadPriority = VM_THREAD_PRIORITY_NORMAL;
        ThreadIDVector[0]->ThreadState = VM_THREAD_STATE_RUNNING;
        
        CurrentThreadIndex = 0;

        ThreadIDVector.push_back(new TCB);
        ThreadIDVector[1]->entry = IdleEntry;
        ThreadIDVector[1]->Thread_ID = 1;
        ThreadIDVector[1]->ThreadState = VM_THREAD_STATE_READY;
        ThreadIDVector[1]->ThreadPriority = 0x00;
        ThreadIDVector[1]->MemorySize = 0x100000;
        ThreadIDVector[1]->BaseStack = new uint8_t[ThreadIDVector[1]->MemorySize];

        MachineContextCreate(&(ThreadIDVector[1]->context), IdleEntry , NULL,ThreadIDVector[1]->BaseStack, ThreadIDVector[1]->MemorySize);


        //if valid address
        if(VMMain != NULL)
        {
            MachineEnableSignals();
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
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(tick == VM_TIMEOUT_INFINITE)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

		if(tick == VM_TIMEOUT_IMMEDIATE)
		{
			ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_READY;
			PlaceIntoQueue(CurrentThreadIndex);
			scheduler();
		}
        //add if VM_TIMEOUT_ERROR
        ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;

        ThreadIDVector[CurrentThreadIndex]->ticks = tick;

        SleepingQueue.push_back(ThreadIDVector[CurrentThreadIndex]);

        scheduler();

        MachineResumeSignals(&OldState);

        return VM_STATUS_SUCCESS;
		
    }


    void FileCallback(void* calldata, int result)
    {    
        TCB* MyTCB = (TCB*)calldata;
        //ThreadIDVector[MyTCB->Thread_ID]->file = result;
        MyTCB->file = result;
        MyTCB->ThreadState = VM_THREAD_STATE_READY;

        PlaceIntoQueue(MyTCB->Thread_ID);
        scheduler();
    }

    TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(data == NULL || length == NULL)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }


        MachineFileWrite(filedescriptor, data, *length, FileCallback, ThreadIDVector[CurrentThreadIndex]);
        ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;
        scheduler();  

        if(ThreadIDVector[CurrentThreadIndex]->file < 0)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_FAILURE;
        }
        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;      

    }    

    TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);
        if(filename == NULL || filedescriptor == NULL)
        {
            //return error
			MachineResumeSignals(&OldState);
			return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        MachineFileOpen(filename, flags, mode, (TMachineFileCallback)FileCallback, ThreadIDVector[CurrentThreadIndex]);

		//return vmstatusfailture if fileopen cant open
		if(ThreadIDVector[CurrentThreadIndex]->file < 0)
		{
			MachineResumeSignals(&OldState);
			return VM_STATUS_FAILURE;
		}

        ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;
        //WaitingQueue.push_back(ThreadIDVector[CurrentThreadIndex]);
        scheduler();

        *filedescriptor = ThreadIDVector[CurrentThreadIndex]->file;
        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;
		
		
    }

    TVMStatus VMFileClose(int filedescriptor)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);


        MachineFileClose(filedescriptor, FileCallback, ThreadIDVector[CurrentThreadIndex]);
        ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;
        scheduler();  

        if(ThreadIDVector[CurrentThreadIndex]->file < 0)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_FAILURE;
        }

        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;      

    }    

    TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(data == NULL || length == NULL)
        {
			MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }


        MachineFileRead(filedescriptor, data, *length, FileCallback, ThreadIDVector[CurrentThreadIndex]);
        ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;
        scheduler();  

        *length = ThreadIDVector[CurrentThreadIndex]->file;

        if(ThreadIDVector[CurrentThreadIndex]->file < 0)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_FAILURE;
        }
        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;      

    }    


    TVMStatus VMFileSeek(int filedescriptor, int offset, int whence,int *newoffset)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        MachineFileSeek(filedescriptor, offset, whence, FileCallback, ThreadIDVector[CurrentThreadIndex]);
        ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;
        scheduler();  

        *newoffset = ThreadIDVector[CurrentThreadIndex]->file;
        if(ThreadIDVector[CurrentThreadIndex]->file < 0)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_FAILURE;
        }
        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;      

    }    

    TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        *tid = ThreadIDVector.size();
        ThreadIDVector.push_back(new TCB);
        //thread id is equal to the size of the vector so it can be added to the end
        ThreadIDVector[*tid]->Thread_ID = *tid;
        ThreadIDVector[*tid]->entry = entry;
        ThreadIDVector[*tid]->ThreadParameter = param;
        ThreadIDVector[*tid]->MemorySize = memsize;
        ThreadIDVector[*tid]->ThreadPriority = prio;
        ThreadIDVector[*tid]->ThreadState = VM_THREAD_STATE_DEAD;
        ThreadIDVector[*tid]->BaseStack = new uint8_t[memsize];

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
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(thread >= ThreadIDVector.size() || thread < 0)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_ID;
        }
        if(ThreadIDVector[thread]->ThreadState == VM_THREAD_STATE_DEAD)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_STATE;
        }

        ThreadIDVector[thread]->ThreadState = VM_THREAD_STATE_DEAD;

        scheduler();
        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;

    }

    TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(thread >= ThreadIDVector.size()|| thread<0)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_ID;
        }

        if(stateref == NULL)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        *stateref = ThreadIDVector[thread]->ThreadState;
     
        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;

    }


    TVMStatus VMThreadDelete(TVMThreadID thread)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);


        //if out of bounds or thread is deleted
        if(thread > (ThreadIDVector.size()-1) || thread < 0 ||ThreadIDVector[thread] == NULL)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_ID;
        }
        //if state is dead delete the thread
        if(ThreadIDVector[thread]->ThreadState == VM_THREAD_STATE_DEAD)
        {
            ThreadIDVector[thread] = NULL;
            MachineResumeSignals(&OldState);
            return VM_STATUS_SUCCESS;
        }
        //state is not dead
        else
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_STATE;
        }

    }

    TVMStatus VMThreadID(TVMThreadIDRef threadref)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(threadref == NULL)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        *threadref =  ThreadIDVector[CurrentThreadIndex]->Thread_ID;
        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;


    }



	TVMStatus VMMutexCreate(TVMMutexIDRef mutexref)
	{
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

		if(mutexref == NULL)
		{
			return VM_STATUS_ERROR_INVALID_PARAMETER;
		}

    	*mutexref =  MutexIDVector.size();



		MutexIDVector.push_back(new mutex);
		
		MutexIDVector[*mutexref]->MutexID = MutexIDVector.size()-1;
        MutexIDVector[*mutexref]->unlocked = true;

        MachineResumeSignals(&OldState);
		return VM_STATUS_SUCCESS;
	}

	TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout)
	{
	    TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);
       
        //cerr<<"called acquire: "<< mutex<<endl;

        if(mutex >= MutexIDVector.size()||mutex < 0)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_ID;
        }

        if(timeout == VM_TIMEOUT_IMMEDIATE)
        {
            if(MutexIDVector[mutex]->OwnerID < 0)
            {
                MutexIDVector[mutex]->OwnerID = CurrentThreadIndex;
                MachineResumeSignals(&OldState);
                return VM_STATUS_SUCCESS;
              // ThreadIDVector[CurrentThreadIndex]->ThreadState =VM_THREAD_STATE_READY;
                //PlaceIntoQueue(CurrentThreadIndex);
                //scheduler();
            }
            else
            {
                MachineResumeSignals(&OldState);
                return VM_STATUS_FAILURE;
            }
        }
        //mutex available
        if(MutexIDVector[mutex]->unlocked)
        {
            MutexIDVector[mutex]->OwnerID = CurrentThreadIndex;
            MutexIDVector[mutex]->unlocked =false;
        }
        //mutex unavailable place in waiting
        else
        {
            //cerr<<"locked"<<endl;
            ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;
            ThreadIDVector[CurrentThreadIndex]->ticks = timeout;
            SleepingQueue.push_back(ThreadIDVector[CurrentThreadIndex]);
            PlaceIntoMutexQueue(CurrentThreadIndex,mutex);

            scheduler();
        

           /* if(ThreadIDVector[CurrentThreadIndex]->ticks == 0)
            {    
                cerr<<"in the timeout zone"<<endl;
                switch(ThreadIDVector[CurrentThreadIndex]->ThreadPriority)
                {
                    case VM_THREAD_PRIORITY_LOW:
                        MutexIDVector[mutex]->LowPrio.erase(MutexIDVector[mutex]->LowPrio.begin()+ThreadIDVector[CurrentThreadIndex]->MutexPrioIndex);
                        break;
                    case VM_THREAD_PRIORITY_NORMAL:
                        MutexIDVector[mutex]->NormalPrio.erase(MutexIDVector[mutex]->NormalPrio.begin()+ThreadIDVector[CurrentThreadIndex]->MutexPrioIndex);
                        break;
                    case VM_THREAD_PRIORITY_HIGH:
                        MutexIDVector[mutex]->HighPrio.erase(MutexIDVector[mutex]->HighPrio.begin()+ThreadIDVector[CurrentThreadIndex]->MutexPrioIndex);
                        break;
                }

                MachineResumeSignals(&OldState);
                return VM_STATUS_FAILURE;
            }*/
        }

        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;	
	}

    TVMStatus VMMutexRelease(TVMMutexID mutex)
    {

        //cerr<<"called release "<<mutex<<endl;
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

		if(mutex >= MutexIDVector.size()||mutex < 0)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_ID;
        }

        if(!MutexIDVector[mutex]->HighPrio.empty())
        {
            //cerr<<"high not empty"<<endl;
            MutexIDVector[mutex]->OwnerID = MutexIDVector[mutex]->HighPrio.front()->Thread_ID;
            MutexIDVector[mutex]->HighPrio.erase(MutexIDVector[mutex]->HighPrio.begin());
            ThreadIDVector[MutexIDVector[mutex]->OwnerID]->ThreadState = VM_THREAD_STATE_READY;
            for(unsigned i = 0; i < SleepingQueue.size(); i++)
            {
                if(SleepingQueue[i]->Thread_ID == MutexIDVector[mutex]->OwnerID)
                {
                    SleepingQueue.erase(SleepingQueue.begin()+i);
                    break;
                }
            }
            PlaceIntoQueue(MutexIDVector[mutex]->OwnerID);

            scheduler();
        }
        else if(!MutexIDVector[mutex]->NormalPrio.empty())
        {
            //cerr<<"normal"<<endl;
            MutexIDVector[mutex]->OwnerID = MutexIDVector[mutex]->NormalPrio.front()->Thread_ID;
            MutexIDVector[mutex]->NormalPrio.erase(MutexIDVector[mutex]->NormalPrio.begin());
            ThreadIDVector[MutexIDVector[mutex]->OwnerID]->ThreadState = VM_THREAD_STATE_READY;
            for(unsigned i = 0; i < SleepingQueue.size(); i++)
            {
                if(SleepingQueue[i]->Thread_ID == MutexIDVector[mutex]->OwnerID)
                {
                    SleepingQueue.erase(SleepingQueue.begin()+i);
                    break;
                }
            }
            PlaceIntoQueue(MutexIDVector[mutex]->OwnerID);
            scheduler();
        }
        else if(!MutexIDVector[mutex]->LowPrio.empty())
        {
            //cerr<<"low"<<endl;
            MutexIDVector[mutex]->OwnerID = MutexIDVector[mutex]->LowPrio.front()->Thread_ID;
            MutexIDVector[mutex]->LowPrio.erase(MutexIDVector[mutex]->LowPrio.begin());
            ThreadIDVector[MutexIDVector[mutex]->OwnerID]->ThreadState = VM_THREAD_STATE_READY;
            for(unsigned i = 0; i < SleepingQueue.size(); i++)
            {
                if(SleepingQueue[i]->Thread_ID == MutexIDVector[mutex]->OwnerID)
                {
                    SleepingQueue.erase(SleepingQueue.begin()+i);
                    break;
                }
            }
            PlaceIntoQueue(MutexIDVector[mutex]->OwnerID);
            scheduler();
        }

        else
        {

            //cerr<<"free "<<mutex<<endl;
            MutexIDVector[mutex]->unlocked = true;
            scheduler();
        }

        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;
		
    }

    TVMStatus VMMutexDelete(TVMMutexID mutex)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(mutex < 0|| mutex > MutexIDVector.size()-1)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_ID;
        }

        if(MutexIDVector[mutex]->OwnerID < 0)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_STATE;
        }

        MutexIDVector.erase(MutexIDVector.begin() + mutex);

		MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;
    }

    TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);
        
        if(mutex < 0|| mutex > MutexIDVector.size()-1)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_ID;
        }
		if(MutexIDVector[mutex]->unlocked)
		{
			MachineResumeSignals(&OldState);
			return VM_THREAD_ID_INVALID;
		}
        if(ownerref == NULL)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        *ownerref = MutexIDVector[mutex]->OwnerID;

        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;
    }


}
