#include "Tutorial1.h"



Tutorial1::Tutorial1(const std::wstring & name, int width, int height, bool vSync) : Game(name, width, height, vSync)
{

}


Tutorial1::~Tutorial1()
{
}

bool Tutorial1::LoadContent()
{
	return true;
}

void Tutorial1::UnloadContent()
{
}
