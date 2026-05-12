#include "my_window.h"
#include "my_application.h"
#include "my_camera.h"
#include <stdexcept>
#include <iostream>
#include <cmath>

MyWindow::MyWindow(int w, int h, std::string name) : 
	m_iWidth(w),
	m_iHeight(h),
	m_sWindowName(name),
	m_pWindow(nullptr),
	m_bframeBufferResize(false),
	m_pMyApplication(nullptr)
{
	_initWindow();
}

MyWindow::~MyWindow()
{
	glfwSetKeyCallback(m_pWindow, nullptr);
	glfwSetMouseButtonCallback(m_pWindow, nullptr);
	glfwSetFramebufferSizeCallback(m_pWindow, nullptr);
	glfwDestroyWindow(m_pWindow);
	glfwTerminate();
}

void MyWindow::_initWindow()
{
	glfwInit();

	// GLFW was designed initially to create OpenGL context by default.
	// By setting GLFW_CLIENT_API to GLFW_NO_API, it tells GLFW NOT to create OpenGL context 
	// (because we are going to use Vulkan)
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// Need to handle the window resizing in a specical way later in Vulkan code
	// so we need to set it to false initally until frameBufferResizeCallback is set
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_pWindow = glfwCreateWindow(m_iWidth, m_iHeight, m_sWindowName.c_str(), nullptr, nullptr);

	// For the call back function to use this pointer
	glfwSetWindowUserPointer(m_pWindow, this);

	// Register viewport resize callback
	glfwSetFramebufferSizeCallback(m_pWindow, s_frameBufferResizeCallback);

	// Register keyboard callback	
	glfwSetKeyCallback(m_pWindow, s_keyboardCallback);

	// Register mouse button callback
	glfwSetMouseButtonCallback(m_pWindow, s_mouseButtonCallback);
}

void MyWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
{
	if (glfwCreateWindowSurface(instance, m_pWindow, nullptr, surface) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create window");
	}
}

void MyWindow::s_frameBufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto mywindow = reinterpret_cast<MyWindow*>(glfwGetWindowUserPointer(window));
	mywindow->m_bframeBufferResize = true;
	mywindow->m_iWidth = width;
	mywindow->m_iHeight = height;
}

void MyWindow::s_keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // Handle keyboard events
	auto mywindow = reinterpret_cast<MyWindow*>(glfwGetWindowUserPointer(window));
	
	if ( key == GLFW_KEY_C || key == GLFW_KEY_ESCAPE ||                                                           // Default operation
		 key == GLFW_KEY_F || key == GLFW_KEY_R || key == GLFW_KEY_P || key == GLFW_KEY_Z || key == GLFW_KEY_T || // Camera operation
		 key == GLFW_KEY_B || key == GLFW_KEY_SPACE || key == GLFW_KEY_N || key == GLFW_KEY_M || 
		 key == GLFW_KEY_X || key == GLFW_KEY_G || key == GLFW_KEY_TAB ) // Surface operation
	{
		if (action == GLFW_PRESS)
			mywindow->keyboardEvent(key, true);
		if (action == GLFW_RELEASE)
			mywindow->keyboardEvent(key, false);
	}
}

void MyWindow::pollEvents()
{
	glfwPollEvents();

	double xpos = 0.0, ypos = 0.0;
	glfwGetCursorPos(m_pWindow, &xpos, &ypos);

	float fMousePos[2];
	fMousePos[0] = (float)xpos / (float)m_iWidth;
	fMousePos[1] = (float)ypos / (float)m_iHeight;

	m_pMyApplication->mouseMotionEvent(fMousePos[0], fMousePos[1]);

	// Wind: hold W and drag to set wind direction.
	// Direction = normalize(current - drag start). Strength is fixed (kWindScale).
	bool wNow = glfwGetKey(m_pWindow, GLFW_KEY_W) == GLFW_PRESS;
	if (wNow && !m_bWasWPressed)
	{
		m_windStartPos[0] = fMousePos[0];
		m_windStartPos[1] = fMousePos[1];
	}
	if (wNow)
	{
		float dx =   fMousePos[0] - m_windStartPos[0];
		float dy = -(fMousePos[1] - m_windStartPos[1]);  // flip Y: screen down = world -Y
		float len = sqrtf(dx * dx + dy * dy);
		if (len > 0.001f)
			m_pMyApplication->setWindPos(dx / len, dy / len);
		else
			m_pMyApplication->setWindPos(0.0f, 0.0f);
	}
	else
	{
		m_pMyApplication->setWindPos(0.0f, 0.0f);
	}
	m_bWasWPressed = wNow;
}

void MyWindow::waitEvents()
{
	glfwWaitEvents();
}

const char** MyWindow::getRequiredInstanceExtensions(uint32_t* extensionCount)
{
	return glfwGetRequiredInstanceExtensions(extensionCount);
}

void MyWindow::bindMyApplication(MyApplication* pMyApplication)
{
	m_pMyApplication = pMyApplication;
}

void MyWindow::keyboardEvent(int key, bool bKeyDown)
{
	if (m_pMyApplication == nullptr) return;

	if (key == GLFW_KEY_C && bKeyDown)
	{
		m_pMyApplication->switchProjectionMatrix();
	}
	else if (key == GLFW_KEY_ESCAPE && bKeyDown)
	{
		glfwSetWindowShouldClose(m_pWindow, 1);
	}
	else if (key == GLFW_KEY_F && bKeyDown)
	{
		m_pMyApplication->setCameraNavigationMode(MyCamera::MYCAMERA_FITALL);
	}
	else if (key == GLFW_KEY_R && bKeyDown)
	{
		m_pMyApplication->setCameraNavigationMode(bKeyDown ? MyCamera::MYCAMERA_ROTATE : MyCamera::MYCAMERA_NONE);
	}
	else if (key == GLFW_KEY_P && bKeyDown)
	{
		m_pMyApplication->setCameraNavigationMode(bKeyDown ? MyCamera::MYCAMERA_PAN : MyCamera::MYCAMERA_NONE);
	}
	else if (key == GLFW_KEY_Z && bKeyDown)
	{
		m_pMyApplication->setCameraNavigationMode(bKeyDown ? MyCamera::MYCAMERA_ZOOM : MyCamera::MYCAMERA_NONE);
	}
	else if (key == GLFW_KEY_T && bKeyDown)
	{
		m_pMyApplication->setCameraNavigationMode(bKeyDown ? MyCamera::MYCAMERA_TWIST : MyCamera::MYCAMERA_NONE);
	}
	else if (key == GLFW_KEY_SPACE && bKeyDown)
	{
	    m_pMyApplication->switchEditMode();
	}
	else if (key == GLFW_KEY_N && bKeyDown)
	{
		// N = traverse next scene graph node
		// Shift+N = show/hide surface normals (HW5)
		bool shiftHeld = glfwGetKey(m_pWindow, GLFW_KEY_LEFT_SHIFT)  == GLFW_PRESS ||
		                 glfwGetKey(m_pWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
		if (shiftHeld)
			m_pMyApplication->showHideNormalVectors();
		else
			m_pMyApplication->traverseNextNode();
	}
	else if (key == GLFW_KEY_M && bKeyDown)
	{
		m_pMyApplication->resetSurface();
	}
	else if (key == GLFW_KEY_X && bKeyDown)
	{
		m_pMyApplication->toggleTwist();
	}
	else if (key == GLFW_KEY_G && bKeyDown)
	{
		m_pMyApplication->toggleColorGradient();
	}
	else if (key == GLFW_KEY_B && bKeyDown)
	{
		m_pMyApplication->createBezierRevolutionSurface();
	}
	else if (key == GLFW_KEY_TAB && bKeyDown)
	{
		m_pMyApplication->printSceneGraph();
	}
}

void MyWindow::s_mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) return;

	auto mywindow = reinterpret_cast<MyWindow*>(glfwGetWindowUserPointer(window));

	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);

	float fMousePos[2];
	fMousePos[0] = (float)xpos / (float)mywindow->m_iWidth;
	fMousePos[1] = (float)ypos / (float)mywindow->m_iHeight;

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		mywindow->m_pMyApplication->mouseButtonEvent(true, fMousePos[0], fMousePos[1]);
	}
	else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
	{
		mywindow->m_pMyApplication->mouseButtonEvent(false, fMousePos[0], fMousePos[1]);
	}
}

