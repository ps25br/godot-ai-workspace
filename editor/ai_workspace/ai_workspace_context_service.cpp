/**************************************************************************/
/*  ai_workspace_context_service.cpp                                      */
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

#include "ai_workspace_context_service.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/io/resource.h"
#include "core/templates/hash_set.h"
#include "editor/editor_node.h"
#include "scene/main/node.h"

namespace {

bool is_context_value(const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::BOOL:
		case Variant::INT:
		case Variant::FLOAT:
		case Variant::STRING:
		case Variant::STRING_NAME:
		case Variant::VECTOR2:
		case Variant::VECTOR2I:
		case Variant::VECTOR3:
		case Variant::VECTOR3I:
		case Variant::COLOR:
		case Variant::NODE_PATH:
			return true;
		default:
			return false;
	}
}

Variant context_value(const Variant &p_value) {
	if (p_value.get_type() == Variant::STRING_NAME) {
		return String(p_value);
	}
	return p_value;
}

void collect_node_properties(Node *p_node, Dictionary &r_properties, Array &r_resources) {
	List<PropertyInfo> property_list;
	p_node->get_property_list(&property_list);
	HashSet<String> resource_keys;
	int property_count = 0;

	for (const PropertyInfo &property : property_list) {
		if (!(property.usage & PROPERTY_USAGE_EDITOR) || (property.usage & PROPERTY_USAGE_READ_ONLY) || (property.usage & PROPERTY_USAGE_INTERNAL) || property.name.begins_with("_") || property.name == "script") {
			continue;
		}

		Variant value = p_node->get(property.name);
		if (value.get_type() == Variant::OBJECT) {
			Object *object = value.get_validated_object();
			Resource *resource = Object::cast_to<Resource>(object);
			if (resource) {
				const String resource_path = resource->get_path();
				const String resource_key = String(resource->get_class()) + "|" + resource_path;
				if (!resource_keys.has(resource_key)) {
					Dictionary resource_data;
					resource_data["property"] = String(property.name);
					resource_data["type"] = resource->get_class();
					resource_data["path"] = resource_path;
					r_resources.push_back(resource_data);
					resource_keys.insert(resource_key);
				}
			}
			continue;
		}

		if (property_count < 12 && is_context_value(value)) {
			r_properties[String(property.name)] = context_value(value);
			property_count++;
		}
	}
}

Dictionary describe_node(Node *p_node, Node *p_scene_root) {
	Dictionary node_data;
	node_data["name"] = String(p_node->get_name());
	node_data["type"] = p_node->get_class();
	node_data["node_path"] = p_scene_root ? String(p_scene_root->get_path_to(p_node)) : String(p_node->get_path());

	Variant script_variant = p_node->get_script();
	Resource *script = Object::cast_to<Resource>(script_variant.get_validated_object());
	node_data["script"] = script ? script->get_path() : String();

	Dictionary properties;
	Array resources;
	collect_node_properties(p_node, properties, resources);
	node_data["properties"] = properties;
	node_data["resources"] = resources;
	return node_data;
}

} // namespace

String AIWorkspaceContextService::get_context_json() const {
	return JSON::stringify(context, "\t");
}

void AIWorkspaceContextService::refresh() {
	Dictionary new_context;

	Dictionary project;
	project["name"] = String(GLOBAL_GET("application/config/name"));
	project["path"] = "res://";
	new_context["project"] = project;

	Node *scene_root = nullptr;
	if (EditorNode::get_singleton()) {
		EditorData &editor_data = EditorNode::get_editor_data();
		const int edited_scene = editor_data.get_edited_scene();
		if (edited_scene >= 0 && edited_scene < editor_data.get_edited_scene_count()) {
			scene_root = editor_data.get_edited_scene_root(edited_scene);
		}
	}
	Dictionary scene;
	scene["path"] = scene_root ? scene_root->get_scene_file_path() : String();
	scene["root"] = scene_root ? String(scene_root->get_name()) : String();
	new_context["scene"] = scene;

	Array selection;
	int resource_count = 0;
	if (EditorNode::get_singleton() && EditorNode::get_singleton()->get_editor_selection()) {
		const HashMap<ObjectID, Object *> &selected_objects = EditorNode::get_singleton()->get_editor_selection()->get_selection();
		for (const KeyValue<ObjectID, Object *> &entry : selected_objects) {
			Node *node = ObjectDB::get_instance<Node>(entry.key);
			if (!node || (scene_root && !scene_root->is_ancestor_of(node) && node != scene_root)) {
				continue;
			}
			Dictionary node_data = describe_node(node, scene_root);
			Array resources = node_data["resources"];
			resource_count += resources.size();
			selection.push_back(node_data);
		}
	}
	new_context["selection"] = selection;
	new_context["resource_count"] = resource_count;

	context = new_context;
}

AIWorkspaceContextService::AIWorkspaceContextService() {
	refresh();
}
