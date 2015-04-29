#include "VirtualMachine.h"
#include "Machine.h"
#include <unistd.h>
#include <iostream>
/*#define VM_THREAD_STATE_DEAD 	((TVMTreadState) 0x00)
#define VM_THREAD_STATE_RUNNING ((TVMTreadState) 0x01)
#define VM_THREAD_STATE_READY 	((TVMTreadState) 0x02)
#define VM_THREAD_STATE_WAITING ((TVMTreadState) 0x03)
extern "C"{
#define VM_TIMEOUT_INFINITE        ((TVMTick)0)
#define VM_TIMEOUT_IMMEDIATE    ((TVMTick)-1)
#define VMPrint(format, ...)
    VMFilePrint (1, format, ##__VA_ARGS__)
#define VMPrintError (format, ...)
    VMFilePrint (2, format, ##__VA_ARGS__)
#define MachineConetextSave(mcntx) setjmp((mcntx)->DJumpBuffer)
#define MachineConextRestore(mcntx) longjmp((mcntx)->DJumpBuffer, 1)
#define MachineConextSwitch(mcntxold,mcntxnew) if(setjmp((mcntxold)->DJumpBuffer)==0) longjmp((mcntxnew)->DJumpBuffer,1)
*/
extern "C"
{    
    typedef struct{
        TVMThreadEntry entry;    //for the tread entry function 
        SMachineContext context;//for the context to switch to/from the thread
        TVMThreadID thread_ID;    //to hold ID
        TVMThreadPriority threadPriority;    //for thread priority
        TVMThreadState threadState;    //for thread stack
        TVMMemorySize memorySize;    //for stack size
        uint8_t *baseStack;        //pointer for base of stack
        void *threadParameter;    // for thread entry parameter
        TVMTick ticks;            // for the ticcks that thread needs to wait
    }TCB; 

    volatile TVMTick globalTick;

    TVMMainEntry VMLoadModule(const char *module);

    void AlarmRequestCallback(void *var)
    {
        globalTick--;
        //std::cout<<"global tick: "<<globalTick<<std::endl;
    }

    TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[])
	{    
        //declare it
		TVMMainEntry mainEntry;

        //load the module
		mainEntry = VMLoadModule(argv[0]);	

        MachineInitialize(machinetickms);
        MachineRequestAlarm(tickms*1000,(TMachineAlarmCallback)AlarmRequestCallback,NULL);

        //if valid address
        if(mainEntry != NULL)
        {
            mainEntry(argc, argv);
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

        globalTick = tick;

        //thread is sleeping until globalTick is set to zero
        while(0 != globalTick)
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
}
