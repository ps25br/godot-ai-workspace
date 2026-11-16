/**************************************************************************/
/*  ai_workspace_main_screen.cpp                                          */
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

#include "ai_workspace_main_screen.h"

#include "core/object/callable_mp.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/center_container.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/split_container.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/texture_rect.h"

namespace {

void set_margin(MarginContainer *p_container, int p_margin) {
	p_container->add_theme_constant_override(SNAME("margin_left"), p_margin);
	p_container->add_theme_constant_override(SNAME("margin_top"), p_margin);
	p_container->add_theme_constant_override(SNAME("margin_right"), p_margin);
	p_container->add_theme_constant_override(SNAME("margin_bottom"), p_margin);
}

void add_context_state(VBoxContainer *p_parent, const String &p_title, const String &p_empty_state) {
	Label *title = memnew(Label);
	title->set_text(p_title);
	title->set_theme_type_variation("HeaderSmall");
	p_parent->add_child(title);

	Label *empty_state = memnew(Label);
	empty_state->set_text(p_empty_state);
	empty_state->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	p_parent->add_child(empty_state);
}

} // namespace

void AIWorkspaceMainScreen::_toggle_context_panel() {
	context_panel->set_visible(!context_panel->is_visible());
	_update_context_toggle_button();
}

void AIWorkspaceMainScreen::_update_context_toggle_button() {
	const bool context_visible = context_panel->is_visible();
	context_toggle_button->set_text(context_visible ? TTRC("Hide Context") : TTRC("Show Context"));
	context_toggle_button->set_tooltip_text(context_visible ? TTRC("Hide the project context panel.") : TTRC("Show the project context panel."));

	if (is_inside_tree()) {
		context_toggle_button->set_button_icon(get_editor_theme_icon(context_visible ? SNAME("Collapse") : SNAME("ExpandTree")));
	}
}

void AIWorkspaceMainScreen::_notification(int p_what) {
	if (p_what == NOTIFICATION_THEME_CHANGED) {
		empty_state_icon->set_texture(get_editor_theme_icon(SNAME("History")));
		settings_button->set_button_icon(get_editor_theme_icon(SNAME("Tools")));
		_update_context_toggle_button();
	}
}

AIWorkspaceMainScreen::AIWorkspaceMainScreen() {
	set_name("AIWorkspace");
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_EXPAND_FILL);
	set_margin(this, 8 * EDSCALE);

	VBoxContainer *workspace = memnew(VBoxContainer);
	workspace->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	workspace->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	workspace->add_theme_constant_override(SNAME("separation"), 8 * EDSCALE);
	add_child(workspace);

	PanelContainer *header_panel = memnew(PanelContainer);
	workspace->add_child(header_panel);

	MarginContainer *header_margin = memnew(MarginContainer);
	set_margin(header_margin, 8 * EDSCALE);
	header_panel->add_child(header_margin);

	HBoxContainer *header = memnew(HBoxContainer);
	header->add_theme_constant_override(SNAME("separation"), 8 * EDSCALE);
	header_margin->add_child(header);

	Label *title = memnew(Label);
	title->set_text(TTRC("AI Workspace"));
	title->set_theme_type_variation("HeaderLarge");
	header->add_child(title);

	Control *header_spacer = memnew(Control);
	header_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header->add_child(header_spacer);

	Label *agent_label = memnew(Label);
	agent_label->set_text(TTRC("Agent:"));
	header->add_child(agent_label);

	agent_value = memnew(Label);
	agent_value->set_text(TTRC("None"));
	agent_value->set_theme_type_variation("HeaderSmall");
	header->add_child(agent_value);

	header->add_child(memnew(VSeparator));

	Label *connection_label = memnew(Label);
	connection_label->set_text(TTRC("Status:"));
	header->add_child(connection_label);

	connection_value = memnew(Label);
	connection_value->set_text(TTRC("Disconnected"));
	connection_value->set_theme_type_variation("HeaderSmall");
	header->add_child(connection_value);

	settings_button = memnew(Button);
	settings_button->set_text(TTRC("Settings"));
	settings_button->set_tooltip_text(TTRC("Agent settings will be available after an agent is connected."));
	settings_button->set_disabled(true);
	header->add_child(settings_button);

	context_toggle_button = memnew(Button);
	context_toggle_button->connect("pressed", callable_mp(this, &AIWorkspaceMainScreen::_toggle_context_panel));
	header->add_child(context_toggle_button);

	HSplitContainer *content_split = memnew(HSplitContainer);
	content_split->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	workspace->add_child(content_split);

	PanelContainer *conversation_panel = memnew(PanelContainer);
	conversation_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	conversation_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	content_split->add_child(conversation_panel);

	MarginContainer *conversation_margin = memnew(MarginContainer);
	set_margin(conversation_margin, 12 * EDSCALE);
	conversation_panel->add_child(conversation_margin);

	VBoxContainer *conversation = memnew(VBoxContainer);
	conversation->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	conversation->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	conversation->add_theme_constant_override(SNAME("separation"), 8 * EDSCALE);
	conversation_margin->add_child(conversation);

	Label *conversation_title = memnew(Label);
	conversation_title->set_text(TTRC("Conversation"));
	conversation_title->set_theme_type_variation("HeaderMedium");
	conversation->add_child(conversation_title);

	ScrollContainer *history_scroll = memnew(ScrollContainer);
	history_scroll->set_name("MessageHistoryScroll");
	history_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	history_scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	history_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	conversation->add_child(history_scroll);

	message_history = memnew(VBoxContainer);
	message_history->set_name("MessageHistory");
	message_history->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	message_history->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	history_scroll->add_child(message_history);

	CenterContainer *empty_state_center = memnew(CenterContainer);
	empty_state_center->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	empty_state_center->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	message_history->add_child(empty_state_center);

	VBoxContainer *empty_state = memnew(VBoxContainer);
	empty_state->set_custom_minimum_size(Size2(320, 0) * EDSCALE);
	empty_state->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	empty_state_center->add_child(empty_state);

	empty_state_icon = memnew(TextureRect);
	empty_state_icon->set_custom_minimum_size(Size2(32, 32) * EDSCALE);
	empty_state_icon->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	empty_state_icon->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
	empty_state->add_child(empty_state_icon);

	Label *empty_state_message = memnew(Label);
	empty_state_message->set_text(TTRC("Connect an AI coding agent to start building."));
	empty_state_message->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	empty_state_message->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	empty_state->add_child(empty_state_message);

	HBoxContainer *composer = memnew(HBoxContainer);
	composer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	composer->add_theme_constant_override(SNAME("separation"), 8 * EDSCALE);
	conversation->add_child(composer);

	instruction_input = memnew(TextEdit);
	instruction_input->set_name("InstructionInput");
	instruction_input->set_placeholder(TTRC("Describe what you want to build or change..."));
	instruction_input->set_custom_minimum_size(Size2(0, 84) * EDSCALE);
	instruction_input->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	instruction_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	composer->add_child(instruction_input);

	send_button = memnew(Button);
	send_button->set_name("SendButton");
	send_button->set_text(TTRC("Send"));
	send_button->set_disabled(true);
	send_button->set_v_size_flags(Control::SIZE_SHRINK_END);
	composer->add_child(send_button);

	context_panel = memnew(PanelContainer);
	context_panel->set_name("ContextPanel");
	context_panel->set_custom_minimum_size(Size2(260, 0) * EDSCALE);
	context_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	content_split->add_child(context_panel);

	MarginContainer *context_margin = memnew(MarginContainer);
	set_margin(context_margin, 12 * EDSCALE);
	context_panel->add_child(context_margin);

	VBoxContainer *context = memnew(VBoxContainer);
	context->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	context->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	context->add_theme_constant_override(SNAME("separation"), 8 * EDSCALE);
	context_margin->add_child(context);

	Label *context_title = memnew(Label);
	context_title->set_text(TTRC("Project Context"));
	context_title->set_theme_type_variation("HeaderMedium");
	context->add_child(context_title);

	Label *context_description = memnew(Label);
	context_description->set_text(TTRC("Project context will appear here when an agent is connected."));
	context_description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	context->add_child(context_description);

	context->add_child(memnew(HSeparator));
	add_context_state(context, TTRC("Selected Scene"), TTRC("No scene selected"));
	context->add_child(memnew(HSeparator));
	add_context_state(context, TTRC("Selected Node"), TTRC("No node selected"));
	context->add_child(memnew(HSeparator));
	add_context_state(context, TTRC("Related Resources"), TTRC("No related resources"));
	context->add_child(memnew(HSeparator));
	add_context_state(context, TTRC("Agent Operations"), TTRC("No agent operations"));

	_update_context_toggle_button();
}
