#include <Windows.h>
#include "FiveM-External.hpp"
#include "Definations/Brand.hpp"
#include "Definations/Cheat.hpp"
#include "Render/WaitingWindow.hpp"

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	Cheat::InstallExceptionHandlers();

	if (!FrameWork::WaitingWindow::RunHub())
	{
		ExitProcess(0);
		return 0;
	}

	Cheat::Initialize();
	Cheat::ShutDown();
	ExitProcess(0);
	return 0;
}
