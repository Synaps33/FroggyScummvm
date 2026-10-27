/* Copyright (C) 2023 Giovanni Cascione <ing.cascione@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#define FORBIDDEN_SYMBOL_EXCEPTION_strcpy
#define FORBIDDEN_SYMBOL_EXCEPTION_strcat

#include "backends/platform/libretro/include/libretro-defs.h"
#include "backends/platform/libretro/include/libretro-os.h"
#include "backends/platform/libretro/include/libretro-mapper.h"
#include "backends/platform/libretro/include/libretro-core.h"
#include "backends/platform/libretro/include/libretro-graphics-surface.h"
#ifdef USE_OPENGL
#include "backends/platform/libretro/include/libretro-graphics-opengl.h"
#endif

void OSystem_libretro::updateMouseXY(float deltaAcc, float *cumulativeXYAcc, int doing_x) {
	int *mouseXY;
	int16 screen_wh;
	int *relMouseXY;
	int cumulativeXYAcc_int;

	if (! deltaAcc)
		return;

	if (_cursorStatus & CURSOR_STATUS_DOING_SLOWER)
		deltaAcc /= retro_setting_get_mouse_fine_control_speed_reduction();

	if (doing_x) {
		_cursorStatus |= CURSOR_STATUS_DOING_X;
		mouseXY = &_mouseX;
		screen_wh = getScreenWidth();
		relMouseXY = &_relMouseX;
	} else {
		_cursorStatus |= CURSOR_STATUS_DOING_Y;
		mouseXY = &_mouseY;
		screen_wh = getScreenHeight();
		relMouseXY = &_relMouseY;
	}
	*cumulativeXYAcc += deltaAcc;
	cumulativeXYAcc_int = (int) * cumulativeXYAcc;
	if (cumulativeXYAcc_int != 0) {
		// Set mouse position
		*mouseXY += cumulativeXYAcc_int;
		*mouseXY = (*mouseXY < 0) ? 0 : *mouseXY;
		*mouseXY = (*mouseXY >= screen_wh) ? (screen_wh ? screen_wh - 1 : 0) : *mouseXY;
		// Update accumulator
		*cumulativeXYAcc -= (float)cumulativeXYAcc_int;
	}
	*relMouseXY = (int)deltaAcc;
}

void OSystem_libretro::getMouseXYFromAnalog(bool is_x, int16 coor) {

	int16 sign = (coor > 0) - (coor < 0);
	uint16 abs_coor = abs(coor);
	float *mouseAcc;

	if (abs_coor < retro_setting_get_analog_deadzone()) return;

	_cursorStatus |= CURSOR_STATUS_DOING_JOYSTICK;

	if (is_x) {
		mouseAcc = &_mouseXAcc;
	} else {
		mouseAcc = &_mouseYAcc;
	}

	*mouseAcc = ((*mouseAcc > 0) - (*mouseAcc < 0)) == sign ? *mouseAcc : 0;
	float analog_amplitude = (float)(abs_coor - retro_setting_get_analog_deadzone()) / (float)(ANALOG_RANGE - retro_setting_get_analog_deadzone());

	if (retro_setting_get_analog_response_is_quadratic())
		analog_amplitude *= analog_amplitude;

	updateMouseXY(sign * analog_amplitude * _adjusted_cursor_speed, mouseAcc, is_x);
}

void OSystem_libretro::getMouseXYFromButton(bool is_x, int16 sign) {
	float *dpadVel;
	float *dpadAcc;

	if (is_x) {
		dpadVel = &_dpadXVel;
		dpadAcc = &_dpadXAcc;
	} else {
		dpadVel = &_dpadYVel;
		dpadAcc = &_dpadYAcc;
	}

	if ((*dpadAcc && ((*dpadAcc > 0) - (*dpadAcc < 0)) != sign) || ! sign) {
		*dpadVel = 0.0f;
		*dpadAcc = 0.0f;
	}

	if (! sign)
		return;

	_cursorStatus |= CURSOR_STATUS_DOING_JOYSTICK;

	*dpadVel = MIN(*dpadVel + _inverse_acceleration_time, 1.0f);

	updateMouseXY(sign * *dpadVel * _adjusted_cursor_speed, dpadAcc, is_x);
}

void OSystem_libretro::processInputs(void) {
	if (!retro_input_cb || !_graphicsManager)
		return;

	// 1. Direct Joypad Polling for SF2000 / Handheld consoles
	bool up     = retro_input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
	bool down   = retro_input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
	bool left   = retro_input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
	bool right  = retro_input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
	bool btn_a  = retro_input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
	bool btn_b  = retro_input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
	bool btn_x  = retro_input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X);
	bool btn_y  = retro_input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y);
	bool start  = retro_input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START);
	bool select = retro_input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT);
	bool btn_l  = retro_input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L);
	bool btn_r  = retro_input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R);

	int screen_w = getScreenWidth();
	int screen_h = getScreenHeight();
	if (screen_w <= 0) screen_w = 320;
	if (screen_h <= 0) screen_h = 240;

	static bool s_pos_initialized = false;
	if (!s_pos_initialized) {
		_mouseX = screen_w / 2;
		_mouseY = screen_h / 2;
		s_pos_initialized = true;
		setMousePosition(_mouseX, _mouseY);
	}

	static int s_dpad_held_frames = 0;
	bool moving = up || down || left || right;
	if (moving) {
		s_dpad_held_frames++;
	} else {
		s_dpad_held_frames = 0;
	}

	int speed = 2;
	if (s_dpad_held_frames > 6)  speed = 4;
	if (s_dpad_held_frames > 16) speed = 7;

	int old_x = _mouseX;
	int old_y = _mouseY;

	if (up)    _mouseY -= speed;
	if (down)  _mouseY += speed;
	if (left)  _mouseX -= speed;
	if (right) _mouseX += speed;

	if (_mouseX < 0) _mouseX = 0;
	if (_mouseX >= screen_w) _mouseX = screen_w - 1;
	if (_mouseY < 0) _mouseY = 0;
	if (_mouseY >= screen_h) _mouseY = screen_h - 1;

	if (_mouseX != old_x || _mouseY != old_y) {
		Common::Event ev;
		ev.type = Common::EVENT_MOUSEMOVE;
		ev.mouse.x = _mouseX;
		ev.mouse.y = _mouseY;
		ev.relMouse.x = _mouseX - old_x;
		ev.relMouse.y = _mouseY - old_y;
		_events.push_back(ev);
		setMousePosition(_mouseX, _mouseY);
	}

	// Left Click: A or L button
	static bool s_lclick_held = false;
	bool lclick = btn_a || btn_l;
	if (lclick && !s_lclick_held) {
		s_lclick_held = true;
		Common::Event ev;
		ev.type = Common::EVENT_LBUTTONDOWN;
		ev.mouse.x = _mouseX;
		ev.mouse.y = _mouseY;
		_events.push_back(ev);
	} else if (!lclick && s_lclick_held) {
		s_lclick_held = false;
		Common::Event ev;
		ev.type = Common::EVENT_LBUTTONUP;
		ev.mouse.x = _mouseX;
		ev.mouse.y = _mouseY;
		_events.push_back(ev);
	}

	// Right Click: B or R button
	static bool s_rclick_held = false;
	bool rclick = btn_b || btn_r;
	if (rclick && !s_rclick_held) {
		s_rclick_held = true;
		Common::Event ev;
		ev.type = Common::EVENT_RBUTTONDOWN;
		ev.mouse.x = _mouseX;
		ev.mouse.y = _mouseY;
		_events.push_back(ev);
	} else if (!rclick && s_rclick_held) {
		s_rclick_held = false;
		Common::Event ev;
		ev.type = Common::EVENT_RBUTTONUP;
		ev.mouse.x = _mouseX;
		ev.mouse.y = _mouseY;
		_events.push_back(ev);
	}

	// Start: ScummVM Menu / Pause
	static bool s_start_held = false;
	if (start && !s_start_held) {
		s_start_held = true;
		Common::Event ev;
		ev.type = Common::EVENT_MAINMENU;
		_events.push_back(ev);
	} else if (!start && s_start_held) {
		s_start_held = false;
	}

	// Space (dialog skip): Select or X button
	static bool s_space_held = false;
	bool space_btn = select || btn_x;
	if (space_btn && !s_space_held) {
		s_space_held = true;
		processKeyEvent(true, Common::KEYCODE_SPACE, ' ', 0);
	} else if (!space_btn && s_space_held) {
		s_space_held = false;
		processKeyEvent(false, Common::KEYCODE_SPACE, ' ', 0);
	}

	// Escape (cancel/pause): Y button
	static bool s_esc_held = false;
	if (btn_y && !s_esc_held) {
		s_esc_held = true;
		processKeyEvent(true, Common::KEYCODE_ESCAPE, 27, 0);
	} else if (!btn_y && s_esc_held) {
		s_esc_held = false;
		processKeyEvent(false, Common::KEYCODE_ESCAPE, 27, 0);
	}
}

void OSystem_libretro::processKeyEvent(bool down, unsigned keycode, uint32 character, uint16 key_modifiers) {
	int _keyflags = 0;
	_keyflags |= (key_modifiers & RETROKMOD_CTRL) ? Common::KBD_CTRL : 0;
	_keyflags |= (key_modifiers & RETROKMOD_ALT) ? Common::KBD_ALT : 0;
	_keyflags |= (key_modifiers & RETROKMOD_SHIFT) ? Common::KBD_SHIFT : 0;
	_keyflags |= (key_modifiers & RETROKMOD_META) ? Common::KBD_META : 0;
	_keyflags |= (key_modifiers & RETROKMOD_CAPSLOCK) ? Common::KBD_CAPS : 0;
	_keyflags |= (key_modifiers & RETROKMOD_NUMLOCK) ? Common::KBD_NUM : 0;
	_keyflags |= (key_modifiers & RETROKMOD_SCROLLOCK) ? Common::KBD_SCRL : 0;

	Common::Event ev;
	ev.type = down ? Common::EVENT_KEYDOWN : Common::EVENT_KEYUP;
	ev.kbd.keycode = (Common::KeyCode)keycode;
	ev.kbd.flags = _keyflags;
	ev.kbd.ascii = keycode;

	/* If shift was down then send upper case letter to engine */
	if (ev.kbd.ascii >= 97 && ev.kbd.ascii <= 122 && (_keyflags & Common::KBD_SHIFT))
		ev.kbd.ascii = ev.kbd.ascii & ~0x20;

	_events.push_back(ev);
}
