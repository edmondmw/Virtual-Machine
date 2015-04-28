#include "VirtualMachine.h"
#include "Machine.h"
/*#define VM_THREAD_STATE_DEAD 	((TVMTreadState) 0x00)
#define VM_THREAD_STATE_RUNNING ((TVMTreadState) 0x01)
#define VM_THREAD_STATE_READY 	((TVMTreadState) 0x02)
#define VM_THREAD_STATE_WAITING ((TVMTreadState) 0x03)
#define VM_TIMEOUT_INFINITE		((TVMTick)0)
#define VM_TIMEOUT_IMMEDIATE	((TVMTick)-1)
#define VMPrint(format, ...)
	VMFilePrint (1, format, ##__VA_ARGS__)
#define VMPrintError (format, ...)
	VMFilePrint (2, format, ##__VA_ARGS__)
#define MachineConetextSave(mcntx) setjmp((mcntx)->DJumpBuffer)
#define MachineConextRestore(mcntx) longjmp((mcntx)->DJumpBuffer, 1)
#define MachineConextSwitch(mcntxold,mcntxnew) if(setjmp((mcntxold)->DJumpBuffer)==0) longjmp((mcntxnew)->DJumpBuffer,1)
*/

extern "C"{
TMVSatus VMStart(int tickms, int machinetickms, int argc, char *argv[])
	{
		typedef void (*TVMMain)(int argc, char *argv[]);
		
		TVMMain VMMain;
		VMMain = VMLoadModule(argv[1]);	
	}	
}
