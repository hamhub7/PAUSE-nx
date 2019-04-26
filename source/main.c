// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include the main libnx system header, for Switch development
#include <switch.h>

// Sysmodules should not use applet*.
u32 __nx_applet_type = AppletType_None;

// Adjust size as needed.
#define INNER_HEAP_SIZE 0x80000
size_t nx_inner_heap_size = INNER_HEAP_SIZE;
char   nx_inner_heap[INNER_HEAP_SIZE];

Handle m_debugHandle;

void __libnx_initheap(void)
{
	void*  addr = nx_inner_heap;
	size_t size = nx_inner_heap_size;

	// Newlib
	extern char* fake_heap_start;
	extern char* fake_heap_end;

	fake_heap_start = (char*)addr;
	fake_heap_end   = (char*)addr + size;
}

// Init/exit services, update as needed.
void __attribute__((weak)) __appInit(void)
{
    Result rc;

    // Initialize default services.

    rc = smInitialize();
    if (R_FAILED(rc))
        fatalSimple(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));

    rc = fsInitialize();
    if (R_FAILED(rc))
        fatalSimple(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));

    fsdevMountSdmc();

    FILE *file = fopen("sdmc:/startlog.txt", "wt");
    fprintf(file, "opened\n");
    fflush(file);

    // Enable this if you want to use HID.
    rc = hidInitialize();
    if (R_FAILED(rc))
        fatalSimple(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));

    fprintf(file, "hid\n");
    fflush(file);

    //Enable this if you want to use time.
    /*rc = timeInitialize();
    if (R_FAILED(rc))
        fatalSimple(MAKERESULT(Module_Libnx, LibnxError_InitFail_Time));

    __libnx_init_time();*/

    // Initialize system for pmdmnt
    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        rc = setsysGetFirmwareVersion(&fw);
        if (R_SUCCEEDED(rc))
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();
    }

    fprintf(file, "sys");
    fflush(file);

    Result pmdmnt = pmdmntInitialize();
    fprintf(file, "pmdmnt: 0x%016x\n", pmdmnt);
    fflush(file);

    fprintf(file, "Done\n");
    fflush(file);
    fclose(file);
}

void __attribute__((weak)) userAppExit(void);

void __attribute__((weak)) __appExit(void)
{
    // Cleanup default services.
    fsdevUnmountAll();
    fsExit();
    //timeExit();//Enable this if you want to use time.
    hidExit();// Enable this if you want to use HID.
    smExit();
}

// Main program entrypoint
int main(int argc, char* argv[])
{
    // Initialization code can go here.
    FILE *file = fopen("sdmc:/runlog.txt", "wt");

    // Your code / main loop goes here.
    // If you need threads, you can use threadCreate etc.
    while(true)
    {
        // Scan all the inputs. This should be done once for each frame
        hidScanInput();

        // hidKeysDown returns information about which buttons have been
        // just pressed in this frame compared to the previous one
        u64 kDown = hidKeysHeld(CONTROLLER_P1_AUTO);

        if (kDown & KEY_DLEFT)
        {
            fprintf(file, "held key\n");
            fflush(file);

            u64 pid = 0;
            Result getapp = pmdmntGetApplicationPid(&pid);
			svcDebugActiveProcess(&m_debugHandle, pid);
            fprintf(file, "pid is: 0x%016lx\n", pid);
            fprintf(file, "getApp returns: 0x%016x\n", getapp);
            fflush(file);
        }
        else
        {   
            fprintf(file, "did not hold key\n");
            fflush(file);
            svcBreakDebugProcess(m_debugHandle);
        }
        
        svcSleepThread(1e+9L/30);
    }

    // Deinitialization and resources clean up code can go here.
    return 0;
}
