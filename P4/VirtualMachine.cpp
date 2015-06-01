#include "VirtualMachine.h"
#include "Machine.h"
#include <unistd.h>
#include <iostream>
#include <vector>
#include <queue>
#include <list>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

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
        TVMMemorySize memsize;
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

    typedef struct 
    {
        uint8_t *address;
        TVMMemorySize length; 
    }block;

    typedef struct 
    {

        TVMMemorySize MemoryPoolSize;
        TVMMemoryPoolID PoolID;
        //pointer to the base of memory array
        uint8_t* base;
        int length;
        //make free space stuff
        TVMMemorySize FreeSpace;
        list<block*> FreeList;
        list<block*> AllocatedList;

    }MemoryPool;

    const TVMMemoryPoolID VM_MEMORY_POOL_ID_SYSTEM = 0;

    const TVMMemoryPoolID VM_MEMORY_POOL_ID_SHARED = 1;

    const TVMMemorySize MACHINE_MEMORY_LIMIT = 512;

    volatile TVMThreadID CurrentThreadIndex;

    int GlobalValue;    //filecallback result

    vector<uint16_t> FATVector;
    //vector<uint8_t> RootVector;
    vector<MemoryPool*> MemoryIDVector;
	vector<mutex*> MutexIDVector;
    vector<TCB*> ThreadIDVector;
    vector<TCB*> LowQueue;
    vector<TCB*> NormalQueue;
    vector<TCB*> HighQueue;
    vector<TCB*> WaitingQueue;
    vector<TCB*> SleepingQueue;
    vector<TCB*> HighWaitQueue;
    vector<TCB*> NormalWaitQueue;
    vector<TCB*> LowWaitQueue;

    TVMMainEntry VMLoadModule(const char *module);
    void scheduler();


   void FileCallback(void* calldata, int result);

   typedef struct 
   {
        string name;
        uint16_t BytePerSector;
        uint16_t SectorsPerCluster;
        uint16_t ReservedSectors;
        uint16_t FATCount;
        uint16_t RootEntry;
        uint16_t SectorCount16;
        uint16_t Media;
        uint16_t FATSize16;
        uint16_t SectorPerTrack;
        uint16_t HeadCount;
        uint32_t HiddenSectorCount;
        uint32_t SectorCount32;
        uint16_t DriveNumer;
        uint16_t BootSignature;
        uint32_t VolumeID;
        string VolumeLabel;
        string FileSystemType;
        uint16_t FirstRootSector;
        uint16_t RootDirectorySectors;
        uint16_t FirstDataSector;
        uint16_t ClusterCount;
   }BPB;

   typedef struct 
   {
        SVMDirectoryEntry DirectoryEntry;
   }RootName;

   typedef struct 
   {
       int offset;
   }Directory;

    vector<Directory*> DirectoryVector;
    vector<RootName*> RootVector;

   void parse(uint8_t* TempPointer)
   {
        //stores the bpb. gets info from fat file
        BPB *MyBPB=new BPB;
        for(int i=3; i<11; i++)
        {
            MyBPB->name.push_back(TempPointer[i]);
        }
        //cerr << "OEM Name: " << MyBPB->name << endl;
        MyBPB->BytePerSector = TempPointer[11] + (((uint16_t)TempPointer[11+1])<<8);
        //cerr << "BytePerSector: " << MyBPB->BytePerSector << endl;
        MyBPB->SectorsPerCluster = TempPointer[13];
        //cerr << "SectorsPerCluster: " << MyBPB->SectorsPerCluster << endl;
        MyBPB->ReservedSectors = TempPointer[14] + (((uint16_t)TempPointer[14+1])<<8);
        //cerr << "ReservedSectors: " << MyBPB->ReservedSectors << endl;
        MyBPB->FATCount = TempPointer[16]; 
        //cerr << "FATCount: " << MyBPB->FATCount << endl;
        MyBPB->RootEntry = TempPointer[17] + (((uint16_t)TempPointer[17+1])<<8);
        //cerr << "RootEntry: " << MyBPB->RootEntry << endl;
        MyBPB->SectorCount16 = TempPointer[19] + (((uint16_t)TempPointer[19+1])<<8);
        //cerr << "SectorCount16: " << MyBPB->SectorCount16<< endl;
        MyBPB->Media = TempPointer[21]; 
        //cerr << "Media: " << MyBPB->Media << endl;
        MyBPB->FATSize16 = TempPointer[22] + (((uint16_t)TempPointer[22+1])<<8);
        //cerr << "FATSize16: " << MyBPB->FATSize16 << endl;
        MyBPB->SectorPerTrack = TempPointer[24] + (((uint16_t)TempPointer[24+1])<<8);
        //cerr << "SectorPerTrack: " << MyBPB->SectorPerTrack << endl;
        MyBPB->HeadCount = TempPointer[26] + (((uint16_t)TempPointer[26+1])<<8);
        //cerr << "HeadCount: " << MyBPB->HeadCount << endl;
        MyBPB->HiddenSectorCount = TempPointer[28] + TempPointer[28] + (((uint32_t)TempPointer[28+1])<<8) + (((uint32_t)TempPointer[28+2])<<16) +  (((uint32_t)TempPointer[28+3])<<24);
        //cerr << "HiddenSectorCount: " << MyBPB->HiddenSectorCount << endl;
        MyBPB->SectorCount32 = TempPointer[32] + (((uint32_t)TempPointer[32+1])<<8) + (((uint32_t)TempPointer[32+2])<<16) + (((uint32_t)TempPointer[32+3])<<24);
        //cerr << "SectorCount32: " << MyBPB->SectorCount32 << endl;
        MyBPB->DriveNumer = TempPointer[36];
        //cerr << "DriveNumer: " << MyBPB->DriveNumer << endl;
        MyBPB->BootSignature = TempPointer[38];
        //cerr << "BootSignature: " << MyBPB->BootSignature << endl;
        MyBPB->VolumeID = TempPointer[39] + (((uint32_t)TempPointer[39+1])<<8) + (((uint32_t)TempPointer[39+2])<<16) + (((uint32_t)TempPointer[39+3])<<24);
        //cerr << "VolumeID: " << hex<< MyBPB->VolumeID << dec<< endl;
        for(int i = 43; i<53; i++)
        {
            MyBPB->VolumeLabel.push_back(TempPointer[i]);
        }
        //cerr << "VolumeLabel: " << "\"" << MyBPB->VolumeLabel << "\"" << endl;
        for(int i = 54; i<61; i++)
        {
            MyBPB->FileSystemType.push_back(TempPointer[i]);
        }
        //cerr << "FileSystemType: " << "\"" << MyBPB->FileSystemType << "\"" << endl;
        MyBPB->RootDirectorySectors = (MyBPB->RootEntry * 32) / 512;
        //cerr << "RootDirectorySectors: " << MyBPB->RootDirectorySectors << endl;
        MyBPB->FirstRootSector = MyBPB->ReservedSectors + MyBPB->FATCount * MyBPB->FATSize16;
        //cerr << "FirstRootSector: " << MyBPB->FirstRootSector << endl;
        MyBPB->FirstDataSector = MyBPB->FirstRootSector + MyBPB->RootDirectorySectors;
        //cerr << "FirstDataSector: " << MyBPB->FirstDataSector << endl;
        MyBPB->ClusterCount = (MyBPB->SectorCount32 - MyBPB->FirstDataSector) / MyBPB->SectorsPerCluster;
        //cerr << "ClusterCount: " << MyBPB->ClusterCount << endl;
        int FirstFatSector = MyBPB->ReservedSectors*(int)MACHINE_MEMORY_LIMIT;
        //gets where the first fat begins
//parse fat
        void *TempOffset=NULL;//for offset for memory pool allocation
        uint16_t *FATtemp; //for casting to make it uint16
        for(int i=0; i<MyBPB->FATSize16; i++)
        {
            //cerr << "1" << endl;
            VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED, MACHINE_MEMORY_LIMIT, (void**)&TempOffset);
            //cerr << "2" << endl;
            ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;
            //iterates though the each 512 bytes from the first fatsector
            MachineFileSeek(GlobalValue, FirstFatSector+(i*(int)MACHINE_MEMORY_LIMIT), 0, FileCallback, ThreadIDVector[CurrentThreadIndex]);  
            //cerr << "3" << endl;
            scheduler();
            ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;
            //stores in tempoffset
            MachineFileRead(GlobalValue, TempOffset, MACHINE_MEMORY_LIMIT, FileCallback, ThreadIDVector[CurrentThreadIndex]);
            //cerr << "4" << endl;
            scheduler();
            int j = 0;
            //temp = *TempOffset;
            //gets info from tempoffset and sets it to uint16 to store in vecotr
            FATtemp=(uint16_t*)TempOffset;
            //cerr << "5" << endl;
            while(j < (int)MACHINE_MEMORY_LIMIT/2)   //read in half of the fat
            {
                //2 sectors per cluster
                //temp=TempPointer[j];// + (((uint16_t)TempPointer[k+1])<<8);
                //cerr << "6" << endl;
                //stores into fatvector
                FATVector.push_back(FATtemp[j]); 
                //cerr << "8" << endl;
                //cerr << temp[j] ;
                //cerr << hex <<FATtemp[j] << dec << " ";
                j++;
                //cerr << "7" << endl;
            }    
            VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED, (void*)TempOffset);
            
        }   
        //cerr << endl;
//ROOT PARSE
        //same as parsing fat cept use diff iteration values
        //cerr << "@" << endl;
        void* RootTempOffset= NULL;
        uint8_t *RootTemp = NULL;

        //cerr << "!" << endl;
        //iterate till end of root directory sector
        for(int i=0; i<MyBPB->RootDirectorySectors; i++)
        {
            //cerr << "1" << endl;
            VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED, MACHINE_MEMORY_LIMIT, (void**)&RootTempOffset);
            //cerr << "2" << endl;
            ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;
            //iterates though each 512 bytes from the first root sector location
            MachineFileSeek(GlobalValue, (MyBPB->FirstRootSector*(int)MACHINE_MEMORY_LIMIT)+(i*(int)MACHINE_MEMORY_LIMIT), 0, FileCallback, ThreadIDVector[CurrentThreadIndex]);  
            //cerr << "3" << endl;
            scheduler();
            ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;
            MachineFileRead(GlobalValue, RootTempOffset, MACHINE_MEMORY_LIMIT, FileCallback, ThreadIDVector[CurrentThreadIndex]);
            //cerr << "4" << endl;
            scheduler();
      //      int j = 0;
            //temp = *TempOffset;
            //stores rootTemp to result get from machine file read
            RootTemp=(uint8_t*)RootTempOffset;
            //cerr << "5" << endl;
            /*
            while(j < (int)MACHINE_MEMORY_LIMIT)   
            {
                //2 sectors per cluster
                //temp=TempPointer[j];// + (((uint16_t)TempPointer[k+1])<<8);
                //cerr << "6" << endl;
                //pushes back into root
                RootVector.push_back(RootTemp[j]); 
                //cerr << "8" << endl;
                //cerr << temp[j] ;
                //cerr << hex <<RootTemp[j] << dec << " ";
                j++;
                //cerr << "7" << endl;
            }    
        */
        //cerr << endl;
//parses directory name
            //cerr << "1" << endl;
            for(int k=0; k<16; k++)
            {
              //  cerr << "2" << endl;
                if(RootTemp[11+(k*32)] != 15 && RootTemp[(k*32)] != '\0')
                {
                    RootName *MyRootEntry=new RootName;

                //    cerr << "3" << endl;
                    //cerr << RootTemp[11+(k*32)] << endl;
                    int CurrentLocation=0;
                    for(int j=0; j<11 ; j++)
                    {
                    //    cerr << "!" << endl;
                        //pushes in first 8 character that is not empty
                        if(j<8 && RootTemp[j+(k*32)] != ' ')
                        {
                      //      cerr <<"@" << endl;
                            MyRootEntry->DirectoryEntry.DShortFileName[CurrentLocation] = RootTemp[j+(k*32)];
                            CurrentLocation++;
                            //cerr << hex << MyRootEntry->DirectoryEntry.DShortFileName[currentlocation] << dec << endl;   
                        }
                        //max number of characters it can take in and sets the "."
                        if(j==8 && RootTemp[j+(k*32)] !=' ')
                        {
                      //      cerr << "#" << endl;
                            //add currentlocation = to .
                            MyRootEntry->DirectoryEntry.DShortFileName[CurrentLocation]='.';
                            CurrentLocation++;

                        }
                        //finishes the extention
                        if(j>=8 && RootTemp[j+(k*32)] !=' ')
                        {
                        //    cerr << "$" << endl;
                            MyRootEntry->DirectoryEntry.DShortFileName[CurrentLocation] = RootTemp[j+(k*32)];
                            //cerr << hex << MyRootEntry->DirectoryEntry.DShortFileName[currentlocation] << dec;

                            CurrentLocation++;
                        }


                        //cerr << hex << RootEntryVector[0]->DirectoryEntry.DShortFileName << " " << dec;    

                    }   
                    MyRootEntry->DirectoryEntry.DSize= RootTemp[28+(k*32)] + (((uint8_t)RootTemp[28+1+(k*32)])<<8) + (((uint8_t)RootTemp[28+2+(k*32)])<<16) + (((uint8_t)RootTemp[28+3+(k*32)])<<24);
                    MyRootEntry->DirectoryEntry.DAttributes = RootTemp[11+(k*32)];
                    uint16_t CreateDate = RootTemp[16+(k*32)] + (((uint16_t)RootTemp[16+1+(k*32)])<<8);
                    uint16_t CreateTime = RootTemp[14+(k*32)] + (((uint16_t)RootTemp[14+1+(k*32)])<<8); 
                    uint16_t AccessDate = RootTemp[18+(k*32)] + (((uint16_t)RootTemp[18+1+(k*32)])<<8);
                    uint16_t ModifyDate = RootTemp[24+(k*32)] + (((uint16_t)RootTemp[24+1+(k*32)])<<8);
                    uint16_t ModifyTime = RootTemp[22+(k*32)] + (((uint16_t)RootTemp[22+1+(k*32)])<<8);
               //DCREATE
                    MyRootEntry->DirectoryEntry.DCreate.DYear = (CreateDate >> 9) + 1980;
                    MyRootEntry->DirectoryEntry.DCreate.DMonth = (CreateDate >> 5) & 0xF;
                    MyRootEntry->DirectoryEntry.DCreate.DDay = (CreateDate & 0xF);
                    MyRootEntry->DirectoryEntry.DCreate.DHour = (CreateTime >> 11);
                    MyRootEntry->DirectoryEntry.DCreate.DMinute = (CreateTime >> 5) & 0x3F;
                    MyRootEntry->DirectoryEntry.DCreate.DSecond = (CreateTime & 0x1F) << 1;
                //DACCESS
                    MyRootEntry->DirectoryEntry.DAccess.DYear = (AccessDate >> 9) + 1980;
                    MyRootEntry->DirectoryEntry.DAccess.DMonth = (AccessDate >> 5) & 0xF;
                    MyRootEntry->DirectoryEntry.DAccess.DDay = (AccessDate & 0xF);
                    //MyRootEntry->DirectoryEntry.DAccess.DHour = (CreateTime >> 11);
                    //MyRootEntry->DirectoryEntry.DAccess.DMinute = (CreateTime >> 5) & 0x3F;
                    //MyRootEntry->DirectoryEntry.DAccess.DSecond = (CreateTime & 0x1F) << 1;
                //DMODIFY
                    MyRootEntry->DirectoryEntry.DModify.DYear = (ModifyDate >> 9) + 1980;
                    MyRootEntry->DirectoryEntry.DModify.DMonth = (ModifyDate>> 5) & 0xF;
                    MyRootEntry->DirectoryEntry.DModify.DDay = (ModifyDate & 0xF);
                    MyRootEntry->DirectoryEntry.DModify.DHour = (ModifyTime >> 11);
                    MyRootEntry->DirectoryEntry.DModify.DMinute = (ModifyTime>> 5) & 0x3F;
                    MyRootEntry->DirectoryEntry.DModify.DSecond = (ModifyTime & 0x1F) << 1;

                    RootVector.push_back(MyRootEntry);                     
                }

            }
            VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED, (void*)RootTempOffset);
        }

   }

    void IdleEntry(void *param)
    {   
        
        MachineEnableSignals();
        while(true)
        {
           //cerr << "idle" << endl;
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
                    ThreadIDVector[thread]->MutexPrioIndex = MutexIDVector[mutex]->LowPrio.size();
                    MutexIDVector[mutex]->LowPrio.push_back(ThreadIDVector[thread]);
                    break;
                case VM_THREAD_PRIORITY_NORMAL:
                    ThreadIDVector[thread]->MutexPrioIndex = MutexIDVector[mutex]->NormalPrio.size();
                    MutexIDVector[mutex]->NormalPrio.push_back(ThreadIDVector[thread]);
                    break;
                case VM_THREAD_PRIORITY_HIGH:
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

    void PlaceIntoWaitQueue(TVMThreadID thread)
    {
        if(ThreadIDVector[thread]->ThreadState == VM_THREAD_STATE_WAITING)
        {
            //cerr<<endl<<"enter wait queu "<<thread<<endl;
            switch(ThreadIDVector[thread]->ThreadPriority)
            {
                case VM_THREAD_PRIORITY_LOW:    
                    LowWaitQueue.push_back(ThreadIDVector[thread]);
                    break;
                case VM_THREAD_PRIORITY_NORMAL:
                    NormalWaitQueue.push_back(ThreadIDVector[thread]);
                    break;
                case VM_THREAD_PRIORITY_HIGH:
                    HighWaitQueue.push_back(ThreadIDVector[thread]);
                    break;
            }
        }
    }

    void WaitToReady()
    {
        TVMThreadID tid;
        if(!HighWaitQueue.empty())
        {
            tid = HighWaitQueue.front()->Thread_ID;
            HighWaitQueue.erase(HighWaitQueue.begin());
            ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_READY;
            PlaceIntoQueue(tid);
        }
        else if(!NormalWaitQueue.empty())
        {
            tid = NormalWaitQueue.front()->Thread_ID;
            NormalWaitQueue.erase(NormalWaitQueue.begin());
            ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_READY;
            //cerr<<endl<<"exit wait queue "<<tid<<endl;
            PlaceIntoQueue(tid);
        }
        else if(!LowWaitQueue.empty())
        {
            tid = LowWaitQueue.front()->Thread_ID;
            LowWaitQueue.erase(LowWaitQueue.begin());
            ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_READY;
            PlaceIntoQueue(tid);
        }
    }

    //Check wait queues to see if there is currently enough free shared space to run the waiting threads
    void CheckForFreeSharedSpace()
    {
        TVMThreadID tid;
        list<block*>::iterator it;

        if(!HighWaitQueue.empty())
        {
            for(it = MemoryIDVector[VM_MEMORY_POOL_ID_SHARED]->FreeList.begin(); it != MemoryIDVector[VM_MEMORY_POOL_ID_SHARED]->FreeList.end();it++)
            {
                if((*it)->length >= HighWaitQueue.front()->memsize)
                {
                    tid = HighWaitQueue.front()->Thread_ID;
                    HighWaitQueue.erase(HighWaitQueue.begin());
                    ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_READY;
                    PlaceIntoQueue(tid);
                    //cerr<<endl<<"exit wait queue"<<tid<<endl;
                }
            }
        }
        else if(!NormalWaitQueue.empty())
        {
            for(it = MemoryIDVector[VM_MEMORY_POOL_ID_SHARED]->FreeList.begin(); it != MemoryIDVector[VM_MEMORY_POOL_ID_SHARED]->FreeList.end();it++)
            {
                if((*it)->length >= NormalWaitQueue.front()->memsize)
                {
                    tid = NormalWaitQueue.front()->Thread_ID;
                    NormalWaitQueue.erase(NormalWaitQueue.begin());
                    ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_READY;
                    PlaceIntoQueue(tid);
                    //cerr<<endl<<"exit wait queue"<<tid<<endl;
                }
            }
        }
        else if(!LowWaitQueue.empty())
        {
            for(it = MemoryIDVector[VM_MEMORY_POOL_ID_SHARED]->FreeList.begin(); it != MemoryIDVector[VM_MEMORY_POOL_ID_SHARED]->FreeList.end();it++)
            {
                if((*it)->length >= LowWaitQueue.front()->memsize)
                {
                    tid = LowWaitQueue.front()->Thread_ID;
                    LowWaitQueue.erase(LowWaitQueue.begin());
                    ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_READY;
                    PlaceIntoQueue(tid);
                    //cerr<<endl<<"exit wait queue"<<tid<<endl;
                }
            }
        }
        
    }

    void scheduler()
    {
        TVMThreadID tid;
        TVMThreadID Original = CurrentThreadIndex;
        //cerr<<"called sched "<< CurrentThreadIndex<< endl;
        if(ThreadIDVector[CurrentThreadIndex]->ThreadState == VM_THREAD_STATE_RUNNING)
        {
            //cerr<<"in here"<<endl;
            if(!HighQueue.empty()&&(VM_THREAD_PRIORITY_HIGH > ThreadIDVector[CurrentThreadIndex]->ThreadPriority))
            {
                tid = HighQueue.front()->Thread_ID;
                HighQueue.erase(HighQueue.begin());
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                ThreadIDVector[Original]->ThreadState = VM_THREAD_STATE_READY;
                PlaceIntoQueue(Original);

                MachineContextSwitch(&ThreadIDVector[Original]->context,&ThreadIDVector[tid]->context);

            }
            else if(!NormalQueue.empty()&&(VM_THREAD_PRIORITY_NORMAL > ThreadIDVector[CurrentThreadIndex]->ThreadPriority))
            {
                tid = NormalQueue.front()->Thread_ID;
                NormalQueue.erase(NormalQueue.begin());
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                ThreadIDVector[Original]->ThreadState = VM_THREAD_STATE_READY;
                PlaceIntoQueue(Original);
                //cerr<<endl<<"switch to "<< CurrentThreadIndex<<endl;
                MachineContextSwitch(&(ThreadIDVector[Original]->context),&(ThreadIDVector[tid]->context));

            }

            else if(!LowQueue.empty()&&(VM_THREAD_PRIORITY_LOW > ThreadIDVector[CurrentThreadIndex]->ThreadPriority))
            {
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
                tid = HighQueue.front()->Thread_ID;
                HighQueue.erase(HighQueue.begin());
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                MachineContextSwitch(&ThreadIDVector[Original]->context,&ThreadIDVector[tid]->context);

            }
            else if(!NormalQueue.empty())
            {
                tid = NormalQueue.front()->Thread_ID;
                NormalQueue.erase(NormalQueue.begin());
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                //cerr<<endl<<"switch to "<< CurrentThreadIndex<<endl;
                MachineContextSwitch(&ThreadIDVector[Original]->context,&ThreadIDVector[tid]->context);

            }

            else if(!LowQueue.empty())
            {
                tid = LowQueue.front()->Thread_ID;
                LowQueue.erase(LowQueue.begin());
                ThreadIDVector[tid]->ThreadState = VM_THREAD_STATE_RUNNING;
                CurrentThreadIndex = tid;
                MachineContextSwitch(&ThreadIDVector[Original]->context,&ThreadIDVector[tid]->context);

            }

            else
            {
                CurrentThreadIndex = 1;
                ThreadIDVector[1]->ThreadState = VM_THREAD_STATE_RUNNING;
                //cerr<<"go to idle"<<endl;
                MachineContextSwitch(&ThreadIDVector[Original]->context, &ThreadIDVector[1]->context);
            }
        }
    }

    bool compareBlockAddresses(const block* block1, const block* block2)
    {
        return ( block1->address < block2->address);
    }

    TVMStatus VMMemoryPoolCreate(void* base, TVMMemorySize size, TVMMemoryPoolIDRef memory)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(base == NULL||memory == NULL|| size == 0)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        *memory = MemoryIDVector.size();
        MemoryIDVector.push_back(new MemoryPool);

        MemoryIDVector[*memory]->PoolID = *memory;
        MemoryIDVector[*memory]->base = (uint8_t*)base;
        MemoryIDVector[*memory]->MemoryPoolSize = size;
        //other stuff for later
        block *ablock = new block;
        ablock->length = size;
        ablock->address = (uint8_t*)base;
        MemoryIDVector[*memory]->FreeList.push_back(ablock);
        MemoryIDVector[*memory]->FreeSpace = size;

        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;
    }

    TVMStatus VMMemoryPoolAllocate(TVMMemoryPoolID memory, TVMMemorySize size, void **pointer)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(memory < 0 || memory >= MemoryIDVector.size() || size == 0 || pointer == NULL)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

       /* if(MemoryIDVector[memory]->FreeSpace < size)
        {
            return VM_STATUS_ERROR_INSUFFICIENT_RESOURCES;
        }*/
        //round up to nearest multiple of 64
        if((size % 64) > 0)
        {
            size = (size+64)/64*64;
        }
        //check first block with enough space,  set the new base, place into allocate
        list<block*>::iterator it;
        for(it = MemoryIDVector[memory]->FreeList.begin(); it != MemoryIDVector[memory]->FreeList.end(); it++)
        {
            //If the free space block has enough size then allocate
            if((*it)->length >= size)
            {
                block* aBlock = new block;
                aBlock->address = (*it)->address;
                aBlock->length = size;
                *pointer = (*it)->address;
                MemoryIDVector[memory]->AllocatedList.push_back(aBlock);
                
                //If size != length, cut the block 
                if(size != (*it)->length)
                {
                    //reduce the length
                    (*it)->length -= size;
                    //new base
                    (*it)->address += size;

                }
                //if the allocated size is equal to the length of the block's free space
                //we need to just erase the block fromt he freelist
                else
                {
                    it = MemoryIDVector[memory]->FreeList.erase(it);
                }

                MemoryIDVector[memory]->FreeSpace -= size;
                MemoryIDVector[memory]->FreeList.sort(compareBlockAddresses);
                MachineResumeSignals(&OldState);
                return VM_STATUS_SUCCESS;
            }//if enough space
        }

        MachineResumeSignals(&OldState);
        return VM_STATUS_ERROR_INSUFFICIENT_RESOURCES;
    }

    //iterate through the memory pool's free list and merge any possible blocks
    void MergeFreeBlocks(TVMMemoryPoolID memory)
    {
        list<block*>::iterator it1;
        list<block*>::iterator it2 = MemoryIDVector[memory]->FreeList.begin();
        it2++;

        for(it1 = MemoryIDVector[memory]->FreeList.begin(); it2 != MemoryIDVector[memory]->FreeList.end();it1++,it2++)
        {
            //if the address of the next block is equal to the address of current block + length then it is continuous
            if(((*it1)->address+(*it1)->length) ==((*it2)->address))
            {
                (*it1)->length += (*(it2))->length;
                it2 = MemoryIDVector[memory]->FreeList.erase(it2);
                it1--;
                it2--; 
            }
        }
    }

    TVMStatus VMMemoryPoolDeallocate(TVMMemoryPoolID memory, void *pointer)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);
        if(memory < 0 || memory >= MemoryIDVector.size())
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        list<block*>::iterator it;
        //search the allocated list for the block we want to deallocate
        for(it = MemoryIDVector[memory]->AllocatedList.begin(); it != MemoryIDVector[memory]->AllocatedList.end(); it++)
        {
            if((*it)->address == pointer)
            {
                //remove from allocated, place into free, sort, iterate through free and merge
                block* aBlock = new block;
                aBlock->address = (*it)->address;
                aBlock->length = (*it)->length;
                MemoryIDVector[memory]->FreeSpace += (*it)->length;
                MemoryIDVector[memory]->FreeList.push_back(aBlock);
                MemoryIDVector[memory]->FreeList.sort(compareBlockAddresses);
                MemoryIDVector[memory]->AllocatedList.erase(it);
                MergeFreeBlocks(memory);

                break;
            }
        }

        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;
    }
    
    TVMStatus VMMemoryPoolDelete(TVMMemoryPoolID memory)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(memory < 0 || memory >= MemoryIDVector.size())
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        MemoryIDVector.erase(MemoryIDVector.begin()+memory);

        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;
    }

    TVMStatus VMMemoryPoolQuery(TVMMemoryPoolID memory, TVMMemorySizeRef bytesleft)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(memory < 0 || memory >= MemoryIDVector.size() || bytesleft == NULL)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        *bytesleft = MemoryIDVector[memory]->FreeSpace;

        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;
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

    TVMStatus VMStart(int tickms, TVMMemorySize heapsize, int machinetickms, TVMMemorySize sharedsize, const char *mount, int argc, char *argv[])
	{    
        
        //declare it
		TVMMainEntry VMMain;
        //load the module
		VMMain = VMLoadModule(argv[0]);	
        TVMMemoryPoolID id  = 12323;
        uint8_t* aBase = new uint8_t[heapsize];
        TVMMemorySize altsharedsize = sharedsize;
        //VMMemoryPoolCreate(FileImageData, 512, &id);
        //VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SYSTEM, 512, (void**)&(ThreadIDVector[VM_MEMORY_POOL_ID_SYSTEM]->BaseStack));
        //MachineFileRead(3, data, length, FileCallback, VM_FILE_IMAGE);


        //round up to nearest multiple of 4096
        if((sharedsize % 4096) > 0)
        {
            altsharedsize = (sharedsize + 4096)/4096*4096;
        }

        void* sharedBase = MachineInitialize(machinetickms, altsharedsize);
        MachineRequestAlarm(tickms*1000,(TMachineAlarmCallback)AlarmRequestCallback,NULL);
        //MachineEnableSignals();
        VMMemoryPoolCreate(aBase, heapsize, &id);

        VMMemoryPoolCreate((uint8_t*)sharedBase, altsharedsize, &id);


        //create the system memory pool
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
        VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SYSTEM,ThreadIDVector[1]->MemorySize, (void**)&(ThreadIDVector[1]->BaseStack));

        MachineContextCreate(&(ThreadIDVector[1]->context), IdleEntry , NULL,ThreadIDVector[1]->BaseStack, ThreadIDVector[1]->MemorySize);

//<<<<<<< HEAD
//        TMachineSignalState OldState;
//        MachineSuspendSignals(&OldState);
        ThreadIDVector[CurrentThreadIndex]->ThreadState=VM_THREAD_STATE_WAITING;

        uint8_t *TempPointer = NULL;
        //mounting 
        MachineFileOpen(mount, O_RDWR, 0644, FileCallback, ThreadIDVector[CurrentThreadIndex]);
        scheduler();
        GlobalValue= ThreadIDVector[CurrentThreadIndex]->file;
        VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED, MACHINE_MEMORY_LIMIT, (void**)&TempPointer);
        ThreadIDVector[CurrentThreadIndex]->ThreadState=VM_THREAD_STATE_WAITING;
//<<<<<<< HEAD
        MachineFileRead(GlobalValue, TempPointer, MACHINE_MEMORY_LIMIT, FileCallback, ThreadIDVector[CurrentThreadIndex]);
        scheduler();
  //      MachineResumeSignals(&OldState);
       /* for(int i=0; i<512; i++)
        {
            cerr<<TempPointer[i]<< " ";
        }
=======
        cerr<<"hand2";
        MachineFileRead(GlobalValue, TempPointer, MACHINE_MEMORY_LIMIT, FileCallback, ThreadIDVector[CurrentThreadIndex]);
        cerr << "hi";
        scheduler();
        cerr << "hey ";

        for(int i=0; 512; i++)
        {
            cerr<<TempPointer[i]<< " ";
        }
>>>>>>> origin/project4
*/
        parse(TempPointer);

/*
        uint8_t *FATPointer = NULL;
        VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED, MACHINE_MEMORY_LIMIT, (void**)&FATPointer);
        ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;
        MachineFileRead(GlobalValue, FATPointer, MACHINE_MEMORY_LIMIT, FileCallback ThreadIDVector[CurrentThreadIndex]);
        scheduler();
*/
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
        unsigned int LengthRemaining = *length;
        unsigned int CurrentLength;
        int it = 0;
       // char* FullString = (char *)data;

        ThreadIDVector[CurrentThreadIndex]->memsize = *length;

        void *shared = NULL;      
     
        while(VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SHARED, MACHINE_MEMORY_LIMIT, &shared) == VM_STATUS_ERROR_INSUFFICIENT_RESOURCES)
        {
            ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;
            PlaceIntoWaitQueue(CurrentThreadIndex);
            scheduler();
        }    
        //while loop to make sure data transfer is in 512 byte segments
        while(LengthRemaining > 0)
        {
            if(LengthRemaining > MACHINE_MEMORY_LIMIT)
            {
                LengthRemaining -= MACHINE_MEMORY_LIMIT;
                CurrentLength = MACHINE_MEMORY_LIMIT;
            }
            else
            {
                CurrentLength = LengthRemaining;
                LengthRemaining = 0;
            }

            memcpy(shared,(char*)data + it,CurrentLength);
            it+=CurrentLength;
            MachineFileWrite(filedescriptor, shared, CurrentLength, FileCallback, ThreadIDVector[CurrentThreadIndex]);
            ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;
            scheduler();  
        }

        VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED, shared);       

        WaitToReady();
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

        unsigned int temp = *length;
        unsigned int temp2;
        *length = 0;

        while(temp > 0)
        {
            if(temp > MACHINE_MEMORY_LIMIT)
            {
                temp -= MACHINE_MEMORY_LIMIT;
                temp2 = MACHINE_MEMORY_LIMIT;
            }
            else
            {
                temp2 = temp;
                temp = 0;
            }

            void *shared;      
            VMMemoryPoolAllocate(1, MACHINE_MEMORY_LIMIT, &shared);

            MachineFileRead(filedescriptor, shared, temp2, FileCallback, ThreadIDVector[CurrentThreadIndex]);
            ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;
            scheduler();  

            memcpy(data, shared, temp2);
            VMMemoryPoolDeallocate(VM_MEMORY_POOL_ID_SHARED, shared);

            *length += ThreadIDVector[CurrentThreadIndex]->file;
        }//while(temp>0)

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
        VMMemoryPoolAllocate(VM_MEMORY_POOL_ID_SYSTEM, memsize, (void**)&(ThreadIDVector[*tid]->BaseStack));

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
            ThreadIDVector[CurrentThreadIndex]->ThreadState = VM_THREAD_STATE_WAITING;
            ThreadIDVector[CurrentThreadIndex]->ticks = timeout;
            SleepingQueue.push_back(ThreadIDVector[CurrentThreadIndex]);
            PlaceIntoMutexQueue(CurrentThreadIndex,mutex);

            scheduler();
        

           /* if(ThreadIDVector[CurrentThreadIndex]->ticks == 0)
            {    
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
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

		if(mutex >= MutexIDVector.size()||mutex < 0)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_ID;
        }

        if(!MutexIDVector[mutex]->HighPrio.empty())
        {
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


    TVMStatus VMDirectoryOpen(const char *dirname, int *dirdescriptor)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);
        if(dirname == NULL || dirdescriptor == NULL)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        if(DirectoryVector.size() < 0)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_FAILURE;
        }

        Directory *MyDirectory = new Directory;
        
        MyDirectory->offset = 0;
        *dirdescriptor = DirectoryVector.size()+1234321;
        DirectoryVector.push_back(MyDirectory);
        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;
    }
    TVMStatus VMDirectoryClose(int dirdescriptor)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);
        

        if(DirectoryVector.size() < 0)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_FAILURE;
        }
        else
        {
            DirectoryVector.erase(DirectoryVector.begin()+dirdescriptor);
            MachineResumeSignals(&OldState);
            return VM_STATUS_SUCCESS;
        }         
    }

    TVMStatus VMDirectoryRead(int dirdescriptor, SVMDirectoryEntryRef dirent)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(dirent == NULL)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }
        if(RootVector.size() > DirectoryVector[dirdescriptor-1234321]->offset)
        {
            DirectoryVector[dirdescriptor-1234321]->offset++;
            *dirent = RootVector[DirectoryVector[dirdescriptor-1234321]->offset]->DirectoryEntry;
            MachineResumeSignals(&OldState);
            return VM_STATUS_SUCCESS;
        }
    }

    TVMStatus VMDirectoryCurrent(char *abspath)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(abspath==NULL)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;
    }

    TVMStatus VMDirectoryChange(const char *path)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(path == NULL)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;
    }

    TVMStatus VMDirectoryUnlink(const char *path)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(path == NULL)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;
    }

    TVMStatus VMDirectoryCreate(const char *dirname)
    {
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        if(dirname == NULL)
        {
            MachineResumeSignals(&OldState);
            return VM_STATUS_ERROR_INVALID_PARAMETER;
        }

        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;   
    }

}

