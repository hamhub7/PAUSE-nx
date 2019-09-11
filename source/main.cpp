// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <iterator>
#include <algorithm>
#include <time.h>
#include <sstream>

// Include the main libnx system header, for Switch development
#include <switch.h>

extern "C"
{
    // Sysmodules should not use applet*.
    u32 __nx_applet_type = AppletType_None;

    // Adjust size as needed.
    #define INNER_HEAP_SIZE 0x8000
    size_t nx_inner_heap_size = INNER_HEAP_SIZE;
    char   nx_inner_heap[INNER_HEAP_SIZE];

    void __libnx_init_time(void);
    void __libnx_initheap(void);
    void __appInit(void);
    void __appExit(void);
}

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

    // Enable this if you want to use HID.
    rc = hidInitialize();
    if (R_FAILED(rc))
        fatalSimple(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));

    //Enable this if you want to use time.
    rc = timeInitialize();
    if (R_FAILED(rc))
        fatalSimple(MAKERESULT(Module_Libnx, LibnxError_InitFail_Time));

    __libnx_init_time();

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
}

void __attribute__((weak)) userAppExit(void);

void __attribute__((weak)) __appExit(void)
{
    // Cleanup default services.
    fsdevUnmountAll();
    fsExit();
    timeExit();//Enable this if you want to use time.
    hidExit();// Enable this if you want to use HID.
    smExit();
}

std::map<int, std::string> translator
{
    {KEY_A, "KEY_A"},
    {KEY_B, "KEY_B"},
    {KEY_X, "KEY_X"},
    {KEY_Y, "KEY_Y"},
    {KEY_LSTICK, "KEY_LSTICK"},
    {KEY_RSTICK, "KEY_RSTICK"},
    {KEY_L, "KEY_L"},
    {KEY_R, "KEY_R"},
    {KEY_ZL, "KEY_ZL"},
    {KEY_ZR, "KEY_ZR"},
    {KEY_PLUS, "KEY_PLUS"},
    {KEY_MINUS, "KEY_MINUS"},
    {KEY_DLEFT, "KEY_DLEFT"},
    {KEY_DUP, "KEY_DUP"},
    {KEY_DRIGHT, "KEY_DRIGHT"},
    {KEY_DDOWN, "KEY_DDOWN"},
};

std::string translateKeys(u64 keys)
{
    std::string returnString;

    for(std::pair<int, std::string> element : translator)
    {
        if(element.first & keys)
        {
            returnString.append(element.second);
            returnString.push_back(';');
        }
    }

    if(!returnString.empty())
    {
        returnString.pop_back();
        return returnString;
    }

    return "NONE";
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
    int frameCount = 0;
    FILE* f;

    // Your code / main loop goes here.
    // If you need threads, you can use threadCreate etc.
    while(true)
    {
        // Scan all the inputs. This should be done once for each frame
        hidScanInput();

        // hidKeysDown returns information about which buttons have been
        // just pressed in this frame compared to the previous one
        if (hidKeyboardDown(KBD_DOWN) && !isPaused)
        {
            u64 pid = 0;
            pmdmntGetApplicationPid(&pid);
            svcDebugActiveProcess(&m_debugHandle, pid);

            isPaused = true;
            frameCount = 0;

            time_t unixTime = time(NULL);
            struct tm* timeStruct = localtime((const time_t *)&unixTime);
            std::stringstream ss;
            std::string filename;
            ss << "/pausenx/recording-" << timeStruct->tm_mon + 1 << "_" << timeStruct->tm_mday << "_" << timeStruct->tm_year + 1900 << "-" << timeStruct->tm_hour << "_" << timeStruct->tm_min << "_" << timeStruct->tm_sec << ".txt";
            ss >> filename;
            f = fopen(filename.c_str(), "w");
        }
        else if(hidKeyboardDown(KBD_DOWN) && isPaused)
        {   
            svcCloseHandle(m_debugHandle);
            isPaused = false;
            fclose(f);
        }

        if(hidKeyboardDown(KBD_RIGHT) && isPaused)
        {
            svcCloseHandle(m_debugHandle);

            //Gather button and stick data
            u64 kHeld = hidKeysHeld(CONTROLLER_P1_AUTO);
            std::string keyString = translateKeys(kHeld);

            JoystickPosition posLeft, posRight;
            hidJoystickRead(&posLeft, CONTROLLER_P1_AUTO, JOYSTICK_LEFT);
            hidJoystickRead(&posRight, CONTROLLER_P1_AUTO, JOYSTICK_RIGHT);

            if(!(keyString == "NONE" && posLeft.dx == 0 && posLeft.dy == 0 && posRight.dy == 0 && posRight.dy == 0))
                fprintf(f, "%i %s %d;%d %d;%d\n", frameCount, keyString.c_str(), posLeft.dx, posLeft.dy, posRight.dx, posRight.dy);

            rc = eventWait(&vsync_event, 0xFFFFFFFFFFF);
            if(R_FAILED(rc))
                fatalSimple(rc);

            u64 pid = 0;
            pmdmntGetApplicationPid(&pid);
            svcDebugActiveProcess(&m_debugHandle, pid);

            frameCount++;
        }
        
        rc = eventWait(&vsync_event, 0xFFFFFFFFFFF);
        if(R_FAILED(rc))
            fatalSimple(rc);
    }
    // Deinitialization and resources clean up code can go here.
    return 0;
}