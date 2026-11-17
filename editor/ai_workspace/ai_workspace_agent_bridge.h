/**************************************************************************/
/*  ai_workspace_agent_bridge.h                                           */
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
/* WITHOUT EVEN THE IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A  */
/* PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS  */
/* OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER          */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,        */
/* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR  */
/* OTHER DEALINGS IN THE SOFTWARE.                                        */
/**************************************************************************/

#pragma once

#include "core/io/tcp_server.h"
#include "core/io/stream_peer_tcp.h"
#include "core/object/object.h"

class AIWorkspaceCommandRouter;
class AIWorkspaceContextService;

class AIWorkspaceAgentBridge : public Object {
	GDCLASS(AIWorkspaceAgentBridge, Object);

	static constexpr int MAX_MESSAGE_SIZE = 64 * 1024;
	static constexpr int MAX_RESPONSE_SIZE = 256 * 1024;
	static constexpr int MAX_CONTAINER_ITEMS = 256;
	static constexpr int MAX_JSON_DEPTH = 16;
	static constexpr uint16_t DEFAULT_PORT = 47831;

	Ref<TCPServer> server;
	Ref<StreamPeerTCP> connection;
	String receive_buffer;
	String session_secret;
	String last_error;
	String pending_summary;
	Dictionary pending_request;
	AIWorkspaceContextService *context_service = nullptr;
	AIWorkspaceCommandRouter *command_router = nullptr;
	bool enabled = false;
	uint16_t port = DEFAULT_PORT;

	String _generate_secret() const;
	bool _validate_limits(const Variant &p_value, int p_depth, int &r_items) const;
	Dictionary _error(const String &p_id, const String &p_code, const String &p_message) const;
	Dictionary _success(const String &p_id, const Variant &p_result) const;
	Dictionary _process_message(const String &p_message);
	void _send_response(const Dictionary &p_response);
	void _close_connection();

public:
	AIWorkspaceAgentBridge(AIWorkspaceContextService *p_context_service, AIWorkspaceCommandRouter *p_command_router);
	~AIWorkspaceAgentBridge();

	Error start(const String &p_secret = String());
	void stop();
	void poll();

	bool is_enabled() const { return enabled; }
	bool has_connection() const { return connection.is_valid() && connection->get_status() == StreamPeerSocket::STATUS_CONNECTED; }
	bool has_pending_request() const { return !pending_request.is_empty(); }
	String get_session_secret() const { return session_secret; }
	String get_last_error() const { return last_error; }
	String get_pending_summary() const { return pending_summary; }
	uint16_t get_port() const { return port; }

	Dictionary approve_pending_request();
	Dictionary reject_pending_request();
};
