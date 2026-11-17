/**************************************************************************/
/*  ai_workspace_command_router.cpp                                       */
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

#include "ai_workspace_command_router.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/scene/scene_tree_editor.h"
#include "scene/2d/node_2d.h"
#include "scene/2d/sprite_2d.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/gradient.h"
#include "scene/resources/gradient_texture.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/texture.h"

namespace {

Node *instantiate_allowed_node(const String &p_type) {
	if (p_type == "Node2D") {
		return memnew(Node2D);
	}
	if (p_type == "Sprite2D") {
		return memnew(Sprite2D);
	}
	if (p_type == "Node3D") {
		return memnew(Node3D);
	}
	return nullptr;
}

bool is_number(const Variant &p_value) {
	return p_value.get_type() == Variant::INT || p_value.get_type() == Variant::FLOAT;
}

bool dictionary_has_only(const Dictionary &p_value, const Vector<StringName> &p_keys) {
	if (p_value.size() != p_keys.size()) {
		return false;
	}
	for (const StringName &key : p_keys) {
		if (!p_value.has(key)) {
			return false;
		}
	}
	return true;
}

bool decode_property_value(Node *p_node, const StringName &p_property, const Variant &p_input, Variant &r_value, Variant::Type &r_expected_type) {
	if (Object::cast_to<Node2D>(p_node)) {
		if (p_property == SNAME("position") || p_property == SNAME("scale")) {
			r_expected_type = Variant::VECTOR2;
			if (p_input.get_type() != Variant::DICTIONARY) {
				return false;
			}
			Dictionary value = p_input;
			if (!dictionary_has_only(value, { SNAME("x"), SNAME("y") }) || !is_number(value["x"]) || !is_number(value["y"])) {
				return false;
			}
			r_value = Vector2(double(value["x"]), double(value["y"]));
			return true;
		}
		if (p_property == SNAME("rotation")) {
			r_expected_type = Variant::FLOAT;
			if (!is_number(p_input)) {
				return false;
			}
			r_value = double(p_input);
			return true;
		}
		if (p_property == SNAME("visible")) {
			r_expected_type = Variant::BOOL;
			if (p_input.get_type() != Variant::BOOL) {
				return false;
			}
			r_value = p_input;
			return true;
		}
	}

	if (Object::cast_to<Node3D>(p_node)) {
		if (p_property == SNAME("position") || p_property == SNAME("rotation") || p_property == SNAME("scale")) {
			r_expected_type = Variant::VECTOR3;
			if (p_input.get_type() != Variant::DICTIONARY) {
				return false;
			}
			Dictionary value = p_input;
			if (!dictionary_has_only(value, { SNAME("x"), SNAME("y"), SNAME("z") }) || !is_number(value["x"]) || !is_number(value["y"]) || !is_number(value["z"])) {
				return false;
			}
			r_value = Vector3(double(value["x"]), double(value["y"]), double(value["z"]));
			return true;
		}
		if (p_property == SNAME("visible")) {
			r_expected_type = Variant::BOOL;
			if (p_input.get_type() != Variant::BOOL) {
				return false;
			}
			r_value = p_input;
			return true;
		}
	}

	if (Object::cast_to<Sprite2D>(p_node) && (p_property == SNAME("flip_h") || p_property == SNAME("flip_v"))) {
		r_expected_type = Variant::BOOL;
		if (p_input.get_type() != Variant::BOOL) {
			return false;
		}
		r_value = p_input;
		return true;
	}

	return false;
}

bool has_property_with_type(Node *p_node, const StringName &p_property, Variant::Type p_expected_type) {
	List<PropertyInfo> properties;
	p_node->get_property_list(&properties);
	for (const PropertyInfo &property : properties) {
		if (property.name == p_property && property.type == p_expected_type && (property.usage & PROPERTY_USAGE_EDITOR) && !(property.usage & PROPERTY_USAGE_READ_ONLY)) {
			return true;
		}
	}
	return false;
}

void add_scene_tree_refresh(EditorUndoRedoManager *p_undo_redo) {
	SceneTreeDock *scene_tree_dock = SceneTreeDock::get_singleton();
	if (!scene_tree_dock || !scene_tree_dock->get_tree_editor()) {
		return;
	}
	p_undo_redo->add_do_method(scene_tree_dock->get_tree_editor(), "update_tree");
	p_undo_redo->add_undo_method(scene_tree_dock->get_tree_editor(), "update_tree");
}

bool is_allowed_root_type(const String &p_type) {
	return p_type == "Node2D" || p_type == "Node3D";
}

Node *instantiate_allowed_root(const String &p_type) {
	if (p_type == "Node2D") {
		return memnew(Node2D);
	}
	if (p_type == "Node3D") {
		return memnew(Node3D);
	}
	return nullptr;
}

bool is_allowed_resource_type(const String &p_type) {
	return p_type == "Gradient" || p_type == "GradientTexture2D";
}

Ref<Resource> instantiate_allowed_resource(const String &p_type) {
	if (p_type == "Gradient") {
		return memnew(Gradient);
	}
	if (p_type == "GradientTexture2D") {
		return memnew(GradientTexture2D);
	}
	return Ref<Resource>();
}

bool is_allowed_resource_extension(const String &p_path) {
	const String extension = p_path.get_extension().to_lower();
	return extension == "tres" || extension == "res" || extension == "png" || extension == "jpg" || extension == "jpeg" || extension == "webp";
}

bool path_has_symlink_component(const String &p_path) {
	Ref<DirAccess> dir = DirAccess::open("res://");
	if (dir.is_null()) {
		return true;
	}

	const PackedStringArray components = p_path.trim_prefix("res://").split("/");
	for (int i = 0; i < components.size(); i++) {
		const String component = components[i];
		if (dir->is_link(component)) {
			return true;
		}
		if (i == components.size() - 1) {
			break;
		}
		if (!dir->dir_exists(component)) {
			return false;
		}
		if (dir->change_dir(component) != OK) {
			return true;
		}
	}
	return false;
}

} // namespace

Dictionary AIWorkspaceCommandRouter::_error(const String &p_command, const String &p_code, const String &p_message) const {
	Dictionary error;
	error["code"] = p_code;
	error["message"] = p_message;

	Dictionary response;
	response["success"] = false;
	response["command"] = p_command;
	response["error"] = error;
	return response;
}

Dictionary AIWorkspaceCommandRouter::_success(const String &p_command, const Dictionary &p_result) const {
	Dictionary response;
	response["success"] = true;
	response["command"] = p_command;
	response["result"] = p_result;
	return response;
}

Node *AIWorkspaceCommandRouter::_get_edited_scene_root() const {
	if (!EditorNode::get_singleton()) {
		return nullptr;
	}
	EditorData &editor_data = EditorNode::get_editor_data();
	const int scene_index = editor_data.get_edited_scene();
	if (scene_index < 0 || scene_index >= editor_data.get_edited_scene_count()) {
		return nullptr;
	}
	return editor_data.get_edited_scene_root(scene_index);
}

Node *AIWorkspaceCommandRouter::_resolve_scene_node(Node *p_scene_root, const String &p_path) const {
	if (!p_scene_root || p_path.is_empty()) {
		return nullptr;
	}
	NodePath path(p_path);
	if (path.is_absolute() || path.get_subname_count() > 0) {
		return nullptr;
	}
	for (int i = 0; i < path.get_name_count(); i++) {
		if (path.get_name(i) == SNAME("..")) {
			return nullptr;
		}
	}
	Node *node = p_scene_root->get_node_or_null(path);
	if (!node || (node != p_scene_root && !p_scene_root->is_ancestor_of(node))) {
		return nullptr;
	}
	return node;
}

bool AIWorkspaceCommandRouter::_is_editable_scene_node(Node *p_node, Node *p_scene_root, bool p_allow_root) const {
	if (!p_node || !p_scene_root || p_node->is_internal()) {
		return false;
	}
	if (p_node == p_scene_root) {
		return p_allow_root;
	}
	return p_scene_root->is_ancestor_of(p_node) && p_node->get_owner() == p_scene_root;
}

bool AIWorkspaceCommandRouter::_validate_project_path(const String &p_path, const String &p_extension, String &r_error) const {
	if (p_path.is_empty() || !p_path.begins_with("res://")) {
		r_error = "Path must be inside the project and start with res://.";
		return false;
	}
	if (p_path.contains("\\") || p_path.find("://", 6) >= 0 || p_path.find(":", 6) >= 0) {
		r_error = "Path separators and protocols outside res:// are not allowed.";
		return false;
	}
	if (p_path != p_path.simplify_path()) {
		r_error = "Path must be normalized and cannot contain traversal or duplicate separators.";
		return false;
	}

	const PackedStringArray components = p_path.trim_prefix("res://").split("/");
	if (components.is_empty() || components[0].is_empty()) {
		r_error = "A project-relative file path is required.";
		return false;
	}
	for (const String &component : components) {
		if (component.is_empty() || component == "." || component == "..") {
			r_error = "Path traversal and empty path components are not allowed.";
			return false;
		}
	}
	if (!p_extension.is_empty() && p_path.get_extension().to_lower() != p_extension) {
		r_error = vformat("The path must use the .%s extension.", p_extension);
		return false;
	}
	if (path_has_symlink_component(p_path)) {
		r_error = "Symbolic-link path components are not allowed.";
		return false;
	}
	return true;
}

Dictionary AIWorkspaceCommandRouter::_create_node(const Dictionary &p_args) {
	const String command = "node.create";
	if (!p_args.has("parent_path") || !p_args.has("type") || !p_args.has("name")) {
		return _error(command, "MISSING_ARGUMENT", "parent_path, type, and name are required.");
	}
	if (p_args["parent_path"].get_type() != Variant::STRING || p_args["type"].get_type() != Variant::STRING || p_args["name"].get_type() != Variant::STRING) {
		return _error(command, "INVALID_ARGUMENT_TYPE", "parent_path, type, and name must be strings.");
	}

	Node *scene_root = _get_edited_scene_root();
	if (!scene_root) {
		return _error(command, "NO_EDITED_SCENE", "No edited scene is available.");
	}
	const String parent_path = p_args["parent_path"];
	Node *parent = _resolve_scene_node(scene_root, parent_path);
	if (!parent) {
		return _error(command, "NODE_NOT_FOUND", "The parent path does not resolve inside the edited scene.");
	}
	if (!_is_editable_scene_node(parent, scene_root, true)) {
		return _error(command, "NODE_NOT_EDITABLE", "The parent is internal, protected, or outside the editable scene.");
	}

	const String type = p_args["type"];
	Node *child = instantiate_allowed_node(type);
	if (!child) {
		return _error(command, "INVALID_NODE_TYPE", "Node type is not allowed. Allowed types: Node2D, Sprite2D, Node3D.");
	}

	const String requested_name = p_args["name"];
	if (requested_name.is_empty() || requested_name.validate_node_name() != requested_name || parent->has_node(NodePath(requested_name))) {
		memdelete(child);
		return _error(command, "INVALID_NODE_NAME", "The node name is empty, invalid, or already used by a child.");
	}
	child->set_name(requested_name);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (!undo_redo) {
		memdelete(child);
		return _error(command, "EDITOR_UNAVAILABLE", "The editor Undo/Redo manager is unavailable.");
	}
	undo_redo->create_action_for_history(TTR("AI Workspace: Create Node"), EditorNode::get_editor_data().get_current_edited_scene_history_id());
	undo_redo->add_do_method(parent, "add_child", child, true);
	undo_redo->add_do_method(child, "set_owner", scene_root);
	undo_redo->add_do_reference(child);
	undo_redo->add_undo_method(parent, "remove_child", child);
	add_scene_tree_refresh(undo_redo);
	EditorSelection *selection = EditorNode::get_singleton()->get_editor_selection();
	undo_redo->add_do_method(selection, "clear");
	undo_redo->add_do_method(selection, "add_node", child);
	undo_redo->add_undo_method(selection, "clear");
	undo_redo->add_undo_method(selection, "add_node", parent);
	undo_redo->add_do_method(EditorNode::get_singleton(), "push_item", child);
	undo_redo->add_undo_method(EditorNode::get_singleton(), "push_item", parent);
	undo_redo->commit_action();

	Dictionary result;
	result["node_path"] = String(scene_root->get_path_to(child));
	result["type"] = type;
	return _success(command, result);
}

Dictionary AIWorkspaceCommandRouter::_set_node_property(const Dictionary &p_args) {
	const String command = "node.set_property";
	if (!p_args.has("node_path") || !p_args.has("property") || !p_args.has("value")) {
		return _error(command, "MISSING_ARGUMENT", "node_path, property, and value are required.");
	}
	if (p_args["node_path"].get_type() != Variant::STRING || p_args["property"].get_type() != Variant::STRING) {
		return _error(command, "INVALID_ARGUMENT_TYPE", "node_path and property must be strings.");
	}

	Node *scene_root = _get_edited_scene_root();
	if (!scene_root) {
		return _error(command, "NO_EDITED_SCENE", "No edited scene is available.");
	}
	const String node_path = p_args["node_path"];
	Node *node = _resolve_scene_node(scene_root, node_path);
	if (!node) {
		return _error(command, "NODE_NOT_FOUND", "The node path does not resolve inside the edited scene.");
	}
	if (!_is_editable_scene_node(node, scene_root, true)) {
		return _error(command, "NODE_NOT_EDITABLE", "The node is internal, protected, or outside the editable scene.");
	}

	const String property_string = p_args["property"];
	const StringName property = property_string;
	Variant value;
	Variant::Type expected_type = Variant::NIL;
	if (!decode_property_value(node, property, p_args["value"], value, expected_type)) {
		return _error(command, "PROPERTY_NOT_ALLOWED", "The property is not allowed for this node type, or the value has an incompatible type.");
	}
	if (!has_property_with_type(node, property, expected_type)) {
		return _error(command, "PROPERTY_NOT_FOUND", "The allowed property is not available with the expected type on this node.");
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (!undo_redo) {
		return _error(command, "EDITOR_UNAVAILABLE", "The editor Undo/Redo manager is unavailable.");
	}
	const Variant previous_value = node->get(property);
	undo_redo->create_action_for_history(vformat(TTR("AI Workspace: Set %s"), property_string), EditorNode::get_editor_data().get_current_edited_scene_history_id());
	undo_redo->add_do_property(node, property, value);
	undo_redo->add_undo_property(node, property, previous_value);
	undo_redo->add_do_method(EditorNode::get_singleton(), "push_item", node, property_string, true);
	undo_redo->add_undo_method(EditorNode::get_singleton(), "push_item", node, property_string, true);
	undo_redo->commit_action();

	Dictionary result;
	result["node_path"] = String(scene_root->get_path_to(node));
	result["property"] = property_string;
	result["value"] = value;
	return _success(command, result);
}

Dictionary AIWorkspaceCommandRouter::_delete_node(const Dictionary &p_args) {
	const String command = "node.delete";
	if (!p_args.has("node_path")) {
		return _error(command, "MISSING_ARGUMENT", "node_path is required.");
	}
	if (p_args["node_path"].get_type() != Variant::STRING) {
		return _error(command, "INVALID_ARGUMENT_TYPE", "node_path must be a string.");
	}

	Node *scene_root = _get_edited_scene_root();
	if (!scene_root) {
		return _error(command, "NO_EDITED_SCENE", "No edited scene is available.");
	}
	const String node_path = p_args["node_path"];
	Node *node = _resolve_scene_node(scene_root, node_path);
	if (!node) {
		return _error(command, "NODE_NOT_FOUND", "The node path does not resolve inside the edited scene.");
	}
	if (node == scene_root) {
		return _error(command, "ROOT_DELETE_FORBIDDEN", "Deleting the edited scene root is not allowed.");
	}
	if (!_is_editable_scene_node(node, scene_root, false) || !node->get_parent()) {
		return _error(command, "NODE_NOT_EDITABLE", "The node is internal, protected, or outside the editable scene.");
	}

	Node *parent = node->get_parent();
	const int child_index = node->get_index(false);
	List<Node *> owned_nodes;
	node->get_owned_by(scene_root, &owned_nodes);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (!undo_redo) {
		return _error(command, "EDITOR_UNAVAILABLE", "The editor Undo/Redo manager is unavailable.");
	}
	undo_redo->create_action_for_history(TTR("AI Workspace: Delete Node"), EditorNode::get_editor_data().get_current_edited_scene_history_id());
	undo_redo->add_do_method(parent, "remove_child", node);
	undo_redo->add_undo_method(parent, "add_child", node, true);
	undo_redo->add_undo_method(parent, "move_child", node, child_index);
	undo_redo->add_undo_method(node, "set_owner", scene_root);
	for (Node *owned_node : owned_nodes) {
		if (owned_node != node) {
			undo_redo->add_undo_method(owned_node, "set_owner", scene_root);
		}
	}
	undo_redo->add_undo_reference(node);
	add_scene_tree_refresh(undo_redo);
	EditorSelection *selection = EditorNode::get_singleton()->get_editor_selection();
	undo_redo->add_do_method(selection, "clear");
	undo_redo->add_do_method(selection, "add_node", parent);
	undo_redo->add_undo_method(selection, "clear");
	undo_redo->add_undo_method(selection, "add_node", node);
	undo_redo->add_do_method(EditorNode::get_singleton(), "push_item", parent);
	undo_redo->add_undo_method(EditorNode::get_singleton(), "push_item", node);
	undo_redo->commit_action();

	Dictionary result;
	result["node_path"] = node_path;
	return _success(command, result);
}

Dictionary AIWorkspaceCommandRouter::_create_scene(const Dictionary &p_args) {
	const String command = "scene.create";
	if (!p_args.has("root_type") || !p_args.has("root_name") || !p_args.has("path")) {
		return _error(command, "MISSING_ARGUMENT", "root_type, root_name, and path are required.");
	}
	if (p_args["root_type"].get_type() != Variant::STRING || p_args["root_name"].get_type() != Variant::STRING || p_args["path"].get_type() != Variant::STRING) {
		return _error(command, "INVALID_ARGUMENT_TYPE", "root_type, root_name, and path must be strings.");
	}

	const String root_type = p_args["root_type"];
	if (!is_allowed_root_type(root_type)) {
		return _error(command, "INVALID_ROOT_TYPE", "Scene roots are limited to Node2D and Node3D.");
	}
	const String root_name = p_args["root_name"];
	if (root_name.is_empty() || root_name.validate_node_name() != root_name) {
		return _error(command, "INVALID_ROOT_NAME", "The scene root name is empty or invalid.");
	}

	const String path = p_args["path"];
	String path_error;
	if (!_validate_project_path(path, "tscn", path_error)) {
		return _error(command, "INVALID_PROJECT_PATH", path_error);
	}
	if (FileAccess::exists(path)) {
		return _error(command, "FILE_EXISTS", "The scene file already exists and will not be overwritten.");
	}
	Ref<DirAccess> parent_dir = DirAccess::open(path.get_base_dir());
	if (parent_dir.is_null()) {
		return _error(command, "DIRECTORY_NOT_FOUND", "The destination directory does not exist.");
	}

	Node *root = instantiate_allowed_root(root_type);
	if (!root) {
		return _error(command, "INVALID_ROOT_TYPE", "The requested scene root is not allowed.");
	}
	root->set_name(root_name);

	Ref<PackedScene> packed_scene;
	packed_scene.instantiate();
	const Error pack_error = packed_scene->pack(root);
	if (pack_error != OK) {
		memdelete(root);
		return _error(command, "SCENE_PACK_FAILED", "The scene root could not be packed.");
	}
	const Error save_error = ResourceSaver::save(packed_scene, path);
	memdelete(root);
	if (save_error != OK || !FileAccess::exists(path)) {
		return _error(command, "SCENE_SAVE_FAILED", "The new scene could not be saved.");
	}

	Dictionary result;
	result["path"] = path;
	result["root_type"] = root_type;
	result["root_name"] = root_name;
	result["opened"] = false;
	return _success(command, result);
}

Dictionary AIWorkspaceCommandRouter::_open_scene(const Dictionary &p_args) {
	const String command = "scene.open";
	if (!p_args.has("path")) {
		return _error(command, "MISSING_ARGUMENT", "path is required.");
	}
	if (p_args["path"].get_type() != Variant::STRING) {
		return _error(command, "INVALID_ARGUMENT_TYPE", "path must be a string.");
	}

	const String path = p_args["path"];
	String path_error;
	if (!_validate_project_path(path, "tscn", path_error)) {
		return _error(command, "INVALID_PROJECT_PATH", path_error);
	}
	if (!FileAccess::exists(path)) {
		return _error(command, "SCENE_NOT_FOUND", "The scene file does not exist.");
	}

	Error load_error = OK;
	Ref<PackedScene> packed_scene = ResourceLoader::load(path, "PackedScene", ResourceFormatLoader::CACHE_MODE_IGNORE, &load_error);
	if (load_error != OK || packed_scene.is_null() || !packed_scene->can_instantiate()) {
		return _error(command, "MALFORMED_SCENE", "The file is not a valid PackedScene.");
	}
	if (!EditorNode::get_singleton()) {
		return _error(command, "EDITOR_UNAVAILABLE", "The editor is unavailable.");
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (undo_redo && undo_redo->is_history_unsaved(EditorNode::get_editor_data().get_current_edited_scene_history_id())) {
		return _error(command, "UNSAVED_CHANGES", "The current scene has unsaved changes; save it before opening another scene.");
	}

	const Error open_error = EditorNode::get_singleton()->open_scene(path);
	if (open_error != OK) {
		return _error(command, "SCENE_OPEN_FAILED", "The editor could not open the scene.");
	}

	Dictionary result;
	result["path"] = path;
	return _success(command, result);
}

Dictionary AIWorkspaceCommandRouter::_save_scene(const Dictionary &p_args) {
	const String command = "scene.save";
	if (!p_args.is_empty()) {
		return _error(command, "INVALID_ARGUMENTS", "scene.save does not accept arguments.");
	}
	Node *scene_root = _get_edited_scene_root();
	if (!scene_root) {
		return _error(command, "NO_EDITED_SCENE", "No edited scene is available.");
	}
	const String path = scene_root->get_scene_file_path();
	if (path.is_empty()) {
		return _error(command, "SCENE_HAS_NO_PATH", "The current scene has no path and will not trigger Save As automatically.");
	}
	String path_error;
	if (!_validate_project_path(path, "tscn", path_error)) {
		return _error(command, "INVALID_PROJECT_PATH", path_error);
	}
	if (!EditorNode::get_singleton()) {
		return _error(command, "EDITOR_UNAVAILABLE", "The editor is unavailable.");
	}

	EditorNode::get_singleton()->save_scene_if_open(path);
	if (!FileAccess::exists(path)) {
		return _error(command, "SCENE_SAVE_FAILED", "The current scene could not be verified on disk after saving.");
	}

	Dictionary result;
	result["path"] = path;
	return _success(command, result);
}

Dictionary AIWorkspaceCommandRouter::_create_resource(const Dictionary &p_args) {
	const String command = "resource.create";
	if (!p_args.has("type") || !p_args.has("path")) {
		return _error(command, "MISSING_ARGUMENT", "type and path are required.");
	}
	if (p_args["type"].get_type() != Variant::STRING || p_args["path"].get_type() != Variant::STRING) {
		return _error(command, "INVALID_ARGUMENT_TYPE", "type and path must be strings.");
	}
	const String type = p_args["type"];
	if (!is_allowed_resource_type(type)) {
		return _error(command, "INVALID_RESOURCE_TYPE", "Allowed resource types are Gradient and GradientTexture2D.");
	}
	const String path = p_args["path"];
	String path_error;
	if (!_validate_project_path(path, "tres", path_error)) {
		return _error(command, "INVALID_PROJECT_PATH", path_error);
	}
	if (FileAccess::exists(path)) {
		return _error(command, "FILE_EXISTS", "The resource file already exists and will not be overwritten.");
	}
	Ref<DirAccess> parent_dir = DirAccess::open(path.get_base_dir());
	if (parent_dir.is_null()) {
		return _error(command, "DIRECTORY_NOT_FOUND", "The destination directory does not exist.");
	}

	Ref<Resource> resource = instantiate_allowed_resource(type);
	if (resource.is_null()) {
		return _error(command, "INVALID_RESOURCE_TYPE", "The requested resource is not allowed.");
	}
	const Error save_error = ResourceSaver::save(resource, path);
	if (save_error != OK || !FileAccess::exists(path)) {
		return _error(command, "RESOURCE_SAVE_FAILED", "The new resource could not be saved.");
	}

	Dictionary result;
	result["path"] = path;
	result["type"] = type;
	return _success(command, result);
}

Dictionary AIWorkspaceCommandRouter::_assign_resource(const Dictionary &p_args) {
	const String command = "resource.assign";
	if (!p_args.has("node_path") || !p_args.has("property") || !p_args.has("resource_path")) {
		return _error(command, "MISSING_ARGUMENT", "node_path, property, and resource_path are required.");
	}
	if (p_args["node_path"].get_type() != Variant::STRING || p_args["property"].get_type() != Variant::STRING || p_args["resource_path"].get_type() != Variant::STRING) {
		return _error(command, "INVALID_ARGUMENT_TYPE", "node_path, property, and resource_path must be strings.");
	}

	Node *scene_root = _get_edited_scene_root();
	if (!scene_root) {
		return _error(command, "NO_EDITED_SCENE", "No edited scene is available.");
	}
	Node *node = _resolve_scene_node(scene_root, p_args["node_path"]);
	if (!node) {
		return _error(command, "NODE_NOT_FOUND", "The node path does not resolve inside the edited scene.");
	}
	if (!_is_editable_scene_node(node, scene_root, true)) {
		return _error(command, "NODE_NOT_EDITABLE", "The node is internal, protected, or outside the editable scene.");
	}
	if (!Object::cast_to<Sprite2D>(node) || String(p_args["property"]) != "texture") {
		return _error(command, "PROPERTY_NOT_ALLOWED", "Only Sprite2D.texture is allowed for resource assignment.");
	}

	const String resource_path = p_args["resource_path"];
	String path_error;
	if (!_validate_project_path(resource_path, String(), path_error) || !is_allowed_resource_extension(resource_path)) {
		return _error(command, "INVALID_PROJECT_PATH", path_error.is_empty() ? "The resource extension is not allowed." : path_error);
	}
	if (!FileAccess::exists(resource_path)) {
		return _error(command, "RESOURCE_NOT_FOUND", "The resource file does not exist.");
	}

	Error load_error = OK;
	Ref<Resource> loaded_resource = ResourceLoader::load(resource_path, "Texture2D", ResourceFormatLoader::CACHE_MODE_IGNORE, &load_error);
	Ref<Texture2D> texture = loaded_resource;
	if (load_error != OK || texture.is_null()) {
		return _error(command, "INCOMPATIBLE_RESOURCE", "The resource is not a compatible Texture2D.");
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (!undo_redo) {
		return _error(command, "EDITOR_UNAVAILABLE", "The editor Undo/Redo manager is unavailable.");
	}
	const Variant previous_value = node->get(SNAME("texture"));
	undo_redo->create_action_for_history(TTR("AI Workspace: Assign Resource"), EditorNode::get_editor_data().get_current_edited_scene_history_id());
	undo_redo->add_do_property(node, SNAME("texture"), texture);
	undo_redo->add_undo_property(node, SNAME("texture"), previous_value);
	undo_redo->add_do_method(EditorNode::get_singleton(), "push_item", node, "texture", true);
	undo_redo->add_undo_method(EditorNode::get_singleton(), "push_item", node, "texture", true);
	undo_redo->commit_action();

	Dictionary result;
	result["node_path"] = String(scene_root->get_path_to(node));
	result["property"] = "texture";
	result["resource_path"] = resource_path;
	return _success(command, result);
}

Dictionary AIWorkspaceCommandRouter::execute(const Dictionary &p_request) {
	if (!p_request.has("command") || p_request["command"].get_type() != Variant::STRING) {
		return _error(String(), "MISSING_COMMAND", "command must be a string.");
	}
	const String command = p_request["command"];
	if (!p_request.has("args") || p_request["args"].get_type() != Variant::DICTIONARY) {
		return _error(command, "INVALID_ARGUMENTS", "args must be an object.");
	}
	const Dictionary args = p_request["args"];
	if (command == "node.create") {
		return _create_node(args);
	}
	if (command == "node.set_property") {
		return _set_node_property(args);
	}
	if (command == "node.delete") {
		return _delete_node(args);
	}
	if (command == "scene.create") {
		return _create_scene(args);
	}
	if (command == "scene.open") {
		return _open_scene(args);
	}
	if (command == "scene.save") {
		return _save_scene(args);
	}
	if (command == "resource.create") {
		return _create_resource(args);
	}
	if (command == "resource.assign") {
		return _assign_resource(args);
	}
	return _error(command, "UNKNOWN_COMMAND", "The requested command is not supported.");
}

Dictionary AIWorkspaceCommandRouter::execute_json(const String &p_json) {
	Ref<JSON> parser;
	parser.instantiate();
	const Error parse_error = parser->parse(p_json);
	if (parse_error != OK) {
		return _error(String(), "INVALID_JSON", vformat("JSON parse error at line %d: %s", parser->get_error_line(), parser->get_error_message()));
	}
	const Variant parsed = parser->get_data();
	if (parsed.get_type() != Variant::DICTIONARY) {
		return _error(String(), "INVALID_REQUEST", "The JSON root must be an object.");
	}
	return execute(parsed);
}
