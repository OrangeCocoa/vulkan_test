#pragma once
#include<windows.h>
#include<string>
#include<memory>

class BaseSystem;

class Window
{
private:
	class Impl;
	std::unique_ptr<Impl> impl_;

public:
	Window();
	~Window();

	bool Init(void);
	void Exit(void);

	HINSTANCE GetWindowInstance(void);
	HWND GetWindowHandle(void);
};