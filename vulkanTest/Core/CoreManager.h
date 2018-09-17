#pragma once

#include<memory>
#include"..\Application\Window\Window.h"

class CoreManager
{
private:
	class Impl;
	std::unique_ptr<Impl> impl_;
public:
	CoreManager() = delete;
	CoreManager(std::unique_ptr<Window>&);
	~CoreManager();

	bool Initialize(void);
	bool Run(void);
	void Exit(void);
};