/**
 * VKTS - VulKan ToolS.
 *
 * The MIT License (MIT)
 *
 * Copyright (c) since 2014 Norbert Nopper
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <vkts/vkts.hpp>

#include "fn_visual_internal.hpp"
#include "fn_visual_gamepad_internal.hpp"
#include "fn_visual_wayland_internal.hpp"

#define VKTS_WINDOWS_MAX_WINDOWS 64

#include <linux/input.h>
#include <poll.h>
#include <unistd.h>

#ifdef VKTS_TEST
#include "fn_visual_wayland_test_internal.hpp"
#endif

namespace vkts
{

#ifdef VKTS_TEST
// For testing, only one window can be created.
struct wl_buffer* surfaceBuffer = nullptr;
#endif

static struct wl_cursor_theme* g_nativeCursorTheme = nullptr;

static struct wl_cursor* g_nativeCursor = nullptr;

static struct wl_surface* g_nativeCursorSurface = nullptr;

static struct wl_buffer* g_nativeCursorBuffer = nullptr;


//

static VkBool32 g_running = VK_TRUE;

static struct wl_touch* g_nativeTouch = nullptr;

static struct wl_keyboard* g_nativeKeyboard = nullptr;

static struct wl_pointer* g_nativePointer = nullptr;

static struct wl_seat* g_nativeSeat = nullptr;

static struct wl_shell* g_nativeShell = nullptr;

static struct wl_shm* g_nativeShm = nullptr;

static struct wl_compositor* g_nativeCompositor = nullptr;

static struct wl_registry* g_nativeRegistry = nullptr;

static int g_nativeFd = -1;

static struct wl_display* g_nativeDisplay = nullptr;

typedef struct _VKTS_NATIVE_DISPLAY_CONTAINER
{

    int32_t index;

    int32_t x;
    int32_t y;

    int32_t width;
    int32_t height;

    VkBool32 defaultDisplay;

    wl_output* output;

    NativeDisplaySP display;

} VKTS_NATIVE_DISPLAY_CONTAINER;

static NativeDisplaySP g_defaultDisplay;

static std::map<int32_t, VKTS_NATIVE_DISPLAY_CONTAINER*> g_allDisplays;

typedef struct _VKTS_NATIVE_WINDOW_CONTAINER
{

    int32_t index;

    struct wl_surface* surface;
    struct wl_shell_surface* shell_surface;
    struct wl_region* region;

    NativeWindowSP nativeWindow;

} VKTS_NATIVE_WINDOW_CONTAINER;

static std::map<struct wl_surface*, VKTS_NATIVE_WINDOW_CONTAINER*> g_allWindows;

//

static int32_t g_numberWindows = 0;

static uint64_t g_windowBits = 0;


static struct wl_surface* g_currentKeySurface = nullptr;

static struct wl_surface* g_currentPointerSurface = nullptr;

static struct wl_surface* g_currentTouchSurface = nullptr;

//

void VKTS_APIENTRY _visualWaylandKeymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size)
{
	// Nothing for now.
}

void VKTS_APIENTRY _visualWaylandKeyEnter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
	g_currentKeySurface = surface;
}

void VKTS_APIENTRY _visualWaylandKeyLeave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface)
{
	g_currentKeySurface = nullptr;
}

void VKTS_APIENTRY _visualWaylandKey(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    int32_t keyCode = _visualTranslateKey(key);

    if (keyCode == VKTS_KEY_ESCAPE)
    {
    	g_running = VK_FALSE;

        return;
    }

	if (!g_currentKeySurface)
	{
		return;
	}

	auto currentWindowContainer = g_allWindows.find(g_currentKeySurface);

	if (currentWindowContainer == g_allWindows.end())
	{
		return;
	}

    if (keyCode != VKTS_KEY_UNKNOWN)
    {
    	currentWindowContainer->second->nativeWindow->getKeyInput().setKey(keyCode, state == WL_KEYBOARD_KEY_STATE_PRESSED);
    }
}

void VKTS_APIENTRY _visualWaylandModifiers(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
	// Nothing for now.
}

void VKTS_APIENTRY _visualWaylandRepeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay)
{
	// Nothing for now.
}

//

void VKTS_APIENTRY _visualWaylandEnter(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	g_currentPointerSurface = surface;

	//

	if (!g_currentPointerSurface)
	{
		return;
	}

	auto currentWindowContainer = g_allWindows.find(g_currentPointerSurface);

	if (currentWindowContainer == g_allWindows.end())
	{
		return;
	}

	if (currentWindowContainer->second->nativeWindow->isInvisibleCursor())
	{
		wl_pointer_set_cursor(wl_pointer, serial, nullptr, 0, 0);
	}
	else
	{
		wl_surface_attach(g_nativeCursorSurface, g_nativeCursorBuffer, 0, 0);
		wl_surface_damage(g_nativeCursorSurface, 0, 0, g_nativeCursor->images[0]->width, g_nativeCursor->images[0]->height);
		wl_surface_commit(g_nativeCursorSurface);

		wl_pointer_set_cursor(wl_pointer, serial, g_nativeCursorSurface, g_nativeCursor->images[0]->hotspot_x, g_nativeCursor->images[0]->hotspot_y);
	}
}

void VKTS_APIENTRY _visualWaylandLeave(void *data, struct wl_pointer* wl_pointer, uint32_t serial, struct wl_surface *surface)
{
	if (!g_currentPointerSurface)
	{
		return;
	}

	auto currentWindowContainer = g_allWindows.find(g_currentPointerSurface);

	if (currentWindowContainer == g_allWindows.end())
	{
		return;
	}

	if (currentWindowContainer->second->nativeWindow->isInvisibleCursor())
	{
		wl_surface_attach(g_nativeCursorSurface, g_nativeCursorBuffer, 0, 0);
		wl_surface_damage(g_nativeCursorSurface, 0, 0, g_nativeCursor->images[0]->width, g_nativeCursor->images[0]->height);
		wl_surface_commit(g_nativeCursorSurface);

		wl_pointer_set_cursor(wl_pointer, serial, g_nativeCursorSurface, g_nativeCursor->images[0]->hotspot_x, g_nativeCursor->images[0]->hotspot_y);
	}

	//

	g_currentPointerSurface = nullptr;
}

void VKTS_APIENTRY _visualWaylandMotion(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	if (!g_currentPointerSurface)
	{
		return;
	}

	auto currentWindowContainer = g_allWindows.find(g_currentPointerSurface);

	if (currentWindowContainer == g_allWindows.end())
	{
		return;
	}

	currentWindowContainer->second->nativeWindow->getMouseInput().setLocation(glm::ivec2(wl_fixed_to_int(surface_x), wl_fixed_to_int(surface_y)));
}

void VKTS_APIENTRY _visualWaylandButton(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
	if (!g_currentPointerSurface)
	{
		return;
	}

	auto currentWindowContainer = g_allWindows.find(g_currentPointerSurface);

	if (currentWindowContainer == g_allWindows.end())
	{
		return;
	}

	currentWindowContainer->second->nativeWindow->getMouseInput().setButton(button - BTN_LEFT, state == WL_POINTER_BUTTON_STATE_PRESSED);
}

void VKTS_APIENTRY _visualWaylandAxis(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
	if (!g_currentPointerSurface)
	{
		return;
	}

	auto currentWindowContainer = g_allWindows.find(g_currentPointerSurface);

	if (currentWindowContainer == g_allWindows.end())
	{
		return;
	}

	if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
	{
		// Division by ten to adapt to internal format.
		currentWindowContainer->second->nativeWindow->getMouseInput().setMouseWheel(wl_fixed_to_int(value) / 10);
	}
}

//

typedef struct _TouchInfo {
	VkBool32 pressed;
	int32_t x;
	int32_t y;

	int32_t id;
	VkBool32 valid;
} TouchInfo;

static TouchInfo touchInfo[VKTS_MAX_TOUCHPAD_SLOTS];

VkBool32 VKTS_APIENTRY _visualInitTouchpad(const VkInstance instance, const VkPhysicalDevice physicalDevice)
{
	for (int32_t i = 0; i < VKTS_MAX_TOUCHPAD_SLOTS; i++)
	{
		touchInfo[i].pressed = VK_FALSE;
		touchInfo[i].x = 0;
		touchInfo[i].y = 0;

		touchInfo[i].id = -1;
		touchInfo[i].valid = VK_FALSE;
	}

	return VK_TRUE;
}

void  VKTS_APIENTRY _visualWaylandDown(void *data, struct wl_touch *wl_touch, uint32_t serial, uint32_t time, struct wl_surface *surface, int32_t id, wl_fixed_t x, wl_fixed_t y)
{
	g_currentTouchSurface = surface;

	if (!g_currentTouchSurface)
	{
		return;
	}

	auto currentWindowContainer = g_allWindows.find(g_currentTouchSurface);

	if (currentWindowContainer == g_allWindows.end())
	{
		return;
	}

	for (int32_t slot = 0; slot < VKTS_MAX_TOUCHPAD_SLOTS; slot++)
	{
		if (!touchInfo[slot].valid)
		{
			touchInfo[slot].valid = VK_TRUE;

			touchInfo[slot].id = id;

			touchInfo[slot].pressed = VK_TRUE;
			touchInfo[slot].x = wl_fixed_to_int(x);
			touchInfo[slot].y = wl_fixed_to_int(y);

			currentWindowContainer->second->nativeWindow->getTouchpadInput().setLocation(slot, glm::ivec2(touchInfo[slot].x, touchInfo[slot].y));
			currentWindowContainer->second->nativeWindow->getTouchpadInput().setPressed(slot, VK_TRUE);
		}
	}
}

void VKTS_APIENTRY _visualWaylandUp(void *data, struct wl_touch *wl_touch, uint32_t serial, uint32_t time, int32_t id)
{
	if (!g_currentTouchSurface)
	{
		return;
	}

	auto currentWindowContainer = g_allWindows.find(g_currentTouchSurface);

	if (currentWindowContainer == g_allWindows.end())
	{
		return;
	}

	for (int32_t slot = 0; slot < VKTS_MAX_TOUCHPAD_SLOTS; slot++)
	{
		if (touchInfo[slot].valid && touchInfo[slot].id == id)
		{
			touchInfo[slot].valid = VK_FALSE;

			touchInfo[slot].id = -1;

			touchInfo[slot].pressed = VK_FALSE;

			currentWindowContainer->second->nativeWindow->getTouchpadInput().setPressed(slot, VK_FALSE);
		}
	}

	//

	g_currentTouchSurface = nullptr;
}

void  VKTS_APIENTRY _visualWaylandTouchMotion(void *data, struct wl_touch *wl_touch, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y)
{
	if (!g_currentTouchSurface)
	{
		return;
	}

	auto currentWindowContainer = g_allWindows.find(g_currentTouchSurface);

	if (currentWindowContainer == g_allWindows.end())
	{
		return;
	}

	for (int32_t slot = 0; slot < VKTS_MAX_TOUCHPAD_SLOTS; slot++)
	{
		if (touchInfo[slot].valid && touchInfo[slot].id == id)
		{
			touchInfo[slot].x = wl_fixed_to_int(x);
			touchInfo[slot].y = wl_fixed_to_int(y);

			currentWindowContainer->second->nativeWindow->getTouchpadInput().setLocation(slot, glm::ivec2(touchInfo[slot].x, touchInfo[slot].y));
		}
	}
}

void  VKTS_APIENTRY _visualWaylandFrame(void *data, struct wl_touch *wl_touch)
{
	// Nothing for now.
}

void  VKTS_APIENTRY _visualWaylandCancel(void *data, struct wl_touch *wl_touch)
{
	// Nothing for now.
}

//

static const struct wl_keyboard_listener keyboard_listener =
{
	_visualWaylandKeymap,
	_visualWaylandKeyEnter,
	_visualWaylandKeyLeave,
	_visualWaylandKey,
	_visualWaylandModifiers,
	_visualWaylandRepeat_info
};

static const struct wl_pointer_listener pointer_listener =
{
	_visualWaylandEnter,
	_visualWaylandLeave,
	_visualWaylandMotion,
	_visualWaylandButton,
	_visualWaylandAxis
};

static const struct wl_touch_listener touch_listener =
{
		_visualWaylandDown,
		_visualWaylandUp,
		_visualWaylandTouchMotion,
		_visualWaylandFrame,
		_visualWaylandCancel
};

void _visualWaylandCapabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities)
{
    if (capabilities & WL_SEAT_CAPABILITY_POINTER)
    {
    	g_nativePointer = wl_seat_get_pointer(g_nativeSeat);

    	//

    	wl_pointer_add_listener(g_nativePointer, &pointer_listener, nullptr);
    }

    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
    {
    	g_nativeKeyboard = wl_seat_get_keyboard(g_nativeSeat);

    	//

    	wl_keyboard_add_listener(g_nativeKeyboard, &keyboard_listener, nullptr);
	}

    if (capabilities & WL_SEAT_CAPABILITY_TOUCH)
    {
    	g_nativeTouch = wl_seat_get_touch(g_nativeSeat);

    	//

    	wl_touch_add_listener(g_nativeTouch, &touch_listener, nullptr);
    }
}

void _visualWaylandName(void *data, struct wl_seat *wl_seat, const char *name)
{
}

static const struct wl_seat_listener seat_listener =
{
		_visualWaylandCapabilities, _visualWaylandName
};


static void _visualWaylandPing(void* data, struct wl_shell_surface* shell_surface, uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
}

void _visualWaylandConfigure(void* data, struct wl_shell_surface* shell_surface, uint32_t edges, int32_t width, int32_t height)
{
	if (!data)
	{
		return;
	}

	VKTS_NATIVE_WINDOW_CONTAINER* currentWindowContainer = (VKTS_NATIVE_WINDOW_CONTAINER*)data;

	currentWindowContainer->nativeWindow->setDimension(glm::uvec2((uint32_t)width, (uint32_t)height));

	//

	if (currentWindowContainer->region)
	{
		wl_region_destroy(currentWindowContainer->region);
	}

	currentWindowContainer->region = wl_compositor_create_region(g_nativeCompositor);

	if (!currentWindowContainer->region)
	{
		wl_region_add(currentWindowContainer->region, 0, 0, width, height);
		wl_surface_set_opaque_region(currentWindowContainer->surface, currentWindowContainer->region);
	}
}

void _visualWaylandPopupDone(void* data, struct wl_shell_surface* shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener =
{
	_visualWaylandPing, _visualWaylandConfigure, _visualWaylandPopupDone
};

//

static void _visualWaylandGeometry(void* data, struct wl_output* output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel, const char* make, const char* model, int32_t transform)
{
	VKTS_NATIVE_DISPLAY_CONTAINER* currentDisplayContainer = (VKTS_NATIVE_DISPLAY_CONTAINER*)data;

	if (!currentDisplayContainer)
	{
		return;
	}

	currentDisplayContainer->x = x;
	currentDisplayContainer->y = y;

	if (x == 0 && y == 0)
	{
		currentDisplayContainer->defaultDisplay = VK_TRUE;
	}
}

static void _visualWaylandMode(void* data, struct wl_output* output, uint32_t flags, int32_t width, int32_t height, int32_t refresh)
{
	VKTS_NATIVE_DISPLAY_CONTAINER* currentDisplayContainer = (VKTS_NATIVE_DISPLAY_CONTAINER*)data;

	if (!currentDisplayContainer)
	{
		return;
	}

	currentDisplayContainer->width = width;
	currentDisplayContainer->height = height;
}

static void _visualWaylandDone(void* data, struct wl_output* output)
{
	// Do nothing for now.
}

static void _visualWaylandScale(void* data, struct wl_output* output, int32_t factor)
{
	// Do nothing for now.
}

static const struct wl_output_listener g_output_listener = {
	_visualWaylandGeometry,
	_visualWaylandMode,
	_visualWaylandDone,
	_visualWaylandScale
};

static void _visualWaylandGlobal(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	if (strcmp(interface, "wl_shm") == 0)
	{
		g_nativeShm = (struct wl_shm*)wl_registry_bind(registry, name, &wl_shm_interface, version);

		//

		g_nativeCursorTheme = wl_cursor_theme_load(nullptr, 32, g_nativeShm);

		if (!g_nativeCursorTheme)
		{
			return;
		}

		g_nativeCursor = wl_cursor_theme_get_cursor(g_nativeCursorTheme, "left_ptr");

		if (!g_nativeCursor)
		{
			return;
		}

		g_nativeCursorSurface = wl_compositor_create_surface(g_nativeCompositor);

		if (!g_nativeCursorSurface)
		{
			return;
		}

		g_nativeCursorBuffer = wl_cursor_image_get_buffer(g_nativeCursor->images[0]);

		if (!g_nativeCursorBuffer)
		{
			return;
		}

		wl_surface_attach(g_nativeCursorSurface, g_nativeCursorBuffer, 0, 0);
		wl_surface_damage(g_nativeCursorSurface, 0, 0, g_nativeCursor->images[0]->width, g_nativeCursor->images[0]->height);
		wl_surface_commit(g_nativeCursorSurface);
	}
  	else if (strcmp(interface, "wl_compositor") == 0)
    {
    	g_nativeCompositor = (struct wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, version);
    }
	else if (strcmp(interface, "wl_shell") == 0)
	{
        g_nativeShell = (struct wl_shell*)wl_registry_bind(registry, name, &wl_shell_interface, version);
    }
    else if (strcmp(interface, "wl_output") == 0)
    {
    	VKTS_NATIVE_DISPLAY_CONTAINER* currentDisplayContainer = new VKTS_NATIVE_DISPLAY_CONTAINER();

    	if (!currentDisplayContainer)
    	{
    		return;
    	}

    	currentDisplayContainer->index = g_allDisplays.size();

    	currentDisplayContainer->x = -1;
    	currentDisplayContainer->y = -1;

    	currentDisplayContainer->width = -1;
    	currentDisplayContainer->height = -1;

    	currentDisplayContainer->defaultDisplay = VK_FALSE;

    	currentDisplayContainer->output = (wl_output*)wl_registry_bind(registry, name, &wl_output_interface, version);

    	currentDisplayContainer->display = NativeDisplaySP();


    	g_allDisplays[currentDisplayContainer->index] = currentDisplayContainer;

    	//

    	wl_output_add_listener(currentDisplayContainer->output, &g_output_listener, currentDisplayContainer);
    }
    else if (strcmp(interface, "wl_seat") == 0)
    {
    	g_nativeSeat = (struct wl_seat *)wl_registry_bind(registry, name, &wl_seat_interface, version);

    	wl_seat_add_listener(g_nativeSeat, &seat_listener, nullptr);
    }
}

static void _visualWaylandGlobal_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	// Do nothing for now.
}

static const struct wl_registry_listener registry_listener = {
	_visualWaylandGlobal,
	_visualWaylandGlobal_remove
};

//

VkBool32 VKTS_APIENTRY _visualInit(const VkInstance instance, const VkPhysicalDevice physicalDevice)
{
	g_nativeDisplay = wl_display_connect(nullptr);

    if (!g_nativeDisplay)
    {
    	return VK_FALSE;
    }

    g_nativeFd = wl_display_get_fd(g_nativeDisplay);

    if (g_nativeFd < 0)
    {
    	return VK_FALSE;
    }

    g_nativeRegistry = wl_display_get_registry(g_nativeDisplay);
    wl_registry_add_listener(g_nativeRegistry, &registry_listener, nullptr);

    wl_display_dispatch(g_nativeDisplay);
    wl_display_roundtrip(g_nativeDisplay);

	if (!_visualInitGamepad(instance, physicalDevice))
	{
		return VK_FALSE;
	}

	if (!_visualInitTouchpad(instance, physicalDevice))
	{
		return VK_FALSE;
	}

	if (!_visualInitKey())
	{
		return VK_FALSE;
	}

	g_running = VK_TRUE;

	return VK_TRUE;
}

VkBool32 VKTS_APIENTRY _visualDispatchMessages()
{
	if (!g_running)
	{
		return VK_FALSE;
	}

	if (g_nativeDisplay)
	{
		struct pollfd fds;

		fds.fd = g_nativeFd;
		fds.events = POLLIN | POLLOUT | POLLERR | POLLHUP;

		wl_display_dispatch_pending(g_nativeDisplay);
		wl_display_flush(g_nativeDisplay);

		if (poll(&fds, 1, 0) > 0)
		{
			if (fds.revents & (POLLERR | POLLHUP))
			{
				return VK_FALSE;
			}

			if (fds.revents & POLLIN)
			{
				wl_display_dispatch(g_nativeDisplay);
			}
			if (fds.revents & POLLOUT)
			{
				wl_display_flush(g_nativeDisplay);
			}
		}
	}

    _visualDispatchMessagesGamepad();

    return VK_TRUE;
}

//

int32_t VKTS_APIENTRY _visualGetNumberDisplays()
{
    return g_allDisplays.size();
}

INativeDisplayWP VKTS_APIENTRY _visualCreateDefaultDisplay()
{
    if (g_defaultDisplay.get())
    {
        return g_defaultDisplay;
    }

    //

    for (auto walkerDisplayContainer : g_allDisplays)
    {
        if (walkerDisplayContainer.second && walkerDisplayContainer.second->defaultDisplay)
        {
        	walkerDisplayContainer.second->display = NativeDisplaySP(new NativeDisplay(walkerDisplayContainer.second->index, walkerDisplayContainer.second->defaultDisplay, walkerDisplayContainer.second->width, walkerDisplayContainer.second->height, g_nativeDisplay));

        	if (!walkerDisplayContainer.second->display.get())
        	{
        		return INativeDisplayWP();
        	}

        	g_defaultDisplay = walkerDisplayContainer.second->display;

        	return g_defaultDisplay;
        }
    }

    return INativeDisplayWP();
}

INativeDisplayWP VKTS_APIENTRY _visualCreateDisplay(const int32_t displayIndex)
{
    if (displayIndex < 0 || displayIndex >= _visualGetNumberDisplays())
    {
        return INativeDisplayWP();
    }

    //

    auto currentDisplayContainer = g_allDisplays[displayIndex];

    if (!currentDisplayContainer)
    {
    	return INativeDisplaySP();
    }

    currentDisplayContainer->display = NativeDisplaySP(new NativeDisplay(currentDisplayContainer->index, currentDisplayContainer->defaultDisplay, currentDisplayContainer->width, currentDisplayContainer->height, g_nativeDisplay));

    if (currentDisplayContainer->defaultDisplay)
    {
    	g_defaultDisplay = currentDisplayContainer->display;
    }

    return currentDisplayContainer->display;
}

NativeDisplaySP VKTS_APIENTRY _visualGetDisplay(const int32_t displayIndex)
{
    if (displayIndex < 0 || displayIndex >= _visualGetNumberDisplays())
    {
        return NativeDisplaySP();
    }

    //

    auto currentDisplayContainer = g_allDisplays[displayIndex];

    if (!currentDisplayContainer)
    {
    	return NativeDisplaySP();
    }


    return currentDisplayContainer->display;
}

const SmartPointerVector<NativeDisplaySP>& VKTS_APIENTRY _visualGetActiveDisplays()
{
    static SmartPointerVector<NativeDisplaySP> displayList;

    displayList.clear();

    for (auto& walkerDisplayContainer : g_allDisplays)
    {
        if (walkerDisplayContainer.second && walkerDisplayContainer.second->display.get())
        {
        	displayList.append(walkerDisplayContainer.second->display);
        }
    }

    return displayList;
}

void VKTS_APIENTRY _visualDestroyDisplay(const NativeDisplaySP& display)
{
    if (display.get() == g_defaultDisplay.get())
    {
        g_defaultDisplay = NativeDisplaySP();
    }

    for (auto& walkerDisplayContainer : g_allDisplays)
    {
        if (walkerDisplayContainer.second && walkerDisplayContainer.second->display.get())
        {
        	walkerDisplayContainer.second->display = NativeDisplaySP();
        }
    }
}

//

INativeWindowWP VKTS_APIENTRY _visualCreateWindow(const INativeDisplayWP& display, const char* title, const int32_t width, const int32_t height, const VkBool32 fullscreen, const VkBool32 resize, const VkBool32 invisibleCursor)
{
    const auto sharedDisplay = display.lock();

    if (!sharedDisplay.get())
    {
        logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "No display.");

        return INativeWindowWP();
    }

    if (sharedDisplay->getIndex() < 0 || sharedDisplay->getIndex() >= _visualGetNumberDisplays())
    {
        logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Invalid display index.");

        return INativeWindowWP();
    }

    auto walkerDisplayContainer = g_allDisplays.find(sharedDisplay->getIndex());

    if (walkerDisplayContainer == g_allDisplays.end() || !walkerDisplayContainer->second || !walkerDisplayContainer->second->display)
    {
        logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Display not known.");

        return INativeWindowWP();
    }

    //

    VkBool32 finalResize = resize && !fullscreen;

    //

    uint32_t currentWindowBit = 1;
    int32_t currentWindowIndex = 0;

    if (g_numberWindows == VKTS_WINDOWS_MAX_WINDOWS)
    {
        logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Maximum windows reached.");

        return INativeWindowWP();
    }

    while (currentWindowBit & g_windowBits)
    {
        currentWindowBit = currentWindowBit << 1;

        currentWindowIndex++;
    }

    //

    // TODO: Implement game mode cursor.

	auto surface = wl_compositor_create_surface(g_nativeCompositor);

	if (!surface)
	{
		return INativeWindowWP();
	}

	auto shell_surface = wl_shell_get_shell_surface(g_nativeShell, surface);

	if (!shell_surface)
	{
		wl_surface_destroy(surface);

		return INativeWindowWP();
	}

	auto region = wl_compositor_create_region(g_nativeCompositor);

	if (!region)
	{
		wl_shell_surface_destroy(shell_surface);

		wl_surface_destroy(surface);

		return INativeWindowWP();
	}

	auto windowContainer = new VKTS_NATIVE_WINDOW_CONTAINER();

	if (!windowContainer)
	{
		wl_region_destroy(region);

		wl_shell_surface_destroy(shell_surface);

		wl_surface_destroy(surface);

		return INativeWindowWP();
	}

	//

	auto nativeWindow = NativeWindowSP(new NativeWindow(display, surface, currentWindowIndex, title, width, height, fullscreen, finalResize, invisibleCursor));

	if (!nativeWindow.get())
	{
		wl_region_destroy(region);

		wl_shell_surface_destroy(shell_surface);

		wl_surface_destroy(surface);

		return INativeWindowWP();
	}

	//

	windowContainer->index = currentWindowIndex;
	windowContainer->surface = surface;
	windowContainer->shell_surface = shell_surface;
	windowContainer->region = region;
	windowContainer->nativeWindow = nativeWindow;

	g_allWindows[surface] = windowContainer;

	//

	wl_shell_surface_add_listener(shell_surface, &shell_surface_listener, windowContainer);

    if (fullscreen)
    {
    	wl_shell_surface_set_fullscreen(shell_surface, WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT, 0, walkerDisplayContainer->second->output);
    }
    else
    {
    	wl_shell_surface_set_toplevel(shell_surface);
    	wl_shell_surface_set_title(shell_surface, title);
    }

	wl_region_add(region, 0, 0, width, height);
	wl_surface_set_opaque_region(surface, region);

#ifdef VKTS_TEST
	surfaceBuffer = _waylandTestCreateBuffer(g_nativeShm, width, height);

	if (!surfaceBuffer)
	{
		return INativeWindowWP();
	}

    wl_surface_attach(surface, surfaceBuffer, 0, 0);
    wl_surface_damage(surface, 0, 0, width, height);
    wl_surface_frame(surface);
    wl_surface_commit(surface);
#endif

    _visualWaylandConfigure(windowContainer, shell_surface, 0, width, height);

    wl_display_dispatch_pending(g_nativeDisplay);

    logPrint(VKTS_LOG_INFO, __FILE__, __LINE__, "Using Wayland, title is not shown and resize is false");

    return nativeWindow;
}

NativeWindowSP VKTS_APIENTRY _visualGetWindow(const int32_t windowIndex)
{
	for (auto& currentWindowContainer : g_allWindows)
	{
		if (currentWindowContainer.second->index == windowIndex)
		{
			return currentWindowContainer.second->nativeWindow;
		}
	}

    return NativeWindowSP();
}

const SmartPointerVector<NativeWindowSP>& VKTS_APIENTRY _visualGetActiveWindows()
{
    static SmartPointerVector<NativeWindowSP> windowList;

    windowList.clear();

	for (auto& currentWindowContainer : g_allWindows)
	{
		windowList.append(currentWindowContainer.second->nativeWindow);
	}

    return windowList;
}

void VKTS_APIENTRY _visualDestroyWindow(const NativeWindowSP& window)
{
    if (!window.get())
    {
        return;
    }

	auto foundWindowContainerWalker = g_allWindows.find(window->getNativeWindow());

	if (foundWindowContainerWalker == g_allWindows.end())
	{
		return;
	}

    if (window->getNativeWindow())
    {

        if (window->isFullscreen())
        {
        	wl_shell_surface_set_toplevel(foundWindowContainerWalker->second->shell_surface);
        }

        if (window->isInvisibleCursor())
        {
    		// Automatically reverted.
        }

        wl_region_destroy(foundWindowContainerWalker->second->region);
        wl_shell_surface_destroy(foundWindowContainerWalker->second->shell_surface);
        wl_surface_destroy(foundWindowContainerWalker->second->surface);

        delete foundWindowContainerWalker->second;

#ifdef VKTS_TEST
        if (surfaceBuffer)
        {
        	wl_buffer_destroy(surfaceBuffer);

        	surfaceBuffer = nullptr;
        }
#endif
    }

    uint32_t currentNegativeWindowBit = ~(1 << window->getIndex());

    g_windowBits &= currentNegativeWindowBit;

    //

    if (window->getNativeWindow())
    {
        g_allWindows.erase(foundWindowContainerWalker);
    }
}

//

void VKTS_APIENTRY _visualTerminate()
{
    _visualTerminateGamepad();

    //

    while (g_allWindows.size() > 0)
    {
        _visualDestroyWindow(g_allWindows.begin()->second->nativeWindow);
    }
    g_allWindows.clear();

    //

    for (auto& walkerDisplayContainer : g_allDisplays)
    {
        if (walkerDisplayContainer.second)
        {
            _visualDestroyDisplay(walkerDisplayContainer.second->display);

            delete walkerDisplayContainer.second;
        }
    }
    g_allDisplays.clear();

    g_defaultDisplay.reset();

    //

    if (g_nativeTouch)
    {
    	wl_touch_destroy(g_nativeTouch);

    	g_nativeTouch = nullptr;
    }

    if (g_nativeKeyboard)
    {
    	wl_keyboard_destroy(g_nativeKeyboard);

    	g_nativeKeyboard = nullptr;
    }

    if (g_nativePointer)
    {
    	wl_pointer_destroy(g_nativePointer);

    	g_nativePointer = nullptr;
    }

    if (g_nativeSeat)
    {
    	wl_seat_destroy(g_nativeSeat);

    	g_nativeSeat = nullptr;
    }

    g_nativeCursor = nullptr;

    if (g_nativeCursorTheme)
    {
    	wl_cursor_theme_destroy(g_nativeCursorTheme);

    	g_nativeCursorTheme = nullptr;
    }

    if (g_nativeCursorSurface)
    {
    	wl_surface_destroy(g_nativeCursorSurface);

    	g_nativeCursorSurface = nullptr;
    }

    g_nativeCursorBuffer = nullptr;

    if (g_nativeShell)
    {
    	wl_shell_destroy(g_nativeShell);

    	g_nativeShell = nullptr;
    }

    if (g_nativeCompositor)
    {
    	wl_compositor_destroy(g_nativeCompositor);

    	g_nativeCompositor = nullptr;
    }

    if (g_nativeShm)
    {
    	wl_shm_destroy(g_nativeShm);

    	g_nativeShm = nullptr;
    }

    if (g_nativeRegistry)
    {
    	wl_registry_destroy(g_nativeRegistry);

    	g_nativeRegistry = nullptr;
    }

    if (g_nativeFd >= 0)
    {
    	close(g_nativeFd);

    	g_nativeFd = -1;
    }

    if (g_nativeDisplay)
    {
    	wl_display_disconnect(g_nativeDisplay);

        g_nativeDisplay = nullptr;
    }
}

//

void VKTS_APIENTRY _visualGamepadSetButton(const int32_t gamepadIndex, const int32_t buttonIndex, const VkBool32 pressed)
{
	for (const auto currentPair : g_allWindows)
	{
		currentPair.second->nativeWindow->getGamepadInput(gamepadIndex).setButton(buttonIndex, pressed);
	}
}

void VKTS_APIENTRY _visualGamepadSetAxis(const int32_t gamepadIndex, const int32_t axisIndex, const float value)
{
    for (const auto currentPair : g_allWindows)
    {
    	currentPair.second->nativeWindow->getGamepadInput(gamepadIndex).setAxis(axisIndex, value);
    }
}

}
