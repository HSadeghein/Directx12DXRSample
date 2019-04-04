#include "Game.h"


#include"Application.h"
#include "Window.h"

#include<cassert>

Game::Game(const std::wstring & name, int width, int height, bool vSync)
	: g_Name(name), g_Width(width), g_Height(height), g_vSync(vSync)
{
}

Game::~Game()
{
	assert(!g_pWindow && "Use Game::Destroy() before destruction.");
}

bool Game::Initialize()
{
	if (!DirectX::XMVerifyCPUSupport())
	{
		OutputDebugString(L"Failed to Initialize DirectX math library");
		return false;
	}
	g_pWindow = Application::Get().CreateRenderWindow(g_Name, g_Width, g_Height, g_vSync);
	g_pWindow->RegisterCallbacks(shared_from_this());
	g_pWindow->Show();

	return true;
}

void Game::Destroy()
{
	Application::Get().DestroyWindow(g_pWindow);
	g_pWindow.reset();
}

void Game::OnUpdate(UpdateEventArgs& e)
{
	//g_pWindow->SetVSync(false);
	//std::wstring s = std::to_wstring(e.TotalTime) + L"\n";
	//OutputDebugString(s.c_str());
}

void Game::OnRender(RenderEventArgs& e)
{

}

void Game::OnKeyPressed(KeyEventArgs& e)
{
	// By default, do nothing.
}

void Game::OnKeyReleased(KeyEventArgs& e)
{
	// By default, do nothing.
}

void Game::OnMouseMoved(class MouseMotionEventArgs& e)
{
	// By default, do nothing.
}

void Game::OnMouseButtonPressed(MouseButtonEventArgs& e)
{
	// By default, do nothing.
}

void Game::OnMouseButtonReleased(MouseButtonEventArgs& e)
{
	// By default, do nothing.
}

void Game::OnMouseWheel(MouseWheelEventArgs& e)
{
	// By default, do nothing.
}

void Game::OnResize(ResizeEventArgs& e)
{
	g_Width = e.Width;
	g_Height = e.Height;
	g_aspectRatio = static_cast<float>(g_Width) / static_cast<float>(g_Height);

	g_pWindow->g_ClientHeight = g_Height;
	g_pWindow->g_ClientWidth = g_Width;

}

void Game::OnWindowDestroy()
{
	// If the Window which we are registered to is 
	// destroyed, then any resources which are associated 
	// to the window must be released.
	UnloadContent();
}

int Game::GetWidth() const
{
	return g_Width;
}

int Game::GetHeight() const
{
	return g_Height;
}

float Game::GetAspectRatio() const
{
	return g_aspectRatio;
}
