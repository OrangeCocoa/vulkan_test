
#include <windows.h>

#include"BaseSystem.h"
#include"..\Window\Window.h"
#include"..\..\Core\CoreManager.h"
#include"..\..\Utilities\Settings.h"
#include"..\..\Utilities\Utils.h"
#include"..\..\Utilities\Log.h"
#include"..\..\Utilities\Input.h"

std::string BaseSystem::workspace_directory_ = "";

class BaseSystem::Impl
{
public:
	bool app_exit_;
	std::unique_ptr<Window> window_;
	std::unique_ptr<CoreManager> core_manager_;

	Impl()
		: app_exit_(false)
		, window_(std::make_unique<Window>())
		, core_manager_(std::make_unique<CoreManager>(window_)){}

	void MessageLoop(void)
	{
		MSG msg = {};

		while (!app_exit_)
		{
			if (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessageA(&msg);
			}

			if (Input::IsKeyTriggered("escape"))
			{
				if (Input::IsMouseCaptured())
				{
					Input::CaptureMouse(window_->GetWindowHandle(), false);
				}
				else
				{
					if (MessageBoxA(window_->GetWindowHandle(), "Quit ?", "User Notification", MB_YESNO | MB_DEFBUTTON2) == IDYES)
					{
						Log::Info("[EXIT] KEY DOWN ESC");
						app_exit_ = true;
					}
				}
			}

			if (msg.message == WM_QUIT)
			{
				app_exit_ = true;
			}
			else
			{
				app_exit_ |= core_manager_->Run();
			}

			Input::PostStateUpdate();
		}
	}
};

BaseSystem::BaseSystem() : impl_(std::make_unique<Impl>()) {}

BaseSystem::~BaseSystem() { Log::Info("~BaseSystem()"); }// = default;

bool BaseSystem::Init(void)
{
	workspace_directory_ = DirectoryUtils::GetSpecialFolderPath(DirectoryUtils::FolderType::APPDATA) + "\\Vulkan Startup";

#ifdef _DEBUG
	Log::Initialize(Log::LogMode::CONSOLE);
#endif
	Input::Initialize();

	if (!impl_->window_->Init()) return false;

	if (!impl_->core_manager_->Initialize()) return false;

	return true;
}

void BaseSystem::Run(void)
{
	impl_->MessageLoop();
}

void BaseSystem::Exit(void)
{
	Log::Finalize();

	impl_->window_->Exit();
	impl_->window_.reset();

	impl_.reset();
}