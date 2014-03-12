#ifndef GLF_WINDOW_INCLUDED
#define GLF_WINDOW_INCLUDED

#include <GLFW/glfw3.h>

namespace glf
{
	class window
	{
	public:
		static void init();
		static void terminate();

		window();
		~window();

		void swap();
		void makeCurrent();

	private:
		GLFWwindow Window;
	};

	inline window::window
	(
		int Width, 
		int Height,
		int mode, 
		const char* title
	) :
		Window(glfwCreateWindow(Width, Height, Mode, Title, NULL))
	{}

	inline window::~window()
	{
		glfwDestroyWindow(this->Window);
	}

	inline void window::swap()
	{
		glfwSwapBuffers(this->Window);
	}

	inline void window::makeCurrent()
	{
		glfwMakeContextCurrent(this->Window);
	}

}//namespace glf

#endif
