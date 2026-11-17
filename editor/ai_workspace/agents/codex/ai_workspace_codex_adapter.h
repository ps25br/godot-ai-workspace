/**************************************************************************/
/*  ai_workspace_codex_adapter.h                                         */
/**************************************************************************/

#pragma once

#include "core/io/file_access.h"
#include "core/os/process_id.h"
#include "core/object/object.h"
#include "core/templates/list.h"

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
	String codex_executable = "codex";
	String codex_script;
	State state = STATE_DISCONNECTED;
	String receive_buffer;
	String response_text;
	String last_error;
	String thread_id;
	String pending_command_json;
	String pending_command_summary;
	String active_turn_id;
	String persisted_thread_id;
	Array chat_history;
	uint64_t next_request_id = 1;
	uint64_t initialize_request_id = 0;
	uint64_t thread_request_id = 0;
	uint64_t turn_request_id = 0;
	uint64_t interrupt_request_id = 0;
	int current_assistant_index = -1;
	int current_proposal_index = -1;
	enum SessionRequest {
		SESSION_REQUEST_NONE,
		SESSION_REQUEST_START,
		SESSION_REQUEST_RESUME,
	};
	SessionRequest session_request = SESSION_REQUEST_NONE;
	bool turn_active = false;
	bool interrupt_requested = false;

	bool _send(const Dictionary &p_message);
	void _resolve_codex_command();
	void _prepare_codex_arguments(const List<String> &p_arguments, List<String> &r_arguments) const;
	void _handle_message(const Dictionary &p_message);
	void _handle_line(const String &p_line);
	void _set_error(const String &p_error, State p_state = STATE_ERROR);
	void _clear_process();
	void _send_session_request(bool p_resume);
	void _load_persistent_state();
	void _save_persistent_state();
	void _finish_turn(const String &p_status);
	void _add_command_proposal();
	bool _is_authentication_error(const String &p_error) const;

public:
	AIWorkspaceCodexAdapter(AIWorkspaceContextService *p_context_service);
	~AIWorkspaceCodexAdapter();

	void detect();
	Error connect_codex();
	void disconnect();
	void poll();
	Error send_instruction(const String &p_instruction);
	Error stop_generation();
	Error new_session();
	Error resume_session(const String &p_thread_id);

	State get_state() const { return state; }
	String get_state_text() const;
	String get_last_error() const { return last_error; }
	String get_response_text() const { return response_text; }
	Array get_chat_history() const { return chat_history; }
	bool is_generating() const { return turn_active; }
	String get_thread_id() const { return thread_id; }
	String get_active_turn_id() const { return active_turn_id; }
	bool has_pending_command() const { return !pending_command_json.is_empty(); }
	String get_pending_command_summary() const { return pending_command_summary; }
	Dictionary approve_pending_command(AIWorkspaceCommandRouter *p_command_router);
	void reject_pending_command();
};

VARIANT_ENUM_CAST(AIWorkspaceCodexAdapter::State);
