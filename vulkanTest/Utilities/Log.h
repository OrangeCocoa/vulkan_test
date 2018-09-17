#pragma once

#include<string>

namespace Log
{
	enum LogMode
	{
		NONE = 0,
		CONSOLE,
		FILE,
		CONSOLE_AND_FILE,
	};

	void Initialize(LogMode);

	void Finalize(void);

	void Error(const std::string&);

	template<class... Args>
	void Error(const std::string& format, Args&&... args)
	{
		char msg[128];
		sprintf_s(msg, format.c_str(), args...);
		Error(std::string(msg));
	}

	void Warning(const std::string&);

	template<class... Args>
	void Warning(const std::string& format, Args&&... args)
	{
		char msg[128];
		sprintf_s(msg, format.c_str(), args...);
		Warning(std::string(msg));
	}

	void Info(const std::string&);

	template<class... Args>
	void Info(const std::string& format, Args&&... args)
	{
		char msg[128];
		sprintf_s(msg, format.c_str(), args...);
		Info(std::string(msg));
	}

	void InitConsole(void);

	void InitFile(void);
};