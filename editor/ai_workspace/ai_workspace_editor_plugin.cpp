/**************************************************************************/
/*  ai_workspace_editor_plugin.cpp                                        */
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

#include "ai_workspace_editor_plugin.h"

#include "editor/editor_node.h"
#include "scene/gui/box_container.h"
#include "scene/gui/center_container.h"
#include "scene/gui/label.h"

String AIWorkspaceEditorPlugin::get_plugin_name() const {
	return TTRC("AI Workspace");
}

void AIWorkspaceEditorPlugin::make_visible(bool p_visible) {
	if (ai_workspace) {
		ai_workspace->set_visible(p_visible);
	}
}

AIWorkspaceEditorPlugin::AIWorkspaceEditorPlugin() {
	ai_workspace = memnew(CenterContainer);
	ai_workspace->set_name("AIWorkspace");
	ai_workspace->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	ai_workspace->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	VBoxContainer *content = memnew(VBoxContainer);
	content->set_custom_minimum_size(Size2(480, 0));
	content->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	ai_workspace->add_child(content);

	Label *title = memnew(Label);
	title->set_text(TTRC("AI Workspace"));
	title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	content->add_child(title);

	Label *description = memnew(Label);
	description->set_text(TTRC("Build and edit Godot projects with your preferred AI coding agent."));
	description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	description->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	content->add_child(description);

	Label *status = memnew(Label);
	status->set_text(TTRC("No agent connected"));
	status->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	content->add_child(status);

	EditorNode::get_singleton()->get_editor_main_screen()->get_control()->add_child(ai_workspace);
	ai_workspace->hide();
}
