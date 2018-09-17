#pragma once

#include<memory>

namespace PrizmEngine
{
	class PerfTimer
	{
	private:
		class Impl;
		std::unique_ptr<Impl> impl_;

	public:
		PerfTimer();
		~PerfTimer();

		float Tick();
		void Start();
		void Stop();
		float DeltaTime() const;
		float TotalTime() const;
		void Reset();
	};
}