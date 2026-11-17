/**************************************************************************/
/*  ai_workspace_main_screen.h                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "scene/gui/margin_container.h"

class Button;
class AIWorkspaceContextService;
class Label;
class PanelContainer;
class TextEdit;
class TextureRect;
class VBoxContainer;

class AIWorkspaceMainScreen : public MarginContainer {
	GDCLASS(AIWorkspaceMainScreen, MarginContainer);

	Label *agent_value = nullptr;
	Label *connection_value = nullptr;
	TextureRect *empty_state_icon = nullptr;
	VBoxContainer *message_history = nullptr;
	TextEdit *instruction_input = nullptr;
	Button *send_button = nullptr;
	Button *settings_button = nullptr;
	Button *context_toggle_button = nullptr;
	PanelContainer *context_panel = nullptr;
	Button *context_inspection_toggle = nullptr;
	TextEdit *context_json_view = nullptr;
	Label *project_value = nullptr;
	Label *scene_value = nullptr;
	Label *selection_value = nullptr;
	Label *resources_value = nullptr;
	AIWorkspaceContextService *context_service = nullptr;

	void _toggle_context_panel();
	void _update_context_toggle_button();
	void _toggle_context_inspection();
	void _refresh_context();

protected:
	void _notification(int p_what);

public:
	AIWorkspaceMainScreen();
	~AIWorkspaceMainScreen();
	void refresh_context();
};
