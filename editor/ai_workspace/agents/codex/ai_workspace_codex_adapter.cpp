/**************************************************************************/
/*  ai_workspace_codex_adapter.cpp                                       */
/**************************************************************************/

#include "ai_workspace_codex_adapter.h"

#include "editor/ai_workspace/ai_workspace_command_router.h"
#include "editor/ai_workspace/ai_workspace_context_service.h"

#include "core/io/json.h"
#include "core/os/os.h"
#include "editor/settings/editor_settings.h"

namespace {
const String COMMAND_MARKER = "AI_WORKSPACE_COMMAND:";
}

AIWorkspaceCodexAdapter::AIWorkspaceCodexAdapter(AIWorkspaceContextService *p_context_service) {
	context_service = p_context_service;
	_load_persistent_state();
}

AIWorkspaceCodexAdapter::~AIWorkspaceCodexAdapter() {
	disconnect();
}

String AIWorkspaceCodexAdapter::get_state_text() const {
	switch (state) {
		case STATE_NOT_INSTALLED: return TTRC("Not installed");
		case STATE_AUTHENTICATION_REQUIRED: return TTRC("Authentication required");
		case STATE_DISCONNECTED: return TTRC("Disconnected");
		case STATE_CONNECTING: return TTRC("Connecting");
		case STATE_CONNECTED: return TTRC("Connected");
		case STATE_ERROR: return last_error.is_empty() ? TTRC("Error") : last_error;
	}
	return TTRC("Disconnected");
}

void AIWorkspaceCodexAdapter::_set_error(const String &p_error, State p_state) {
	last_error = p_error;
	state = p_state;
}

void AIWorkspaceCodexAdapter::_resolve_codex_command() {
	codex_executable = "codex";
	codex_script = String();
	if (OS::get_singleton()->get_name() != "Windows") {
		return;
	}

	const Vector<String> path_entries = OS::get_singleton()->get_environment("PATH").split(";", false);
	String node_executable;
	for (const String &entry : path_entries) {
		const String directory = entry.strip_edges();
		if (!directory.is_empty() && FileAccess::file_exists(directory.path_join("node.exe"))) {
			node_executable = directory.path_join("node.exe");
			break;
		}
	}
	for (const String &entry : path_entries) {
		const String directory = entry.strip_edges();
		if (directory.is_empty()) {
			continue;
		}
		const String native_executable = directory.path_join("codex.exe");
		if (FileAccess::file_exists(native_executable)) {
			codex_executable = native_executable;
			return;
		}
		const String npm_launcher = directory.path_join("codex.cmd");
		const String npm_script = directory.path_join("node_modules").path_join("@openai").path_join("codex").path_join("bin").path_join("codex.js");
		if (FileAccess::file_exists(npm_launcher) && !node_executable.is_empty() && FileAccess::file_exists(npm_script)) {
			codex_executable = node_executable;
			codex_script = npm_script;
			return;
		}
	}
}

void AIWorkspaceCodexAdapter::_prepare_codex_arguments(const List<String> &p_arguments, List<String> &r_arguments) const {
	if (!codex_script.is_empty()) {
		r_arguments.push_back(codex_script);
	}
	for (const String &argument : p_arguments) {
		r_arguments.push_back(argument);
	}
}

void AIWorkspaceCodexAdapter::_load_persistent_state() {
	if (!EditorSettings::get_singleton()) {
		return;
	}
	persisted_thread_id = EditorSettings::get_singleton()->get_project_metadata("ai_workspace_codex", "thread_id", String());
	const String history_json = EditorSettings::get_singleton()->get_project_metadata("ai_workspace_codex", "history", String());
	if (!history_json.is_empty()) {
		const Variant parsed = JSON::parse_string(history_json);
		if (parsed.get_type() == Variant::ARRAY) {
			chat_history = parsed;
			if (chat_history.size() > 64) {
				chat_history.resize(64);
			}
		}
	}
}

void AIWorkspaceCodexAdapter::_save_persistent_state() {
	if (!EditorSettings::get_singleton()) {
		return;
	}
	EditorSettings::get_singleton()->set_project_metadata("ai_workspace_codex", "thread_id", thread_id);
	Array limited_history;
	const int first = MAX(0, chat_history.size() - 64);
	for (int i = first; i < chat_history.size(); i++) {
		Dictionary message = chat_history[i];
		String text = message.get("text", String());
		if (text.length() > 32768) {
			text = text.left(32768);
		}
		message["text"] = text;
		limited_history.push_back(message);
	}
	EditorSettings::get_singleton()->set_project_metadata("ai_workspace_codex", "history", JSON::stringify(limited_history));
}

bool AIWorkspaceCodexAdapter::_is_authentication_error(const String &p_error) const {
	const String error = p_error.to_lower();
	return error.contains("auth") || error.contains("login") || error.contains("credential") || error.contains("token") || error.contains("unauthorized");
}

void AIWorkspaceCodexAdapter::detect() {
	if (state == STATE_CONNECTING || state == STATE_CONNECTED) {
		return;
	}
	_resolve_codex_command();
	List<String> arguments;
	arguments.push_back("--version");
	List<String> resolved_arguments;
	_prepare_codex_arguments(arguments, resolved_arguments);
	String output;
	int exit_code = -1;
	const Error error = OS::get_singleton()->execute(codex_executable, resolved_arguments, &output, &exit_code, false);
	if (error != OK || exit_code != 0) {
		_set_error(TTRC("Codex CLI was not found on PATH"), STATE_NOT_INSTALLED);
		return;
	}
	last_error = String();
	state = STATE_DISCONNECTED;
}

bool AIWorkspaceCodexAdapter::_send(const Dictionary &p_message) {
	if (!stdio.is_valid()) {
		return false;
	}
	stdio->store_line(JSON::stringify(p_message));
	stdio->flush();
	return stdio->get_error() == OK;
}

Error AIWorkspaceCodexAdapter::connect_codex() {
	detect();
	if (state == STATE_NOT_INSTALLED) {
		return ERR_FILE_NOT_FOUND;
	}
	if (state == STATE_CONNECTED || state == STATE_CONNECTING) {
		return OK;
	}

	disconnect();
	List<String> arguments;
	arguments.push_back("app-server");
	arguments.push_back("--listen");
	arguments.push_back("stdio://");
	List<String> resolved_arguments;
	_prepare_codex_arguments(arguments, resolved_arguments);
	const Dictionary process = OS::get_singleton()->execute_with_pipe(codex_executable, resolved_arguments, false);
	if (process.is_empty()) {
		_set_error(TTRC("Unable to start Codex app-server"));
		return ERR_CANT_CREATE;
	}
	stdio = process.get("stdio", Ref<FileAccess>());
	stderr_pipe = process.get("stderr", Ref<FileAccess>());
	process_id = process.get("pid", ProcessID(0));
	if (!stdio.is_valid() || process_id == 0) {
		_clear_process();
		_set_error(TTRC("Codex app-server did not provide a valid stdio transport"));
		return ERR_CANT_CREATE;
	}

	state = STATE_CONNECTING;
	last_error = String();
	Dictionary client_info;
	client_info["name"] = "godot_ai_workspace";
	client_info["title"] = "Godot AI Workspace";
	client_info["version"] = "0.1.0";
	Dictionary params;
	params["clientInfo"] = client_info;
	Dictionary initialize;
	initialize["id"] = int(next_request_id++);
	initialize_request_id = uint64_t(initialize["id"]);
	initialize["method"] = "initialize";
	initialize["params"] = params;
	if (!_send(initialize)) {
		_set_error(TTRC("Unable to send initialize to Codex app-server"));
		disconnect();
		return ERR_CANT_CONNECT;
	}
	return OK;
}

void AIWorkspaceCodexAdapter::_send_session_request(bool p_resume) {
	Dictionary request;
	request["id"] = int(next_request_id++);
	thread_request_id = uint64_t(request["id"]);
	request["method"] = p_resume ? "thread/resume" : "thread/start";
	Dictionary params;
	if (p_resume) {
		params["threadId"] = persisted_thread_id;
	}
	request["params"] = params;
	session_request = p_resume ? SESSION_REQUEST_RESUME : SESSION_REQUEST_START;
	if (!_send(request)) {
		_set_error(TTRC("Unable to initialize the Codex session"));
	}
}

void AIWorkspaceCodexAdapter::_clear_process() {
	if (stdio.is_valid()) {
		stdio->close();
	}
	if (stderr_pipe.is_valid()) {
		stderr_pipe->close();
	}
	if (process_id != 0 && OS::get_singleton()->is_process_running(process_id)) {
		OS::get_singleton()->kill(process_id);
	}
	stdio.unref();
	stderr_pipe.unref();
	process_id = 0;
	receive_buffer = String();
	turn_active = false;
	active_turn_id = String();
	interrupt_requested = false;
	current_assistant_index = -1;
}

void AIWorkspaceCodexAdapter::disconnect() {
	_save_persistent_state();
	_clear_process();
	thread_id = String();
	active_turn_id = String();
	state = STATE_DISCONNECTED;
	last_error = String();
}

void AIWorkspaceCodexAdapter::_add_command_proposal() {
	const int marker = response_text.find(COMMAND_MARKER);
	if (marker < 0) {
		return;
	}
	const String command_line = response_text.substr(marker + COMMAND_MARKER.length()).get_slice("\n", 0).strip_edges();
	const Variant command = JSON::parse_string(command_line);
	if (command.get_type() != Variant::DICTIONARY) {
		return;
	}
	const Dictionary command_dictionary = command;
	if (!command_dictionary.has("command") || !command_dictionary.has("args")) {
		return;
	}
	pending_command_json = JSON::stringify(command_dictionary);
	pending_command_summary = String(command_dictionary.get("command", TTRC("unknown command")));
	Dictionary proposal;
	proposal["role"] = "proposal";
	proposal["text"] = pending_command_summary + "\n" + pending_command_json;
	proposal["status"] = "pending";
	proposal["command"] = pending_command_summary;
	chat_history.push_back(proposal);
	current_proposal_index = chat_history.size() - 1;
}

void AIWorkspaceCodexAdapter::_finish_turn(const String &p_status) {
	if (!turn_active && current_assistant_index < 0) {
		return;
	}
	turn_active = false;
	active_turn_id = String();
	interrupt_request_id = 0;
	const String normalized_status = p_status.to_lower();
	const String message_status = normalized_status == "interrupted" || interrupt_requested ? "interrupted" : (normalized_status == "completed" ? "completed" : "failed");
	if (current_assistant_index >= 0 && current_assistant_index < chat_history.size()) {
		Dictionary assistant = chat_history[current_assistant_index];
		assistant["text"] = response_text;
		assistant["status"] = message_status;
		chat_history[current_assistant_index] = assistant;
	}
	if (message_status == "completed") {
		_add_command_proposal();
	}
	interrupt_requested = false;
	current_assistant_index = -1;
	_save_persistent_state();
}

void AIWorkspaceCodexAdapter::_handle_line(const String &p_line) {
	const Variant parsed = JSON::parse_string(p_line);
	if (parsed.get_type() != Variant::DICTIONARY) {
		return;
	}
	_handle_message(parsed);
}

void AIWorkspaceCodexAdapter::_handle_message(const Dictionary &p_message) {
	if (p_message.has("method") && p_message.has("id") && !p_message.has("result") && !p_message.has("error")) {
		Dictionary error;
		error["code"] = -32601;
		error["message"] = TTRC("Godot AI Workspace does not expose Codex server-request tools in this stage");
		Dictionary response;
		response["id"] = p_message["id"];
		response["error"] = error;
		_send(response);
		return;
	}
	if (p_message.has("error")) {
		const Dictionary error = p_message["error"];
		const String message = error.get("message", TTRC("Codex app-server request failed"));
		const uint64_t error_id = uint64_t(int64_t(p_message.get("id", 0)));
		if (error_id == thread_request_id && session_request == SESSION_REQUEST_RESUME && !_is_authentication_error(message)) {
			persisted_thread_id = String();
			_send_session_request(false);
			return;
		}
		_set_error(_is_authentication_error(message) ? TTRC("Authentication required") : message, _is_authentication_error(message) ? STATE_AUTHENTICATION_REQUIRED : STATE_ERROR);
		return;
	}
	const String method = p_message.get("method", String());
	const uint64_t id = uint64_t(int64_t(p_message.get("id", 0)));
	if (id == initialize_request_id && p_message.has("result")) {
		Dictionary initialized;
		initialized["method"] = "initialized";
		_send(initialized);
		_send_session_request(!persisted_thread_id.is_empty());
		return;
	}
	if (id == thread_request_id && p_message.has("result")) {
		const Dictionary result = p_message["result"];
		const Dictionary thread = result.get("thread", Dictionary());
		thread_id = thread.get("id", String());
		if (thread_id.is_empty()) {
			_set_error(TTRC("Codex app-server returned no thread id"));
		} else {
			if (session_request == SESSION_REQUEST_START) {
				chat_history.clear();
				response_text = String();
			}
			persisted_thread_id = thread_id;
			_save_persistent_state();
			state = STATE_CONNECTED;
			last_error = String();
		}
		session_request = SESSION_REQUEST_NONE;
		return;
	}
	if (method == "item/agentMessage/delta") {
		const Dictionary params = p_message.get("params", Dictionary());
		const String event_turn_id = params.get("turnId", String());
		if (!active_turn_id.is_empty() && !event_turn_id.is_empty() && event_turn_id != active_turn_id) {
			return;
		}
		const String delta = params.get("delta", String());
		response_text += delta;
		if (current_assistant_index >= 0 && current_assistant_index < chat_history.size()) {
			Dictionary assistant = chat_history[current_assistant_index];
			assistant["text"] = response_text;
			assistant["status"] = "generating";
			chat_history[current_assistant_index] = assistant;
		}
		return;
	}
	if (method == "turn/completed") {
		const Dictionary params = p_message.get("params", Dictionary());
		const Dictionary turn = params.get("turn", Dictionary());
		const String event_turn_id = turn.get("id", String());
		if (!active_turn_id.is_empty() && !event_turn_id.is_empty() && event_turn_id != active_turn_id) {
			return;
		}
		const String status = turn.get("status", String());
		_finish_turn(status.is_empty() ? String("completed") : status);
		return;
	}
	if (method == "turn/started") {
		const Dictionary params = p_message.get("params", Dictionary());
		const Dictionary turn = params.get("turn", Dictionary());
		active_turn_id = turn.get("id", active_turn_id);
		return;
	}
	if (id == turn_request_id && p_message.has("result")) {
		const Dictionary result = p_message["result"];
		const Dictionary turn = result.get("turn", Dictionary());
		active_turn_id = turn.get("id", active_turn_id);
		return;
	}
	if (id == interrupt_request_id && p_message.has("result")) {
		interrupt_request_id = 0;
		return;
	}
	if (method == "thread/started") {
		return;
	}
	if (method == "error") {
		const Dictionary params = p_message.get("params", Dictionary());
		const String message = params.get("message", TTRC("Codex reported an error"));
		_set_error(_is_authentication_error(message) ? TTRC("Authentication required") : message, _is_authentication_error(message) ? STATE_AUTHENTICATION_REQUIRED : STATE_ERROR);
		return;
	}
	if (method == "turn/failed") {
		_finish_turn("failed");
		return;
	}
	return;
}

void AIWorkspaceCodexAdapter::poll() {
	if (!stdio.is_valid()) {
		return;
	}
	if (process_id != 0 && !OS::get_singleton()->is_process_running(process_id)) {
		_set_error(TTRC("Codex app-server stopped unexpectedly"));
		_clear_process();
		return;
	}
	const uint64_t available = stdio->get_length() - stdio->get_position();
	if (available > 0 && available < 256 * 1024) {
		const Vector<uint8_t> bytes = stdio->get_buffer(available);
		if (!bytes.is_empty()) {
			receive_buffer += String::utf8(reinterpret_cast<const char *>(bytes.ptr()), bytes.size());
		}
	}
	int processed = 0;
	while (processed < 64) {
		const int newline = receive_buffer.find("\n");
		if (newline < 0) {
			break;
		}
		const String line = receive_buffer.substr(0, newline).strip_edges();
		receive_buffer = receive_buffer.substr(newline + 1);
		if (!line.is_empty()) {
			_handle_line(line);
		}
		processed++;
	}
}

Error AIWorkspaceCodexAdapter::send_instruction(const String &p_instruction) {
	if (state != STATE_CONNECTED || thread_id.is_empty() || turn_active || !pending_command_json.is_empty() || p_instruction.strip_edges().is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	response_text = String();
	pending_command_json = String();
	pending_command_summary = String();
	String prompt = p_instruction;
	if (context_service) {
		context_service->refresh();
		prompt += "\n\nEditor context (read-only JSON):\n" + context_service->get_context_json();
	}
	prompt += "\n\nTreat editor context as read-only. Never execute or authorize a mutation. If the user explicitly requests a scene mutation, optionally propose exactly one command on a line beginning with AI_WORKSPACE_COMMAND: followed by a JSON object using the existing Godot Command Router schema, for example {\"command\":\"node.create\",\"args\":{\"parent_path\":\".\",\"type\":\"Node2D\",\"name\":\"CodexEnemy\"}}. Do not invent action, scene_path, node_type, or node_name fields. The Godot UI validates the proposal and requires separate approval.";
	Dictionary input;
	input["type"] = "text";
	input["text"] = prompt;
	Array input_items;
	input_items.push_back(input);
	Dictionary params;
	params["threadId"] = thread_id;
	params["input"] = input_items;
	Dictionary turn_start;
	turn_start["id"] = int(next_request_id++);
	turn_request_id = uint64_t(turn_start["id"]);
	turn_start["method"] = "turn/start";
	turn_start["params"] = params;
	if (!_send(turn_start)) {
		_set_error(TTRC("Unable to send instruction to Codex app-server"));
		return ERR_CANT_CONNECT;
	}
	turn_active = true;
	Dictionary user_message;
	user_message["role"] = "user";
	user_message["text"] = p_instruction;
	user_message["status"] = "sent";
	chat_history.push_back(user_message);
	Dictionary assistant_message;
	assistant_message["role"] = "assistant";
	assistant_message["text"] = String();
	assistant_message["status"] = "generating";
	chat_history.push_back(assistant_message);
	current_assistant_index = chat_history.size() - 1;
	current_proposal_index = -1;
	return OK;
}

Error AIWorkspaceCodexAdapter::stop_generation() {
	if (!turn_active || thread_id.is_empty() || active_turn_id.is_empty() || !stdio.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	Dictionary params;
	params["threadId"] = thread_id;
	params["turnId"] = active_turn_id;
	Dictionary interrupt;
	interrupt["id"] = int(next_request_id++);
	interrupt_request_id = uint64_t(interrupt["id"]);
	interrupt["method"] = "turn/interrupt";
	interrupt["params"] = params;
	interrupt_requested = true;
	if (!_send(interrupt)) {
		interrupt_requested = false;
		_set_error(TTRC("Unable to interrupt Codex generation"));
		return ERR_CANT_CONNECT;
	}
	return OK;
}

Error AIWorkspaceCodexAdapter::new_session() {
	if (state != STATE_CONNECTED || turn_active) {
		return ERR_BUSY;
	}
	persisted_thread_id = String();
	thread_id = String();
	chat_history.clear();
	response_text = String();
	pending_command_json = String();
	pending_command_summary = String();
	current_assistant_index = -1;
	current_proposal_index = -1;
	_send_session_request(false);
	state = STATE_CONNECTING;
	return OK;
}

Error AIWorkspaceCodexAdapter::resume_session(const String &p_thread_id) {
	if (state != STATE_CONNECTED || turn_active || p_thread_id.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	persisted_thread_id = p_thread_id;
	_send_session_request(true);
	state = STATE_CONNECTING;
	return OK;
}

Dictionary AIWorkspaceCodexAdapter::approve_pending_command(AIWorkspaceCommandRouter *p_command_router) {
	if (!p_command_router || pending_command_json.is_empty()) {
		return Dictionary();
	}
	const Dictionary result = p_command_router->execute_json(pending_command_json);
	if (current_proposal_index >= 0 && current_proposal_index < chat_history.size()) {
		Dictionary proposal = chat_history[current_proposal_index];
		const bool success = result.get("success", false);
		proposal["status"] = success ? "executed" : "failed";
		proposal["result"] = JSON::stringify(result);
		chat_history[current_proposal_index] = proposal;
	}
	pending_command_json = String();
	pending_command_summary = String();
	_save_persistent_state();
	return result;
}

void AIWorkspaceCodexAdapter::reject_pending_command() {
	if (current_proposal_index >= 0 && current_proposal_index < chat_history.size()) {
		Dictionary proposal = chat_history[current_proposal_index];
		proposal["status"] = "rejected";
		chat_history[current_proposal_index] = proposal;
	}
	pending_command_json = String();
	pending_command_summary = String();
	_save_persistent_state();
}
