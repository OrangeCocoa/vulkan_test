
#include<windows.h>
#include "BaseSystem\BaseSystem.h"

int __stdcall WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	BaseSystem application;
	if(application.Init()) application.Run();
	application.Exit();
	return 0;
}