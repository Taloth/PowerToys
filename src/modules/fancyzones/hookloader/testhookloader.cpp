#include <iostream>
#include <Windows.h>
#include <string>

extern "C" __declspec(dllimport) HHOOK __cdecl MinMaxAttachHook();
extern "C" __declspec(dllimport) bool __cdecl MinMaxDetachHook(HHOOK hhook);

int main()
{
    HHOOK hook = MinMaxAttachHook();
    
    if (!hook)
    {
        std::cout << "Failed to load x86 hook." << std::endl;
    }

    // Background
    std::string dummy;
    std::cout << "Enter to continue..." << std::endl;
    std::getline(std::cin, dummy);

    if (hook)
    {
        MinMaxDetachHook(hook);
        UnhookWindowsHookEx(hook);
        PostMessage(HWND_BROADCAST, WM_NULL, 0, 0L);
        Sleep(100);
        PostMessage(HWND_BROADCAST, WM_NULL, 0, 0L);
    }
    std::cout << "Hello World!\n";
}
