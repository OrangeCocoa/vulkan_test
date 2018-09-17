#pragma once

#include<memory>
#include"..\Application\Window\Window.h"

class Graphics
{
private:
	class Impl;
	std::unique_ptr<Impl> impl_;
public:
	Graphics() = delete;
	Graphics(std::unique_ptr<Window>&);
	~Graphics();

	bool Initialize(void);
	bool Run(void);
	void Exit(void);

	void BeginFrame(void);
	void EndFrame(void);
};