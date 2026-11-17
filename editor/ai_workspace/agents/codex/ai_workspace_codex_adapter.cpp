/**************************************************************************/
/*  ai_workspace_codex_adapter.cpp                                       */
/**************************************************************************/

#include "ai_workspace_codex_adapter.h"

#include "editor/ai_workspace/ai_workspace_command_router.h"
#include "editor/ai_workspace/ai_workspace_context_service.h"

#include "core/io/json.h"
#include "core/os/os.h"

namespace {
const String COMMAND_MARKER = "AI_WORKSPACE_COMMAND:";
}

AIWorkspaceCodexAdapter::AIWorkspaceCodexAdapter(AIWorkspaceContextService *p_context_service) {
	context_service = p_context_service;
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

bool AIWorkspaceCodexAdapter::_is_authentication_error(const String &p_error) const {
	const String error = p_error.to_lower();
	return error.contains("auth") || error.contains("login") || error.contains("credential") || error.contains("token") || error.contains("unauthorized");
}

void AIWorkspaceCodexAdapter::detect() {
	if (state == STATE_CONNECTING || state == STATE_CONNECTED) {
		return;
	}
	List<String> arguments;
	arguments.push_back("--version");
	String output;
	int exit_code = -1;
	const Error error = OS::get_singleton()->execute("codex", arguments, &output, &exit_code, false);
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
	const Dictionary process = OS::get_singleton()->execute_with_pipe("codex", arguments, false);
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
}

void AIWorkspaceCodexAdapter::disconnect() {
	_clear_process();
	thread_id = String();
	state = STATE_DISCONNECTED;
	last_error = String();
}

void AIWorkspaceCodexAdapter::_handle_line(const String &p_line) {
	const Variant parsed = JSON::parse_string(p_line);
	if (parsed.get_type() != Variant::DICTIONARY) {
		return;
	}
	_handle_message(parsed);
}

void AIWorkspaceCodexAdapter::_handle_message(const Dictionary &p_message) {
	if (p_message.has("error")) {
		const Dictionary error = p_message["error"];
		const String message = error.get("message", TTRC("Codex app-server request failed"));
		_set_error(_is_authentication_error(message) ? TTRC("Authentication required") : message, _is_authentication_error(message) ? STATE_AUTHENTICATION_REQUIRED : STATE_ERROR);
		return;
	}
	const String method = p_message.get("method", String());
	const uint64_t id = uint64_t(int64_t(p_message.get("id", 0)));
	if (id == initialize_request_id && p_message.has("result")) {
		Dictionary initialized;
		initialized["method"] = "initialized";
		_send(initialized);
		Dictionary thread_start;
		thread_start["id"] = int(next_request_id++);
		thread_request_id = uint64_t(thread_start["id"]);
		thread_start["method"] = "thread/start";
		thread_start["params"] = Dictionary();
		_send(thread_start);
		return;
	}
	if (id == thread_request_id && p_message.has("result")) {
		const Dictionary result = p_message["result"];
		const Dictionary thread = result.get("thread", Dictionary());
		thread_id = thread.get("id", String());
		if (thread_id.is_empty()) {
			_set_error(TTRC("Codex app-server returned no thread id"));
		} else {
			state = STATE_CONNECTED;
			last_error = String();
		}
		return;
	}
	if (method == "item/agentMessage/delta") {
		const Dictionary params = p_message.get("params", Dictionary());
		response_text += String(params.get("delta", String()));
		return;
	}
	if (method == "turn/completed") {
		turn_active = false;
		const Dictionary params = p_message.get("params", Dictionary());
		const Dictionary turn = params.get("turn", Dictionary());
		const String status = turn.get("status", String());
		if (status != "completed" && !status.is_empty()) {
			_set_error(vformat(TTRC("Codex turn ended with status: %s"), status));
		}
		const int marker = response_text.find(COMMAND_MARKER);
		if (marker >= 0) {
			String command_line = response_text.substr(marker + COMMAND_MARKER.length()).get_slice("\n", 0).strip_edges();
			const Variant command = JSON::parse_string(command_line);
			if (command.get_type() == Variant::DICTIONARY) {
				const Dictionary command_dictionary = command;
				if (command_dictionary.has("command") && command_dictionary.has("args")) {
					pending_command_json = JSON::stringify(command_dictionary);
					pending_command_summary = String(command_dictionary.get("command", TTRC("unknown command")));
				}
			}
		}
		return;
	}
	if (method == "error") {
		const Dictionary params = p_message.get("params", Dictionary());
		const String message = params.get("message", TTRC("Codex reported an error"));
		_set_error(_is_authentication_error(message) ? TTRC("Authentication required") : message, _is_authentication_error(message) ? STATE_AUTHENTICATION_REQUIRED : STATE_ERROR);
		return;
	}
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
	if (state != STATE_CONNECTED || thread_id.is_empty() || turn_active || p_instruction.strip_edges().is_empty()) {
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
	turn_start["method"] = "turn/start";
	turn_start["params"] = params;
	if (!_send(turn_start)) {
		_set_error(TTRC("Unable to send instruction to Codex app-server"));
		return ERR_CANT_CONNECT;
	}
	turn_active = true;
	return OK;
}

Dictionary AIWorkspaceCodexAdapter::approve_pending_command(AIWorkspaceCommandRouter *p_command_router) {
	if (!p_command_router || pending_command_json.is_empty()) {
		return Dictionary();
	}
	const Dictionary result = p_command_router->execute_json(pending_command_json);
	pending_command_json = String();
	pending_command_summary = String();
	return result;
}

void AIWorkspaceCodexAdapter::reject_pending_command() {
	pending_command_json = String();
	pending_command_summary = String();
}
