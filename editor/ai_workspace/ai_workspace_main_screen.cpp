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

#include "editor/ai_workspace/ai_workspace_command_router.h"
#include "editor/ai_workspace/ai_workspace_context_service.h"
#include "editor/ai_workspace/ai_workspace_agent_bridge.h"
#include "editor/ai_workspace/agents/codex/ai_workspace_codex_adapter.h"

#include "core/io/json.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "editor/editor_node.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/center_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
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

Label *add_context_state(VBoxContainer *p_parent, const String &p_title, const String &p_empty_state) {
	Label *title = memnew(Label);
	title->set_text(p_title);
	title->set_theme_type_variation("HeaderSmall");
	p_parent->add_child(title);

	Label *empty_state = memnew(Label);
	empty_state->set_text(p_empty_state);
	empty_state->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	p_parent->add_child(empty_state);
	return empty_state;
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

void AIWorkspaceMainScreen::_toggle_context_inspection() {
	const bool inspection_visible = !context_json_view->is_visible();
	context_json_view->set_visible(inspection_visible);
	context_inspection_toggle->set_text(inspection_visible ? TTRC("Hide Context Data") : TTRC("Inspect Context"));
	if (inspection_visible) {
		context_json_view->set_text(context_service->get_context_json());
	}
}

void AIWorkspaceMainScreen::_toggle_command_test_panel() {
	const bool panel_visible = !command_test_panel->is_visible();
	command_test_panel->set_visible(panel_visible);
	command_test_toggle->set_text(panel_visible ? TTRC("Hide Command Router Test") : TTRC("Show Command Router Test"));
}

void AIWorkspaceMainScreen::_execute_test_command() {
	if (!command_router) {
		return;
	}
	const Dictionary response = command_router->execute_json(command_json_input->get_text());
	command_result_view->set_text(JSON::stringify(response, "\t"));
	_refresh_context();
}

void AIWorkspaceMainScreen::_toggle_agent_bridge() {
	if (agent_bridge->is_enabled()) {
		agent_bridge->stop();
	} else {
		agent_bridge->start();
	}
	_refresh_agent_bridge();
}

void AIWorkspaceMainScreen::_approve_agent_request() {
	if (codex_adapter->has_pending_command()) {
		_approve_codex_request();
		return;
	}
	agent_bridge->approve_pending_request();
	_refresh_agent_bridge();
	_refresh_context();
}

void AIWorkspaceMainScreen::_reject_agent_request() {
	if (codex_adapter->has_pending_command()) {
		_reject_codex_request();
		return;
	}
	agent_bridge->reject_pending_request();
	_refresh_agent_bridge();
}

void AIWorkspaceMainScreen::_toggle_codex_connection() {
	if (codex_adapter->get_state() == AIWorkspaceCodexAdapter::STATE_CONNECTED || codex_adapter->get_state() == AIWorkspaceCodexAdapter::STATE_CONNECTING) {
		codex_adapter->disconnect();
	} else {
		codex_adapter->connect_codex();
	}
	_refresh_codex();
}

void AIWorkspaceMainScreen::_send_codex_instruction() {
	const Error error = codex_adapter->send_instruction(instruction_input->get_text());
	if (error != OK) {
		_refresh_codex();
		return;
	}
	instruction_input->clear();
	_refresh_codex();
}

void AIWorkspaceMainScreen::_approve_codex_request() {
	const Dictionary result = codex_adapter->approve_pending_command(command_router);
	command_result_view->set_text(JSON::stringify(result, "\t"));
	_refresh_context();
	_refresh_codex();
}

void AIWorkspaceMainScreen::_reject_codex_request() {
	codex_adapter->reject_pending_command();
	_refresh_codex();
}

void AIWorkspaceMainScreen::_refresh_codex() {
	if (!codex_adapter) {
		return;
	}
	connection_value->set_text(codex_adapter->get_state_text());
	codex_connect_button->set_text(codex_adapter->get_state() == AIWorkspaceCodexAdapter::STATE_CONNECTED ? TTRC("Disconnect Codex") : TTRC("Connect Codex"));
	send_button->set_disabled(codex_adapter->get_state() != AIWorkspaceCodexAdapter::STATE_CONNECTED);
	if (!codex_adapter->get_response_text().is_empty()) {
		codex_response_view->set_text(codex_adapter->get_response_text());
	}
	if (codex_adapter->has_pending_command()) {
		bridge_pending->set_text(vformat(TTRC("Codex proposed command: %s"), codex_adapter->get_pending_command_summary()));
		bridge_approve->set_disabled(false);
		bridge_reject->set_disabled(false);
	} else if (!agent_bridge->has_pending_request()) {
		bridge_pending->set_text(TTRC("No pending command"));
		bridge_approve->set_disabled(true);
		bridge_reject->set_disabled(true);
	}
}

void AIWorkspaceMainScreen::_refresh_agent_bridge() {
	if (!agent_bridge) {
		return;
	}
	if (!agent_bridge->is_enabled()) {
		bridge_status->set_text(agent_bridge->get_last_error().is_empty() ? TTRC("Disabled") : agent_bridge->get_last_error());
		bridge_toggle->set_text(TTRC("Enable Local Bridge"));
		bridge_secret->clear();
	} else if (!agent_bridge->get_last_error().is_empty()) {
		bridge_status->set_text(agent_bridge->get_last_error());
	} else {
		bridge_status->set_text(agent_bridge->has_connection() ? vformat(TTRC("Connected on 127.0.0.1:%d"), agent_bridge->get_port()) : vformat(TTRC("Listening on 127.0.0.1:%d"), agent_bridge->get_port()));
		bridge_toggle->set_text(TTRC("Disable Local Bridge"));
		bridge_secret->set_text(agent_bridge->get_session_secret());
	}
	const bool pending = agent_bridge->has_pending_request();
	bridge_pending->set_text(pending ? vformat(TTRC("Pending command: %s"), agent_bridge->get_pending_summary()) : TTRC("No pending command"));
	bridge_approve->set_disabled(!pending);
	bridge_reject->set_disabled(!pending);
}

void AIWorkspaceMainScreen::_refresh_context() {
	if (!context_service) {
		return;
	}
	context_service->refresh();

	const Dictionary snapshot = context_service->get_context();
	const Dictionary project = snapshot["project"];
	const Dictionary scene = snapshot["scene"];
	const Array selection = snapshot["selection"];

	const String project_name = project.get("name", TTRC("Unnamed project"));
	project_value->set_text(project_name.is_empty() ? TTRC("Unnamed project") : project_name);
	project_value->set_tooltip_text(String(project.get("path", String())));

	const String scene_path = scene.get("path", String());
	scene_value->set_text(scene_path.is_empty() ? TTRC("No scene open") : scene_path);

	if (selection.is_empty()) {
		selection_value->set_text(TTRC("No node selected"));
	} else {
		const Dictionary first_node = selection[0];
		const String selection_summary = vformat(TTRC("%s (%s)"), String(first_node.get("name", String())), String(first_node.get("type", String())));
		selection_value->set_text(selection.size() == 1 ? selection_summary : vformat(TTRC("%s + %d more nodes"), selection_summary, selection.size() - 1));
	}

	resources_value->set_text(vformat(TTRC("%d resource(s) linked to selection"), int(snapshot.get("resource_count", 0))));
	if (context_json_view->is_visible()) {
		context_json_view->set_text(context_service->get_context_json());
	}
}

void AIWorkspaceMainScreen::refresh_context() {
	_refresh_context();
}

void AIWorkspaceMainScreen::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS: {
			agent_bridge->poll();
			codex_adapter->poll();
			_refresh_agent_bridge();
			_refresh_codex();
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			empty_state_icon->set_texture(get_editor_theme_icon(SNAME("History")));
			settings_button->set_button_icon(get_editor_theme_icon(SNAME("Tools")));
			_update_context_toggle_button();
		} break;
	}
}

AIWorkspaceMainScreen::AIWorkspaceMainScreen() {
	context_service = memnew(AIWorkspaceContextService);
	command_router = memnew(AIWorkspaceCommandRouter);
	agent_bridge = memnew(AIWorkspaceAgentBridge(context_service, command_router));
	codex_adapter = memnew(AIWorkspaceCodexAdapter(context_service));
	set_process(true);
	if (EditorNode::get_singleton() && EditorNode::get_singleton()->get_editor_selection()) {
		EditorNode::get_singleton()->get_editor_selection()->connect("selection_changed", callable_mp(this, &AIWorkspaceMainScreen::_refresh_context));
	}

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

	agent_selector = memnew(OptionButton);
	agent_selector->set_name("AgentSelector");
	agent_selector->add_item(TTRC("Codex"));
	agent_selector->select(0);
	agent_selector->set_tooltip_text(TTRC("Codex is the only integrated agent in this stage."));
	header->add_child(agent_selector);

	header->add_child(memnew(VSeparator));

	Label *connection_label = memnew(Label);
	connection_label->set_text(TTRC("Status:"));
	header->add_child(connection_label);

	connection_value = memnew(Label);
	connection_value->set_text(TTRC("Disconnected"));
	connection_value->set_theme_type_variation("HeaderSmall");
	header->add_child(connection_value);

	codex_connect_button = memnew(Button);
	codex_connect_button->set_text(TTRC("Connect Codex"));
	codex_connect_button->connect("pressed", callable_mp(this, &AIWorkspaceMainScreen::_toggle_codex_connection));
	header->add_child(codex_connect_button);

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
	send_button->connect("pressed", callable_mp(this, &AIWorkspaceMainScreen::_send_codex_instruction));

	codex_response_view = memnew(TextEdit);
	codex_response_view->set_name("CodexResponseView");
	codex_response_view->set_custom_minimum_size(Size2(0, 120) * EDSCALE);
	codex_response_view->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	codex_response_view->set_editable(false);
	codex_response_view->set_placeholder(TTRC("Codex response"));
	conversation->add_child(codex_response_view);

	command_test_toggle = memnew(Button);
	command_test_toggle->set_name("CommandRouterTestToggle");
	command_test_toggle->set_text(TTRC("Show Command Router Test"));
	command_test_toggle->set_tooltip_text(TTRC("Open the temporary development tool for structured scene commands."));
	command_test_toggle->connect("pressed", callable_mp(this, &AIWorkspaceMainScreen::_toggle_command_test_panel));
	conversation->add_child(command_test_toggle);

	command_test_panel = memnew(PanelContainer);
	command_test_panel->set_name("CommandRouterTestPanel");
	command_test_panel->hide();
	conversation->add_child(command_test_panel);

	PanelContainer *bridge_panel = memnew(PanelContainer);
	bridge_panel->set_name("AgentBridgePanel");
	conversation->add_child(bridge_panel);
	MarginContainer *bridge_margin = memnew(MarginContainer);
	set_margin(bridge_margin, 8 * EDSCALE);
	bridge_panel->add_child(bridge_margin);
	VBoxContainer *bridge_box = memnew(VBoxContainer);
	bridge_box->add_theme_constant_override(SNAME("separation"), 6 * EDSCALE);
	bridge_margin->add_child(bridge_box);
	Label *bridge_title = memnew(Label);
	bridge_title->set_text(TTRC("Local Agent Bridge (Development)"));
	bridge_title->set_theme_type_variation("HeaderSmall");
	bridge_box->add_child(bridge_title);
	Label *bridge_description = memnew(Label);
	bridge_description->set_text(TTRC("Loopback JSON transport. Connections never authorize edits automatically."));
	bridge_description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	bridge_box->add_child(bridge_description);
	HBoxContainer *bridge_controls = memnew(HBoxContainer);
	bridge_box->add_child(bridge_controls);
	bridge_toggle = memnew(Button);
	bridge_toggle->connect("pressed", callable_mp(this, &AIWorkspaceMainScreen::_toggle_agent_bridge));
	bridge_controls->add_child(bridge_toggle);
	bridge_status = memnew(Label);
	bridge_status->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	bridge_controls->add_child(bridge_status);
	bridge_secret = memnew(LineEdit);
	bridge_secret->set_name("AgentBridgeSessionSecret");
	bridge_secret->set_editable(false);
	bridge_secret->set_placeholder(TTRC("Session secret appears when enabled"));
	bridge_secret->set_custom_minimum_size(Size2(260, 0) * EDSCALE);
	bridge_box->add_child(bridge_secret);
	bridge_pending = memnew(Label);
	bridge_pending->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	bridge_box->add_child(bridge_pending);
	HBoxContainer *approval_controls = memnew(HBoxContainer);
	bridge_box->add_child(approval_controls);
	bridge_approve = memnew(Button);
	bridge_approve->set_text(TTRC("Approve Command"));
	bridge_approve->connect("pressed", callable_mp(this, &AIWorkspaceMainScreen::_approve_agent_request));
	approval_controls->add_child(bridge_approve);
	bridge_reject = memnew(Button);
	bridge_reject->set_text(TTRC("Reject Command"));
	bridge_reject->connect("pressed", callable_mp(this, &AIWorkspaceMainScreen::_reject_agent_request));
	approval_controls->add_child(bridge_reject);

	MarginContainer *command_test_margin = memnew(MarginContainer);
	set_margin(command_test_margin, 8 * EDSCALE);
	command_test_panel->add_child(command_test_margin);

	VBoxContainer *command_test = memnew(VBoxContainer);
	command_test->add_theme_constant_override(SNAME("separation"), 6 * EDSCALE);
	command_test_margin->add_child(command_test);

	Label *command_test_title = memnew(Label);
	command_test_title->set_text(TTRC("Command Router Test (Development Tool)"));
	command_test_title->set_theme_type_variation("HeaderSmall");
	command_test->add_child(command_test_title);

	command_json_input = memnew(TextEdit);
	command_json_input->set_name("CommandJsonInput");
	command_json_input->set_custom_minimum_size(Size2(0, 130) * EDSCALE);
	command_json_input->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	command_json_input->set_text("{\n\t\"command\": \"node.create\",\n\t\"args\": {\n\t\t\"parent_path\": \".\",\n\t\t\"type\": \"Node2D\",\n\t\t\"name\": \"Enemy\"\n\t}\n}");
	command_test->add_child(command_json_input);

	Button *execute_command_button = memnew(Button);
	execute_command_button->set_name("ExecuteCommandButton");
	execute_command_button->set_text(TTRC("Validate and Execute"));
	execute_command_button->set_tooltip_text(TTRC("Validate and execute this command through the native editor Undo/Redo history."));
	execute_command_button->connect("pressed", callable_mp(this, &AIWorkspaceMainScreen::_execute_test_command));
	command_test->add_child(execute_command_button);

	command_result_view = memnew(TextEdit);
	command_result_view->set_name("CommandResultView");
	command_result_view->set_custom_minimum_size(Size2(0, 110) * EDSCALE);
	command_result_view->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	command_result_view->set_editable(false);
	command_result_view->set_placeholder(TTRC("Structured command result"));
	command_test->add_child(command_result_view);

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
	ScrollContainer *context_scroll = memnew(ScrollContainer);
	context_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	context_scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	context_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	context_margin->add_child(context_scroll);
	context_scroll->add_child(context);

	Label *context_title = memnew(Label);
	context_title->set_text(TTRC("Project Context"));
	context_title->set_theme_type_variation("HeaderMedium");
	context->add_child(context_title);

	Label *context_description = memnew(Label);
	context_description->set_text(TTRC("Project context will appear here when an agent is connected."));
	context_description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	context->add_child(context_description);

	context->add_child(memnew(HSeparator));
	project_value = add_context_state(context, TTRC("Project"), TTRC("Unnamed project"));
	context->add_child(memnew(HSeparator));
	scene_value = add_context_state(context, TTRC("Scene"), TTRC("No scene open"));
	context->add_child(memnew(HSeparator));
	selection_value = add_context_state(context, TTRC("Selection"), TTRC("No node selected"));
	context->add_child(memnew(HSeparator));
	resources_value = add_context_state(context, TTRC("Resources"), TTRC("No resources linked to selection"));
	context->add_child(memnew(HSeparator));
	add_context_state(context, TTRC("Agent Operations"), TTRC("No agent operations"));

	context_inspection_toggle = memnew(Button);
	context_inspection_toggle->set_text(TTRC("Inspect Context"));
	context_inspection_toggle->set_tooltip_text(TTRC("Show the read-only structured context collected from the editor."));
	context_inspection_toggle->connect("pressed", callable_mp(this, &AIWorkspaceMainScreen::_toggle_context_inspection));
	context->add_child(context_inspection_toggle);

	context_json_view = memnew(TextEdit);
	context_json_view->set_name("ContextDataView");
	context_json_view->set_custom_minimum_size(Size2(0, 180) * EDSCALE);
	context_json_view->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	context_json_view->set_editable(false);
	context_json_view->set_v_grow_direction(Control::GROW_DIRECTION_BEGIN);
	context_json_view->hide();
	context->add_child(context_json_view);

	_update_context_toggle_button();
	_refresh_context();
	_refresh_agent_bridge();
	_refresh_codex();

	String command_line_secret;
	bool command_line_bridge = false;
	for (const String &argument : OS::get_singleton()->get_cmdline_args()) {
		if (argument == "--ai-workspace-bridge") {
			command_line_bridge = true;
		} else if (argument.begins_with("--ai-workspace-bridge-secret=")) {
			command_line_secret = argument.trim_prefix("--ai-workspace-bridge-secret=");
		}
	}
	if (command_line_bridge) {
		agent_bridge->start(command_line_secret);
		_refresh_agent_bridge();
	}
}

AIWorkspaceMainScreen::~AIWorkspaceMainScreen() {
	set_process(false);
	memdelete(agent_bridge);
	memdelete(codex_adapter);
	memdelete(command_router);
	memdelete(context_service);
}
