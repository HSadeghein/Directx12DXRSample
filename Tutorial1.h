#pragma once
#include "Game.h"
class Tutorial1 : public Game 
{
public:
	Tutorial1(const std::wstring & name, int width, int height, bool vSync);
	~Tutorial1();

	// Inherited via Game
	virtual bool LoadContent() override;
	virtual void UnloadContent() override;
};

