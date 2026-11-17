/**************************************************************************/
/*  ai_workspace_command_router.h                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
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

#include "core/object/object.h"

class Node;

class AIWorkspaceCommandRouter : public Object {
	GDCLASS(AIWorkspaceCommandRouter, Object);

	Dictionary _error(const String &p_command, const String &p_code, const String &p_message) const;
	Dictionary _success(const String &p_command, const Dictionary &p_result) const;
	Node *_get_edited_scene_root() const;
	Node *_resolve_scene_node(Node *p_scene_root, const String &p_path) const;
	bool _is_editable_scene_node(Node *p_node, Node *p_scene_root, bool p_allow_root) const;
	Dictionary _create_node(const Dictionary &p_args);
	Dictionary _set_node_property(const Dictionary &p_args);
	Dictionary _delete_node(const Dictionary &p_args);

public:
	Dictionary execute(const Dictionary &p_request);
	Dictionary execute_json(const String &p_json);
};
