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

// Control Scheme Bools (read from config file)
bool DPadCont = false;
bool KeyboardCont = false;

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

    // Enable this if you want to use HID.
    rc = hidInitialize();
    if (R_FAILED(rc))
        fatalSimple(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));

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

    rc = viInitialize(ViServiceType_System);
    if(R_FAILED(rc))
        fatalSimple(rc);

    pmdmntInitialize();

    FILE *file = fopen("sdmc:/atmosphere/titles/0100000000000125/config.txt", "r");

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
    ViDisplay disp;
    Result rc = viOpenDefaultDisplay(&disp);
    if(R_FAILED(rc))
        fatalSimple(rc);
    Event vsync_event;
    rc = viGetDisplayVsyncEvent(&disp, &vsync_event);
    if(R_FAILED(rc))
        fatalSimple(rc);

    // Initialization code can go here.
    bool isPaused = false;

    // Your code / main loop goes here.
    // If you need threads, you can use threadCreate etc.
    while(true)
    {
        // Scan all the inputs. This should be done once for each frame
        hidScanInput();

        // hidKeysDown returns information about which buttons have been
        // just pressed in this frame compared to the previous one
        if(DPadCont)
        {
            u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

            if ((kDown & KEY_DLEFT) && !isPaused)
            {
                u64 pid = 0;
                pmdmntGetApplicationPid(&pid);
			    svcDebugActiveProcess(&m_debugHandle, pid);

                isPaused = true;
            }
            else if((kDown & KEY_DLEFT) && isPaused)
            {   
                svcCloseHandle(m_debugHandle);
                isPaused = false;
            }

            if((kDown & KEY_DUP) && isPaused)
            {
             svcCloseHandle(m_debugHandle);

             svcSleepThread(1e+9L/60);

                u64 pid = 0;
                pmdmntGetApplicationPid(&pid);
			    svcDebugActiveProcess(&m_debugHandle, pid);
            }
        }
        if(KeyboardCont)
        {
            if (hidKeyboardDown(KBD_DOWN) && !isPaused)
            {
                u64 pid = 0;
                pmdmntGetApplicationPid(&pid);
			    svcDebugActiveProcess(&m_debugHandle, pid);

                isPaused = true;
            }
            else if(hidKeyboardDown(KBD_DOWN) && isPaused)
            {   
                svcCloseHandle(m_debugHandle);
                isPaused = false;
            }

            if(hidKeyboardDown(KBD_RIGHT) && isPaused)
            {
                svcCloseHandle(m_debugHandle);

                rc = eventWait(&vsync_event, 0xFFFFFFFFFFF);
                if(R_FAILED(rc))
                    fatalSimple(rc);

                u64 pid = 0;
                pmdmntGetApplicationPid(&pid);
			    svcDebugActiveProcess(&m_debugHandle, pid);
            }
        }
        
        rc = eventWait(&vsync_event, 0xFFFFFFFFFFF);
        if(R_FAILED(rc))
            fatalSimple(rc);
    }
    // Deinitialization and resources clean up code can go here.
    return 0;
}