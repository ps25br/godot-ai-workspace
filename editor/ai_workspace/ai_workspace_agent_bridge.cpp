/**************************************************************************/
/*  ai_workspace_agent_bridge.cpp                                         */
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
/* WITHOUT EVEN THE IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A    */
/* PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS  */
/* OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER          */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,        */
/* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR  */
/* OTHER DEALINGS IN THE SOFTWARE.                                        */
/**************************************************************************/

#include "ai_workspace_agent_bridge.h"

#include "editor/ai_workspace/ai_workspace_command_router.h"
#include "editor/ai_workspace/ai_workspace_context_service.h"

#include "core/crypto/crypto_core.h"
#include "core/io/json.h"
#include "core/io/ip_address.h"

String AIWorkspaceAgentBridge::_generate_secret() const {
	CryptoCore::RandomGenerator random;
	if (random.init() != OK) {
		return String();
	}
	uint8_t bytes[32];
	if (random.get_random_bytes(bytes, sizeof(bytes)) != OK) {
		return String();
	}
	return CryptoCore::b64_encode_str(bytes, sizeof(bytes));
}

bool AIWorkspaceAgentBridge::_validate_limits(const Variant &p_value, int p_depth, int &r_items) const {
	if (p_depth > MAX_JSON_DEPTH) {
		return false;
	}
	if (p_value.get_type() == Variant::ARRAY) {
		Array array = p_value;
		if (array.size() > MAX_CONTAINER_ITEMS) {
			return false;
		}
		r_items += array.size();
		for (const Variant &value : array) {
			if (!_validate_limits(value, p_depth + 1, r_items)) {
				return false;
			}
		}
	} else if (p_value.get_type() == Variant::DICTIONARY) {
		Dictionary dictionary = p_value;
		if (dictionary.size() > MAX_CONTAINER_ITEMS) {
			return false;
		}
		r_items += dictionary.size();
		for (const Variant &key : dictionary.get_key_list()) {
			if (key.get_type() != Variant::STRING && key.get_type() != Variant::STRING_NAME) {
				return false;
			}
			if (!_validate_limits(dictionary[key], p_depth + 1, r_items)) {
				return false;
			}
		}
	}
	return r_items <= MAX_CONTAINER_ITEMS;
}

Dictionary AIWorkspaceAgentBridge::_error(const String &p_id, const String &p_code, const String &p_message) const {
	Dictionary error;
	error["code"] = p_code;
	error["message"] = p_message;
	Dictionary response;
	response["protocol"] = "ai-workspace/1";
	response["id"] = p_id;
	response["success"] = false;
	response["error"] = error;
	return response;
}

Dictionary AIWorkspaceAgentBridge::_success(const String &p_id, const Variant &p_result) const {
	Dictionary response;
	response["protocol"] = "ai-workspace/1";
	response["id"] = p_id;
	response["success"] = true;
	response["result"] = p_result;
	return response;
}

Dictionary AIWorkspaceAgentBridge::_process_message(const String &p_message) {
	if (p_message.length() > MAX_MESSAGE_SIZE) {
		return _error(String(), "MESSAGE_TOO_LARGE", "The message exceeds the 64 KiB limit.");
	}

	JSON json;
	if (json.parse(p_message) != OK) {
		return _error(String(), "INVALID_JSON", json.get_error_message());
	}
	Variant parsed = json.get_data();
	int item_count = 0;
	if (parsed.get_type() != Variant::DICTIONARY || !_validate_limits(parsed, 0, item_count)) {
		return _error(String(), "INVALID_REQUEST", "The JSON structure exceeds the protocol limits.");
	}
	Dictionary request = parsed;
	const String id = request.get("id", String());
	if (id.is_empty() || id.length() > 128 || request.get("protocol", String()) != "ai-workspace/1") {
		return _error(id, "INVALID_REQUEST", "A valid protocol and request id are required.");
	}
	const String auth = request.get("auth", String());
	if (auth != session_secret) {
		return _error(id, "UNAUTHORIZED", "The session secret is invalid.");
	}
	const String method = request.get("method", String());
	const Variant params_variant = request.get("params", Dictionary());
	if (params_variant.get_type() != Variant::DICTIONARY) {
		return _error(id, "INVALID_REQUEST", "params must be an object.");
	}
	Dictionary params = params_variant;

	if (method == "bridge.ping") {
		Dictionary result;
		result["enabled"] = enabled;
		result["port"] = port;
		return _success(id, result);
	}
	if (method == "context.get") {
		if (context_service) {
			context_service->refresh();
		}
		return _success(id, context_service ? context_service->get_context() : Dictionary());
	}
	if (method != "command.execute") {
		return _error(id, "UNKNOWN_METHOD", "The requested method is not supported.");
	}
	if (!pending_request.is_empty()) {
		return _error(id, "APPROVAL_PENDING", "Another command is already waiting for approval.");
	}
	const String command = params.get("command", String());
	const Variant args_variant = params.get("args", Dictionary());
	if (command.is_empty() || args_variant.get_type() != Variant::DICTIONARY) {
		return _error(id, "INVALID_REQUEST", "command and args are required.");
	}
	pending_request = request;
	pending_summary = command;
	return Dictionary();
}

void AIWorkspaceAgentBridge::_send_response(const Dictionary &p_response) {
	if (!has_connection()) {
		return;
	}
	const String payload = JSON::stringify(p_response) + "\n";
	if (payload.length() > MAX_RESPONSE_SIZE) {
		_close_connection();
		return;
	}
	const CharString payload_utf8 = payload.utf8();
	connection->put_data((const uint8_t *)payload_utf8.get_data(), payload_utf8.size());
}

void AIWorkspaceAgentBridge::_close_connection() {
	if (connection.is_valid()) {
		connection->disconnect_from_host();
	}
	connection.unref();
	receive_buffer = String();
	if (!pending_request.is_empty()) {
		pending_request.clear();
		pending_summary = String();
	}
}

Error AIWorkspaceAgentBridge::start(const String &p_secret) {
	if (enabled) {
		return OK;
	}
	server.instantiate();
	const Error err = server->listen(port, IPAddress("127.0.0.1"));
	if (err != OK) {
		last_error = "Unable to listen on 127.0.0.1:" + itos(port);
		server.unref();
		return err;
	}
	session_secret = p_secret.is_empty() ? _generate_secret() : p_secret;
	if (session_secret.is_empty()) {
		server->stop();
		server.unref();
		last_error = "Unable to generate a session secret.";
		return ERR_CANT_CREATE;
	}
	enabled = true;
	last_error = String();
	return OK;
}

void AIWorkspaceAgentBridge::stop() {
	_close_connection();
	if (server.is_valid()) {
		server->stop();
		server.unref();
	}
	enabled = false;
	session_secret = String();
}

void AIWorkspaceAgentBridge::poll() {
	if (!enabled || server.is_null()) {
		return;
	}
	if (!has_connection() && server->is_connection_available()) {
		Ref<StreamPeerTCP> candidate = server->take_connection();
		if (candidate.is_valid()) {
			candidate->poll();
			if (candidate->get_connected_host() != IPAddress("127.0.0.1")) {
				candidate->disconnect_from_host();
			} else {
				connection = candidate;
				connection->set_no_delay(true);
			}
		}
	}
	if (!has_connection()) {
		return;
	}
	connection->poll();
	if (!has_connection()) {
		_close_connection();
		return;
	}
	const int available = connection->get_available_bytes();
	if (available > MAX_MESSAGE_SIZE - receive_buffer.length()) {
		_send_response(_error(String(), "MESSAGE_TOO_LARGE", "The message exceeds the 64 KiB limit."));
		_close_connection();
		return;
	}
	if (available > 0) {
		Vector<uint8_t> bytes;
		bytes.resize(available);
		int received = 0;
		if (connection->get_partial_data(bytes.ptrw(), available, received) != OK || received <= 0) {
			_close_connection();
			return;
		}
		receive_buffer += String::utf8((const char *)bytes.ptr(), received);
	}
	int newline = receive_buffer.find("\n");
	while (newline >= 0) {
		const String message = receive_buffer.substr(0, newline).strip_edges();
		receive_buffer = receive_buffer.substr(newline + 1);
		const Dictionary response = _process_message(message);
		if (!response.is_empty()) {
			_send_response(response);
		}
		newline = receive_buffer.find("\n");
	}
}

Dictionary AIWorkspaceAgentBridge::approve_pending_request() {
	if (pending_request.is_empty()) {
		return _error(String(), "NO_PENDING_REQUEST", "There is no command waiting for approval.");
	}
	const String id = pending_request.get("id", String());
	const Dictionary params = pending_request.get("params", Dictionary());
	const String command = params.get("command", String());
	Dictionary command_request;
	command_request["command"] = command;
	command_request["args"] = params.get("args", Dictionary());
	Dictionary result = command_router ? command_router->execute(command_request) : Dictionary();
	Dictionary response;
	if (result.get("success", false)) {
		response = _success(id, result);
	} else {
		const Dictionary error = result.get("error", Dictionary());
		response = _error(id, error.get("code", "COMMAND_FAILED"), error.get("message", "The command was rejected."));
	}
	pending_request.clear();
	pending_summary = String();
	_send_response(response);
	return response;
}

Dictionary AIWorkspaceAgentBridge::reject_pending_request() {
	if (pending_request.is_empty()) {
		return _error(String(), "NO_PENDING_REQUEST", "There is no command waiting for approval.");
	}
	const String id = pending_request.get("id", String());
	pending_request.clear();
	pending_summary = String();
	Dictionary response = _error(id, "COMMAND_REJECTED", "The editor user rejected the command.");
	_send_response(response);
	return response;
}

AIWorkspaceAgentBridge::AIWorkspaceAgentBridge(AIWorkspaceContextService *p_context_service, AIWorkspaceCommandRouter *p_command_router) {
	context_service = p_context_service;
	command_router = p_command_router;
}

AIWorkspaceAgentBridge::~AIWorkspaceAgentBridge() {
	stop();
}
