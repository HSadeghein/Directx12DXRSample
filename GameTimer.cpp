#include "GameTimer.h"
#include <string>


GameTimer::GameTimer(std::string name) :mSecondsPerCount(0.0),mDeltaTime(-1.0),mBaseTime(0),mPausedTime(0),mPrevTime(0),mCurrTime(0),mStopped(false)
{
	mName = name;
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	mSecondsPerCount = 1 / (double)countsPerSec;

}


GameTimer::~GameTimer()
{
}

void GameTimer::Tick()
{
	if (mStopped)
	{
		mDeltaTime = 0.0;
		return;
	}
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;

	mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;

	mPrevTime = currTime;

	if (mDeltaTime < 0)
		mDeltaTime = 0;
}

void GameTimer::CalculateFrameStatics()
{
	//static int frameCount = 0;
	//static float elapsedTime = 0.0;
	//frameCount++;
	mFrameCount++;
	if ((GetTotalTime() - mElapsedTime) >= 1.0f)
	{
		float fps = mFrameCount;
		float framePeriod = (1 / fps) * 1000;

		std::wstring text;
		text += L"FPS: " + std::wstring(mName.begin(), mName.end()) + std::to_wstring(fps) + L"\n";
		OutputDebugString(text.c_str());
		text = L"";
		text += L"FramePeriod: " + std::to_wstring(framePeriod) + L"\n";
		OutputDebugString(text.c_str());

		mFrameCount = 0;
		mElapsedTime += 1.0;
	}
}

float GameTimer::GetDeltaTime() const
{
	return mDeltaTime;
}

float GameTimer::GetGameTime() const
{
	return mSecondsPerCount;
}

void GameTimer::Reset()
{
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	//mCurrTime = currTime;
	mPrevTime = currTime;
	mBaseTime = currTime;
	mStopTime = 0;
	mStopped = false;
}

void GameTimer::Stop()
{
	if (!mStopped)
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		mStopTime = currTime;
		mStopped = true;
	}
}

void GameTimer::Start()
{
	__int64 startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);


	if (mStopped)
	{
		mPausedTime += (startTime - mStopTime);

		mPrevTime = startTime;

		mStopped = false;
		mStopTime = 0.0;
	}
}

float GameTimer::GetTotalTime() const
{


	if(mStopped)
	{
		return (float)((mStopTime - mPausedTime - mBaseTime) * mSecondsPerCount);
	}
	else
	{
		return (float)((mCurrTime - mPausedTime - mBaseTime) * mSecondsPerCount);
	}
}
