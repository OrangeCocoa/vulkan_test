
#include"CoreManager.h"
#include"Graphics.h"

class CoreManager::Impl
{
public:
	std::shared_ptr<Graphics> graphics_;
	bool wait_exit;

	Impl(std::unique_ptr<Window>& window) : graphics_(std::make_shared<Graphics>(window)), wait_exit(false){}
};

CoreManager::CoreManager(std::unique_ptr<Window>& window) : impl_(std::make_unique<Impl>(window)) {}

CoreManager::~CoreManager() = default;

bool CoreManager::Initialize(void)
{
	if (!impl_->graphics_->Initialize()) return false;

	return true;
}

bool CoreManager::Run(void)
{
	impl_->graphics_->BeginFrame();

	// application run anything
	impl_->graphics_->Run();

	impl_->graphics_->EndFrame();

	return impl_->wait_exit; // default -> false
}

void CoreManager::Exit(void)
{
	impl_->graphics_->Exit();
}