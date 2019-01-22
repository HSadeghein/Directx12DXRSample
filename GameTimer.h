#pragma once
#include<Windows.h>
#include<string>
class GameTimer
{
public:
	GameTimer(std::string name);
	~GameTimer();

	float GetGameTime() const;
	float GetDeltaTime() const;
	float GetTotalTime() const;
	void Reset();
	void Start();
	void Stop();
	void Tick();
	void CalculateFrameStatics();




private:
	double mSecondsPerCount;
	double mDeltaTime;

	__int64 mBaseTime;
	__int64 mPausedTime;
	__int64 mStopTime;
	__int64 mPrevTime;
	__int64 mCurrTime;

	bool mStopped;

	std::string mName;
	int mFrameCount = 0;
	float mElapsedTime = 0;
};

