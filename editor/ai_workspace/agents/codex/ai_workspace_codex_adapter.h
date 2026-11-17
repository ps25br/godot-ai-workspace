/**************************************************************************/
/*  ai_workspace_codex_adapter.h                                         */
/**************************************************************************/

#pragma once

#include "core/io/file_access.h"
#include "core/os/process_id.h"
#include "core/object/object.h"

class AIWorkspaceCommandRouter;
class AIWorkspaceContextService;

class AIWorkspaceCodexAdapter : public Object {
	GDCLASS(AIWorkspaceCodexAdapter, Object);

public:
	enum State {
		STATE_NOT_INSTALLED,
		STATE_AUTHENTICATION_REQUIRED,
		STATE_DISCONNECTED,
		STATE_CONNECTING,
		STATE_CONNECTED,
		STATE_ERROR,
	};

private:
	AIWorkspaceContextService *context_service = nullptr;
	Ref<FileAccess> stdio;
	Ref<FileAccess> stderr_pipe;
	ProcessID process_id = 0;
	State state = STATE_DISCONNECTED;
	String receive_buffer;
	String response_text;
	String last_error;
	String thread_id;
	String pending_command_json;
	String pending_command_summary;
	uint64_t next_request_id = 1;
	uint64_t initialize_request_id = 0;
	uint64_t thread_request_id = 0;
	bool turn_active = false;

	bool _send(const Dictionary &p_message);
	void _handle_message(const Dictionary &p_message);
	void _handle_line(const String &p_line);
	void _set_error(const String &p_error, State p_state = STATE_ERROR);
	void _clear_process();
	bool _is_authentication_error(const String &p_error) const;

public:
	AIWorkspaceCodexAdapter(AIWorkspaceContextService *p_context_service);
	~AIWorkspaceCodexAdapter();

	void detect();
	Error connect_codex();
	void disconnect();
	void poll();
	Error send_instruction(const String &p_instruction);

	State get_state() const { return state; }
	String get_state_text() const;
	String get_last_error() const { return last_error; }
	String get_response_text() const { return response_text; }
	bool has_pending_command() const { return !pending_command_json.is_empty(); }
	String get_pending_command_summary() const { return pending_command_summary; }
	Dictionary approve_pending_command(AIWorkspaceCommandRouter *p_command_router);
	void reject_pending_command();
};

VARIANT_ENUM_CAST(AIWorkspaceCodexAdapter::State);
