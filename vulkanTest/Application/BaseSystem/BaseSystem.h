#pragma once

#include<memory>
#include<string>


class BaseSystem
{
	friend class BaseScene;

private:
	class Impl;
	std::unique_ptr<Impl> impl_;

public:
	BaseSystem();
	~BaseSystem();

	bool Init(void);
	void Run(void);
	void Exit(void);

	static std::string workspace_directory_;
};