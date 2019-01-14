#pragma once
#include "Game.h"
#include"Window.h"
class Tutorial1 : public Game 
{
public:
	Tutorial1(const std::wstring & name, int width, int height, bool vSync);
	~Tutorial1();

	// Inherited via Game
	virtual bool LoadContent() override;
	virtual void UnloadContent() override;

	using super = Game;

protected:
	virtual void OnUpdate(UpdateEventArgs& e) override;
	virtual void OnRender(RenderEventArgs& e) override;

private:
	uint64_t g_FenceValues[Window::BufferCount] = {};

};

