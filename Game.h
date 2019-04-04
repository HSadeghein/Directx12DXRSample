#pragma once
#include<memory>
#include<string>
#include<DirectXMath.h>
#include "Events.h"
class Window;
class Game : public std::enable_shared_from_this<Game>
{
public:

	Game(const std::wstring& name, int width, int height, bool vSync);
	virtual ~Game();

	virtual bool Initialize();

	virtual bool LoadContent() = 0;

	virtual void UnloadContent() = 0;

	virtual void Destroy();

protected:
	friend class Window;
	std::shared_ptr<Window> g_pWindow;
	
	/**
	 *  Update the game logic.
	 */
	virtual void OnUpdate(UpdateEventArgs& e);

	/**
	 *  Render stuff.
	 */
	virtual void OnRender(RenderEventArgs& e);

	/**
	 * Invoked by the registered window when a key is pressed
	 * while the window has focus.
	 */
	virtual void OnKeyPressed(KeyEventArgs& e);

	/**
	 * Invoked when a key on the keyboard is released.
	 */
	virtual void OnKeyReleased(KeyEventArgs& e);

	/**
	 * Invoked when the mouse is moved over the registered window.
	 */
	virtual void OnMouseMoved(MouseMotionEventArgs& e);

	/**
	 * Invoked when a mouse button is pressed over the registered window.
	 */
	virtual void OnMouseButtonPressed(MouseButtonEventArgs& e);

	/**
	 * Invoked when a mouse button is released over the registered window.
	 */
	virtual void OnMouseButtonReleased(MouseButtonEventArgs& e);

	/**
	 * Invoked when the mouse wheel is scrolled while the registered window has focus.
	 */
	virtual void OnMouseWheel(MouseWheelEventArgs& e);

	/**
	 * Invoked when the attached window is resized.
	 */
	virtual void OnResize(ResizeEventArgs& e);

	/**
	 * Invoked when the registered window instance is destroyed.
	 */
	virtual void OnWindowDestroy();

	int GetWidth() const;
	int GetHeight() const;
	float GetAspectRatio() const;

private:
	std::wstring g_Name;
	int g_Width;
	int g_Height;
	float g_aspectRatio;
	bool g_vSync;
};

