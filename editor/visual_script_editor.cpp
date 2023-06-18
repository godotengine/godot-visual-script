/*************************************************************************/
/*  visual_script_editor.cpp                                             */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "visual_script_editor.h"

#include "../visual_script_expression.h"
#include "../visual_script_flow_control.h"
#include "../visual_script_func_nodes.h"
#include "../visual_script_nodes.h"
#include "core/input/input.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "core/os/keyboard.h"
#include "core/variant/variant.h"
#include "editor/editor_node.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/inspector_dock.h"
#include "scene/gui/check_button.h"
#include "scene/gui/graph_edit.h"
#include "scene/gui/separator.h"
#include "scene/gui/view_panner.h"
#include "scene/main/window.h"

#ifdef TOOLS_ENABLED

void VisualScriptEditedProperty::_bind_methods() {
  ClassDB::bind_method(D_METHOD("set_edited_property", "value"),
                       &VisualScriptEditedProperty::set_edited_property);
  ClassDB::bind_method(D_METHOD("get_edited_property"),
                       &VisualScriptEditedProperty::get_edited_property);

  ADD_PROPERTY(PropertyInfo(Variant::NIL, "edited_property", PROPERTY_HINT_NONE,
                            "", PROPERTY_USAGE_NIL_IS_VARIANT),
               "set_edited_property", "get_edited_property");
}

void VisualScriptEditedProperty::set_edited_property(Variant p_variant) {
  edited_property = p_variant;
}

Variant VisualScriptEditedProperty::get_edited_property() const {
  return edited_property;
}

/////////////////

class VisualScriptEditorSignalEdit : public Object {
  GDCLASS(VisualScriptEditorSignalEdit, Object);

  StringName sig;

public:
  Ref<EditorUndoRedoManager> undo_redo;
  Ref<VisualScript> script;

protected:
  static void _bind_methods() {
    ClassDB::bind_method("_sig_changed",
                         &VisualScriptEditorSignalEdit::_sig_changed);
    ADD_SIGNAL(MethodInfo("changed"));
  }

  void _sig_changed() {
    notify_property_list_changed();
    emit_signal(SNAME("changed"));
  }

  bool _set(const StringName &p_name, const Variant &p_value) {
    if (sig == StringName()) {
      return false;
    }

    if (p_name == "argument_count") {
      int new_argc = p_value;
      int argc = script->custom_signal_get_argument_count(sig);
      if (argc == new_argc) {
        return true;
      }

      undo_redo->create_action(TTR("Change Signal Arguments"));

      if (new_argc < argc) {
        for (int i = new_argc; i < argc; i++) {
          undo_redo->add_do_method(
              script.ptr(), "custom_signal_remove_argument", sig, new_argc);
          undo_redo->add_undo_method(
              script.ptr(), "custom_signal_add_argument", sig,
              script->custom_signal_get_argument_name(sig, i),
              script->custom_signal_get_argument_type(sig, i), -1);
        }
      } else if (new_argc > argc) {
        for (int i = argc; i < new_argc; i++) {
          undo_redo->add_do_method(script.ptr(), "custom_signal_add_argument",
                                   sig, Variant::NIL, "arg" + itos(i + 1), -1);
          undo_redo->add_undo_method(
              script.ptr(), "custom_signal_remove_argument", sig, argc);
        }
      }

      undo_redo->add_do_method(this, "_sig_changed");
      undo_redo->add_undo_method(this, "_sig_changed");

      undo_redo->commit_action();

      return true;
    }
    if (String(p_name).begins_with("argument/")) {
      int idx = String(p_name).get_slice("/", 1).to_int() - 1;
      ERR_FAIL_INDEX_V(idx, script->custom_signal_get_argument_count(sig),
                       false);
      String what = String(p_name).get_slice("/", 2);
      if (what == "type") {
        int old_type = script->custom_signal_get_argument_type(sig, idx);
        int new_type = p_value;
        undo_redo->create_action(TTR("Change Argument Type"));
        undo_redo->add_do_method(script.ptr(),
                                 "custom_signal_set_argument_type", sig, idx,
                                 new_type);
        undo_redo->add_undo_method(script.ptr(),
                                   "custom_signal_set_argument_type", sig, idx,
                                   old_type);
        undo_redo->commit_action();

        return true;
      }

      if (what == "name") {
        String old_name = script->custom_signal_get_argument_name(sig, idx);
        String new_name = p_value;
        undo_redo->create_action(TTR("Change Argument name"));
        undo_redo->add_do_method(script.ptr(),
                                 "custom_signal_set_argument_name", sig, idx,
                                 new_name);
        undo_redo->add_undo_method(script.ptr(),
                                   "custom_signal_set_argument_name", sig, idx,
                                   old_name);
        undo_redo->commit_action();
        return true;
      }
    }

    return false;
  }

  bool _get(const StringName &p_name, Variant &r_ret) const {
    if (sig == StringName()) {
      return false;
    }

    if (p_name == "argument_count") {
      r_ret = script->custom_signal_get_argument_count(sig);
      return true;
    }
    if (String(p_name).begins_with("argument/")) {
      int idx = String(p_name).get_slice("/", 1).to_int() - 1;
      ERR_FAIL_INDEX_V(idx, script->custom_signal_get_argument_count(sig),
                       false);
      String what = String(p_name).get_slice("/", 2);
      if (what == "type") {
        r_ret = script->custom_signal_get_argument_type(sig, idx);
        return true;
      }
      if (what == "name") {
        r_ret = script->custom_signal_get_argument_name(sig, idx);
        return true;
      }
    }

    return false;
  }
  void _get_property_list(List<PropertyInfo> *p_list) const {
    if (sig == StringName()) {
      return;
    }

    p_list->push_back(PropertyInfo(Variant::INT, "argument_count",
                                   PROPERTY_HINT_RANGE, "0,256"));
    String argt = "Variant";
    for (int i = 1; i < Variant::VARIANT_MAX; i++) {
      argt += "," + Variant::get_type_name(Variant::Type(i));
    }

    for (int i = 0; i < script->custom_signal_get_argument_count(sig); i++) {
      p_list->push_back(PropertyInfo(Variant::INT,
                                     "argument/" + itos(i + 1) + "/type",
                                     PROPERTY_HINT_ENUM, argt));
      p_list->push_back(
          PropertyInfo(Variant::STRING, "argument/" + itos(i + 1) + "/name"));
    }
  }

public:
  void edit(const StringName &p_sig) {
    sig = p_sig;
    notify_property_list_changed();
  }
};

class VisualScriptEditorVariableEdit : public Object {
  GDCLASS(VisualScriptEditorVariableEdit, Object);

  StringName var;

public:
  Ref<EditorUndoRedoManager> undo_redo;
  Ref<VisualScript> script;

protected:
  static void _bind_methods() {
    ClassDB::bind_method("_var_changed",
                         &VisualScriptEditorVariableEdit::_var_changed);
    ClassDB::bind_method("_var_value_changed",
                         &VisualScriptEditorVariableEdit::_var_value_changed);
    ADD_SIGNAL(MethodInfo("changed"));
  }

  void _var_changed() {
    notify_property_list_changed();
    emit_signal(SNAME("changed"));
  }
  void _var_value_changed() { emit_signal(SNAME("changed")); }

  bool _set(const StringName &p_name, const Variant &p_value) {
    if (var == StringName()) {
      return false;
    }

    if (String(p_name) == "value") {
      undo_redo->create_action(TTR("Set Variable Default Value"));
      Variant current = script->get_variable_default_value(var);
      undo_redo->add_do_method(script.ptr(), "set_variable_default_value", var,
                               p_value);
      undo_redo->add_undo_method(script.ptr(), "set_variable_default_value",
                                 var, current);
      undo_redo->add_do_method(this, "_var_value_changed");
      undo_redo->add_undo_method(this, "_var_value_changed");
      undo_redo->commit_action();
      return true;
    }

    Dictionary d = script->call("get_variable_info", var);

    if (String(p_name) == "type") {
      Dictionary dc = d.duplicate();
      dc["type"] = p_value;
      undo_redo->create_action(TTR("Set Variable Type"));
      undo_redo->add_do_method(script.ptr(), "set_variable_info", var, dc);
      undo_redo->add_undo_method(script.ptr(), "set_variable_info", var, d);

      // Setting the default value.
      Variant::Type type = (Variant::Type)(int)p_value;
      if (type != Variant::NIL) {
        Variant default_value;
        Callable::CallError ce;
        Variant::construct(type, default_value, nullptr, 0, ce);
        if (ce.error == Callable::CallError::CALL_OK) {
          undo_redo->add_do_method(script.ptr(), "set_variable_default_value",
                                   var, default_value);
          undo_redo->add_undo_method(script.ptr(), "set_variable_default_value",
                                     var, dc["value"]);
        }
      }

      undo_redo->add_do_method(this, "_var_changed");
      undo_redo->add_undo_method(this, "_var_changed");
      undo_redo->commit_action();
      return true;
    }

    if (String(p_name) == "hint") {
      Dictionary dc = d.duplicate();
      dc["hint"] = p_value;
      undo_redo->create_action(TTR("Set Variable Type"));
      undo_redo->add_do_method(script.ptr(), "set_variable_info", var, dc);
      undo_redo->add_undo_method(script.ptr(), "set_variable_info", var, d);
      undo_redo->add_do_method(this, "_var_changed");
      undo_redo->add_undo_method(this, "_var_changed");
      undo_redo->commit_action();
      return true;
    }

    if (String(p_name) == "hint_string") {
      Dictionary dc = d.duplicate();
      dc["hint_string"] = p_value;
      undo_redo->create_action(TTR("Set Variable Type"));
      undo_redo->add_do_method(script.ptr(), "set_variable_info", var, dc);
      undo_redo->add_undo_method(script.ptr(), "set_variable_info", var, d);
      undo_redo->add_do_method(this, "_var_changed");
      undo_redo->add_undo_method(this, "_var_changed");
      undo_redo->commit_action();
      return true;
    }

    if (String(p_name) == "export") {
      script->set_variable_export(var, p_value);
      InspectorDock::get_inspector_singleton()->update_tree();
      return true;
    }

    return false;
  }

  bool _get(const StringName &p_name, Variant &r_ret) const {
    if (var == StringName()) {
      return false;
    }

    if (String(p_name) == "value") {
      r_ret = script->get_variable_default_value(var);
      return true;
    }

    PropertyInfo pinfo = script->get_variable_info(var);

    if (String(p_name) == "type") {
      r_ret = pinfo.type;
      return true;
    }
    if (String(p_name) == "hint") {
      r_ret = pinfo.hint;
      return true;
    }
    if (String(p_name) == "hint_string") {
      r_ret = pinfo.hint_string;
      return true;
    }

    if (String(p_name) == "export") {
      r_ret = script->get_variable_export(var);
      return true;
    }

    return false;
  }
  void _get_property_list(List<PropertyInfo> *p_list) const {
    if (var == StringName()) {
      return;
    }

    String argt = "Variant";
    for (int i = 1; i < Variant::VARIANT_MAX; i++) {
      argt += "," + Variant::get_type_name(Variant::Type(i));
    }
    p_list->push_back(
        PropertyInfo(Variant::INT, "type", PROPERTY_HINT_ENUM, argt));
    p_list->push_back(PropertyInfo(script->get_variable_info(var).type, "value",
                                   script->get_variable_info(var).hint,
                                   script->get_variable_info(var).hint_string,
                                   PROPERTY_USAGE_DEFAULT));
    // Update this when PropertyHint changes.
    p_list->push_back(PropertyInfo(
        Variant::INT, "hint", PROPERTY_HINT_ENUM,
        "None,Range,ExpRange,Enum,ExpEasing,Length,SpriteFrame,KeyAccel,Flags,"
        "Layers2dRender,Layers2dPhysics,Layer3dRender,Layer3dPhysics,File,Dir,"
        "GlobalFile,GlobalDir,ResourceType,MultilineText,PlaceholderText,"
        "ColorNoAlpha,ImageCompressLossy,ImageCompressLossLess,ObjectId,String,"
        "NodePathToEditedNode,MethodOfVariantType,MethodOfBaseType,"
        "MethodOfInstance,MethodOfScript,PropertyOfVariantType,"
        "PropertyOfBaseType,PropertyOfInstance,PropertyOfScript,ObjectTooBig,"
        "NodePathValidTypes"));
    p_list->push_back(PropertyInfo(Variant::STRING, "hint_string"));
    p_list->push_back(PropertyInfo(Variant::BOOL, "export"));
  }

public:
  void edit(const StringName &p_var) {
    var = p_var;
    notify_property_list_changed();
  }
};

static Color _color_from_type(Variant::Type p_type, bool dark_theme = true) {
  Color color;
  if (dark_theme) {
    switch (p_type) {
    case Variant::NIL:
      color = Color(0.41, 0.93, 0.74);
      break;

    case Variant::BOOL:
      color = Color(0.55, 0.65, 0.94);
      break;
    case Variant::INT:
      color = Color(0.49, 0.78, 0.94);
      break;
    case Variant::FLOAT:
      color = Color(0.38, 0.85, 0.96);
      break;
    case Variant::STRING:
      color = Color(0.42, 0.65, 0.93);
      break;

    case Variant::VECTOR2:
      color = Color(0.74, 0.57, 0.95);
      break;
    case Variant::VECTOR2I:
      color = Color(0.74, 0.57, 0.95);
      break;
    case Variant::RECT2:
      color = Color(0.95, 0.57, 0.65);
      break;
    case Variant::RECT2I:
      color = Color(0.95, 0.57, 0.65);
      break;
    case Variant::VECTOR3:
      color = Color(0.84, 0.49, 0.93);
      break;
    case Variant::VECTOR3I:
      color = Color(0.84, 0.49, 0.93);
      break;
    case Variant::VECTOR4:
      color = Color(0.84, 0.49, 0.94);
      break;
    case Variant::VECTOR4I:
      color = Color(0.84, 0.49, 0.94);
      break;
    case Variant::TRANSFORM2D:
      color = Color(0.77, 0.93, 0.41);
      break;
    case Variant::PLANE:
      color = Color(0.97, 0.44, 0.44);
      break;
    case Variant::QUATERNION:
      color = Color(0.93, 0.41, 0.64);
      break;
    case Variant::AABB:
      color = Color(0.93, 0.47, 0.57);
      break;
    case Variant::BASIS:
      color = Color(0.89, 0.93, 0.41);
      break;
    case Variant::TRANSFORM3D:
      color = Color(0.96, 0.66, 0.43);
      break;

    case Variant::COLOR:
      color = Color(0.62, 1.0, 0.44);
      break;
    case Variant::NODE_PATH:
      color = Color(0.41, 0.58, 0.93);
      break;
    case Variant::RID:
      color = Color(0.41, 0.93, 0.6);
      break;
    case Variant::OBJECT:
      color = Color(0.47, 0.95, 0.91);
      break;
    case Variant::DICTIONARY:
      color = Color(0.47, 0.93, 0.69);
      break;

    case Variant::ARRAY:
      color = Color(0.88, 0.88, 0.88);
      break;
    case Variant::PACKED_BYTE_ARRAY:
      color = Color(0.67, 0.96, 0.78);
      break;
    case Variant::PACKED_INT32_ARRAY:
      color = Color(0.69, 0.86, 0.96);
      break;
    case Variant::PACKED_FLOAT32_ARRAY:
      color = Color(0.59, 0.91, 0.97);
      break;
    case Variant::PACKED_INT64_ARRAY:
      color = Color(0.69, 0.86, 0.96);
      break;
    case Variant::PACKED_FLOAT64_ARRAY:
      color = Color(0.59, 0.91, 0.97);
      break;
    case Variant::PACKED_STRING_ARRAY:
      color = Color(0.62, 0.77, 0.95);
      break;
    case Variant::PACKED_VECTOR2_ARRAY:
      color = Color(0.82, 0.7, 0.96);
      break;
    case Variant::PACKED_VECTOR3_ARRAY:
      color = Color(0.87, 0.61, 0.95);
      break;
    case Variant::PACKED_COLOR_ARRAY:
      color = Color(0.91, 1.0, 0.59);
      break;

    default:
      color.set_hsv(p_type / float(Variant::VARIANT_MAX), 0.7, 0.7);
    }
  } else {
    switch (p_type) {
    case Variant::NIL:
      color = Color(0.15, 0.89, 0.63);
      break;

    case Variant::BOOL:
      color = Color(0.43, 0.56, 0.92);
      break;
    case Variant::INT:
      color = Color(0.31, 0.7, 0.91);
      break;
    case Variant::FLOAT:
      color = Color(0.15, 0.8, 0.94);
      break;
    case Variant::STRING:
      color = Color(0.27, 0.56, 0.91);
      break;

    case Variant::VECTOR2:
      color = Color(0.68, 0.46, 0.93);
      break;
    case Variant::VECTOR2I:
      color = Color(0.68, 0.46, 0.93);
      break;
    case Variant::RECT2:
      color = Color(0.93, 0.46, 0.56);
      break;
    case Variant::RECT2I:
      color = Color(0.93, 0.46, 0.56);
      break;
    case Variant::VECTOR3:
      color = Color(0.86, 0.42, 0.93);
      break;
    case Variant::VECTOR3I:
      color = Color(0.86, 0.42, 0.93);
      break;
    case Variant::TRANSFORM2D:
      color = Color(0.59, 0.81, 0.1);
      break;
    case Variant::PLANE:
      color = Color(0.97, 0.44, 0.44);
      break;
    case Variant::QUATERNION:
      color = Color(0.93, 0.41, 0.64);
      break;
    case Variant::AABB:
      color = Color(0.93, 0.47, 0.57);
      break;
    case Variant::BASIS:
      color = Color(0.7, 0.73, 0.1);
      break;
    case Variant::TRANSFORM3D:
      color = Color(0.96, 0.56, 0.28);
      break;

    case Variant::COLOR:
      color = Color(0.24, 0.75, 0.0);
      break;
    case Variant::NODE_PATH:
      color = Color(0.41, 0.58, 0.93);
      break;
    case Variant::RID:
      color = Color(0.17, 0.9, 0.45);
      break;
    case Variant::OBJECT:
      color = Color(0.07, 0.84, 0.76);
      break;
    case Variant::DICTIONARY:
      color = Color(0.34, 0.91, 0.62);
      break;

    case Variant::ARRAY:
      color = Color(0.45, 0.45, 0.45);
      break;
    case Variant::PACKED_BYTE_ARRAY:
      color = Color(0.38, 0.92, 0.6);
      break;
    case Variant::PACKED_INT32_ARRAY:
      color = Color(0.38, 0.73, 0.92);
      break;
    case Variant::PACKED_FLOAT32_ARRAY:
      color = Color(0.25, 0.83, 0.95);
      break;
    case Variant::PACKED_INT64_ARRAY:
      color = Color(0.38, 0.73, 0.92);
      break;
    case Variant::PACKED_FLOAT64_ARRAY:
      color = Color(0.25, 0.83, 0.95);
      break;
    case Variant::PACKED_STRING_ARRAY:
      color = Color(0.38, 0.62, 0.92);
      break;
    case Variant::PACKED_VECTOR2_ARRAY:
      color = Color(0.62, 0.36, 0.92);
      break;
    case Variant::PACKED_VECTOR3_ARRAY:
      color = Color(0.79, 0.35, 0.92);
      break;
    case Variant::PACKED_COLOR_ARRAY:
      color = Color(0.57, 0.73, 0.0);
      break;

    default:
      color.set_hsv(p_type / float(Variant::VARIANT_MAX), 0.3, 0.3);
    }
  }

  return color;
}

void VisualScriptEditor::_update_graph_connections() {
  graph->clear_connections();

  List<VisualScript::SequenceConnection> sequence_conns;
  script->get_sequence_connection_list(&sequence_conns);

  for (const VisualScript::SequenceConnection &E : sequence_conns) {
    graph->connect_node(itos(E.from_node), E.from_output, itos(E.to_node), 0);
  }

  List<VisualScript::DataConnection> data_conns;
  script->get_data_connection_list(&data_conns);

  for (VisualScript::DataConnection &dc : data_conns) {
    Ref<VisualScriptNode> from_node = script->get_node(dc.from_node);
    Ref<VisualScriptNode> to_node = script->get_node(dc.to_node);

    if (to_node->has_input_sequence_port()) {
      dc.to_port++;
    }

    dc.from_port += from_node->get_output_sequence_port_count();

    graph->connect_node(itos(dc.from_node), dc.from_port, itos(dc.to_node),
                        dc.to_port);
  }
}

void VisualScriptEditor::_update_graph(int p_only_id) {
  if (updating_graph) {
    return;
  }

  updating_graph = true;

  // byebye all nodes
  if (p_only_id >= 0) {
    if (graph->has_node(itos(p_only_id))) {
      Node *gid = graph->get_node(itos(p_only_id));
      if (gid) {
        memdelete(gid);
      }
    }
  } else {
    for (int i = 0; i < graph->get_child_count(); i++) {
      if (Object::cast_to<GraphNode>(graph->get_child(i))) {
        memdelete(graph->get_child(i));
        i--;
      }
    }
  }
  graph->show();
  select_func_text->hide();

  Ref<Texture2D> type_icons[Variant::VARIANT_MAX] = {
      Control::get_theme_icon(SNAME("Variant"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("bool"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("int"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("float"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("String"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Vector2"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Vector2i"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Rect2"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Rect2i"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Vector3"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Vector3i"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Transform2D"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Plane"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Quaternion"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("AABB"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Basis"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Transform3D"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Color"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("StringName"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("NodePath"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("RID"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("MiniObject"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Callable"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Signal"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Dictionary"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Array"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedByteArray"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedInt32Array"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedInt64Array"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedFloat32Array"),
                              SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedFloat64Array"),
                              SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedStringArray"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedVector2Array"),
                              SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedVector3Array"),
                              SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedColorArray"), SNAME("EditorIcons"))};

  // Visual script specific theme for MSDF font.
  Ref<Theme> vstheme;
  vstheme.instantiate();
  Ref<Font> label_font =
      EditorNode::get_singleton()->get_editor_theme()->get_font("main_msdf",
                                                                "EditorFonts");
  vstheme->set_font("font", "Label", label_font);
  vstheme->set_font("font", "LineEdit", label_font);
  vstheme->set_font("font", "Button", label_font);

  Ref<Texture2D> seq_port =
      Control::get_theme_icon(SNAME("VisualShaderPort"), SNAME("EditorIcons"));
  List<int> node_ids;
  script->get_node_list(&node_ids);

  List<int> ids;
  script->get_node_list(&ids);

  for (int &E : ids) {
    if (p_only_id >= 0 && p_only_id != E) {
      continue;
    }

    Ref<VisualScriptNode> node = script->get_node(E);
    Vector2 pos = script->get_node_position(E);

    GraphNode *gnode = memnew(GraphNode);
    gnode->set_title(node->get_caption());
    gnode->set_position_offset(pos * EDSCALE);
    if (error_line == E) {
      gnode->set_overlay(GraphNode::OVERLAY_POSITION);
    } else if (node->is_breakpoint()) {
      gnode->set_overlay(GraphNode::OVERLAY_BREAKPOINT);
    }

    gnode->set_meta("__vnode", node);
    gnode->set_name(itos(E));
    gnode->connect("dragged",
                   callable_mp(this, &VisualScriptEditor::_node_moved).bind(E));
    gnode->connect("close_request",
                   callable_mp(this, &VisualScriptEditor::_remove_node).bind(E),
                   CONNECT_DEFERRED);

    {
      Ref<VisualScriptFunction> v = node;
      if (!v.is_valid()) {
        gnode->set_show_close_button(true);
      }
    }

    bool has_gnode_text = false;

    Ref<VisualScriptLists> nd_list = node;
    bool is_vslist = nd_list.is_valid();
    if (is_vslist) {
      HBoxContainer *hbnc = memnew(HBoxContainer);
      if (nd_list->is_input_port_editable()) {
        has_gnode_text = true;
        Button *btn = memnew(Button);
        btn->set_text(TTR("Add Input Port"));
        hbnc->add_child(btn);
        btn->connect(
            "pressed",
            callable_mp(this, &VisualScriptEditor::_add_input_port).bind(E),
            CONNECT_DEFERRED);
      }
      if (nd_list->is_output_port_editable()) {
        if (nd_list->is_input_port_editable()) {
          hbnc->add_spacer();
        }
        has_gnode_text = true;
        Button *btn = memnew(Button);
        btn->set_text(TTR("Add Output Port"));
        hbnc->add_child(btn);
        btn->connect(
            "pressed",
            callable_mp(this, &VisualScriptEditor::_add_output_port).bind(E),
            CONNECT_DEFERRED);
      }
      gnode->add_child(hbnc);
    } else if (Object::cast_to<VisualScriptExpression>(node.ptr())) {
      has_gnode_text = true;
      LineEdit *line_edit = memnew(LineEdit);
      line_edit->set_text(node->get_text());
      line_edit->set_expand_to_text_length_enabled(true);
      line_edit->add_theme_font_override(
          "font", get_theme_font(SNAME("source"), SNAME("EditorFonts")));
      gnode->add_child(line_edit);
      line_edit->connect(
          "text_changed",
          callable_mp(this, &VisualScriptEditor::_expression_text_changed)
              .bind(E));
    } else {
      String text = node->get_text();
      if (!text.is_empty()) {
        has_gnode_text = true;
        Label *label = memnew(Label);
        label->set_text(text);
        gnode->add_child(label);
      }
    }

    if (Object::cast_to<VisualScriptComment>(node.ptr())) {
      Ref<VisualScriptComment> vsc = node;
      gnode->set_comment(true);
      gnode->set_resizable(true);
      gnode->set_custom_minimum_size(vsc->get_size() * EDSCALE);
      gnode->connect(
          "resize_request",
          callable_mp(this, &VisualScriptEditor::_comment_node_resized)
              .bind(E));
    }

    if (node_styles.has(node->get_category())) {
      Ref<StyleBoxFlat> sbf = node_styles[node->get_category()];
      if (gnode->is_comment()) {
        sbf = EditorNode::get_singleton()
                  ->get_theme_base()
                  ->get_theme()
                  ->get_stylebox(SNAME("comment"), SNAME("GraphNode"));
      }

      Color c = sbf->get_border_color();
      c = ((c.r + c.g + c.b) / 3) < 0.7 ? Color(1.0, 1.0, 1.0, 0.85)
                                        : Color(0.0, 0.0, 0.0, 0.85);
      Color ic = c;
      gnode->add_theme_color_override("title_color", c);
      c.a = 1;
      gnode->add_theme_color_override("close_color", c);
      gnode->add_theme_color_override("resizer_color", ic);
      gnode->add_theme_style_override("frame", sbf);
    }

    const Color mono_color =
        get_theme_color(SNAME("mono_color"), SNAME("Editor"));

    int slot_idx = 0;

    bool single_seq_output = node->get_output_sequence_port_count() == 1 &&
                             node->get_output_sequence_port_text(0) == String();
    if ((node->has_input_sequence_port() || single_seq_output) ||
        has_gnode_text) {
      // IF has_gnode_text is true BUT we have no sequence ports to draw (in
      // here), we still draw the disabled default ones to shift up the slots by
      // one, so the slots DON'T start with the content text.

      // IF has_gnode_text is false, but we DO want to draw default sequence
      // ports, we draw a dummy text to take up the position of the sequence
      // nodes, so all the other ports are still aligned correctly.
      if (!has_gnode_text) {
        Label *dummy = memnew(Label);
        dummy->set_text(" ");
        gnode->add_child(dummy);
      }
      gnode->set_slot(0, node->has_input_sequence_port(), TYPE_SEQUENCE,
                      mono_color, single_seq_output, TYPE_SEQUENCE, mono_color,
                      seq_port, seq_port);
      slot_idx++;
    }

    int mixed_seq_ports = 0;

    if (!single_seq_output) {
      if (node->has_mixed_input_and_sequence_ports()) {
        mixed_seq_ports = node->get_output_sequence_port_count();
      } else {
        for (int i = 0; i < node->get_output_sequence_port_count(); i++) {
          Label *text2 = memnew(Label);
          text2->set_text(node->get_output_sequence_port_text(i));
          text2->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
          gnode->add_child(text2);
          gnode->set_slot(slot_idx, false, 0, Color(), true, TYPE_SEQUENCE,
                          mono_color, seq_port, seq_port);
          slot_idx++;
        }
      }
    }

    for (int i = 0;
         i < MAX(node->get_output_value_port_count(),
                 MAX(mixed_seq_ports, node->get_input_value_port_count()));
         i++) {
      bool left_ok = false;
      Variant::Type left_type = Variant::NIL;
      String left_name;

      if (i < node->get_input_value_port_count()) {
        PropertyInfo pi = node->get_input_value_port_info(i);
        left_ok = true;
        left_type = pi.type;
        left_name = pi.name;
      }

      bool right_ok = false;
      Variant::Type right_type = Variant::NIL;
      String right_name;

      if (i >= mixed_seq_ports &&
          i < node->get_output_value_port_count() + mixed_seq_ports) {
        PropertyInfo pi = node->get_output_value_port_info(i - mixed_seq_ports);
        right_ok = true;
        right_type = pi.type;
        right_name = pi.name;
      }
      VBoxContainer *vbc = memnew(VBoxContainer);
      HBoxContainer *hbc = memnew(HBoxContainer);
      HBoxContainer *hbc2 = memnew(HBoxContainer);
      vbc->add_child(hbc);
      vbc->add_child(hbc2);
      if (left_ok) {
        Ref<Texture2D> t;
        if (left_type >= 0 && left_type < Variant::VARIANT_MAX) {
          t = type_icons[left_type];
        }
        if (t.is_valid()) {
          TextureRect *tf = memnew(TextureRect);
          tf->set_texture(t);
          tf->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
          hbc->add_child(tf);
        }

        if (is_vslist) {
          if (nd_list->is_input_port_name_editable()) {
            LineEdit *name_box = memnew(LineEdit);
            hbc->add_child(name_box);
            name_box->set_custom_minimum_size(Size2(60 * EDSCALE, 0));
            name_box->set_text(left_name);
            name_box->set_expand_to_text_length_enabled(true);
            name_box->connect(
                "resized",
                callable_mp(this, &VisualScriptEditor::_update_node_size)
                    .bind(E));
            name_box->connect(
                "focus_exited",
                callable_mp(this, &VisualScriptEditor::_port_name_focus_out)
                    .bind(name_box, E, i, true));
          } else {
            hbc->add_child(memnew(Label(left_name)));
          }

          if (nd_list->is_input_port_type_editable()) {
            OptionButton *opbtn = memnew(OptionButton);
            for (int j = Variant::NIL; j < Variant::VARIANT_MAX; j++) {
              opbtn->add_item(Variant::get_type_name(Variant::Type(j)));
            }
            opbtn->select(left_type);
            opbtn->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
            hbc->add_child(opbtn);
            opbtn->connect(
                "item_selected",
                callable_mp(this, &VisualScriptEditor::_change_port_type)
                    .bind(E, i, true),
                CONNECT_DEFERRED);
          }

          Button *rmbtn = memnew(Button);
          rmbtn->set_icon(
              EditorNode::get_singleton()->get_gui_base()->get_theme_icon(
                  SNAME("Remove"), SNAME("EditorIcons")));
          hbc->add_child(rmbtn);
          rmbtn->connect(
              "pressed",
              callable_mp(this, &VisualScriptEditor::_remove_input_port)
                  .bind(E, i),
              CONNECT_DEFERRED);
        } else {
          hbc->add_child(memnew(Label(left_name)));
        }

        if (left_type != Variant::NIL &&
            !script->is_input_value_port_connected(E, i)) {
          PropertyInfo pi = node->get_input_value_port_info(i);
          Button *button = memnew(Button);
          Variant value = node->get_default_input_value(i);
          if (value.get_type() != left_type) {
            // different type? for now convert
            // not the same, reconvert
            Callable::CallError ce;
            const Variant *existingp = &value;
            Variant::construct(left_type, value, &existingp, 1, ce);
          }

          if (left_type == Variant::COLOR) {
            button->set_custom_minimum_size(Size2(30, 0) * EDSCALE);
            button->connect(
                "draw",
                callable_mp(this, &VisualScriptEditor::_draw_color_over_button)
                    .bind(button, value));
          } else if (left_type == Variant::OBJECT &&
                     Ref<Resource>(value).is_valid()) {
            Ref<Resource> res = value;
            Array arr;
            arr.push_back(button->get_instance_id());
            arr.push_back(String(value));
            EditorResourcePreview::get_singleton()
                ->queue_edited_resource_preview(
                    res, this, "_button_resource_previewed", arr);

          } else if (pi.type == Variant::INT && pi.hint == PROPERTY_HINT_ENUM) {
            bool found = false;
            const Vector<String> options = pi.hint_string.split(",");
            int64_t current_val = 0;
            for (const String &option : options) {
              Vector<String> text_split = option.split(":");
              if (text_split.size() != 1) {
                current_val = text_split[1].to_int();
              }
              if (value.operator int() == current_val) {
                button->set_text(text_split[0]);
                found = true;
                break;
              }
              current_val += 1;
            }
            if (!found) {
              button->set_text(value);
            }
          } else if (pi.type == Variant::INT &&
                     pi.hint == PROPERTY_HINT_FLAGS) {
            Vector<String> value_texts;
            const Vector<String> options = pi.hint_string.split(",");
            uint32_t v = value;
            for (const String &option : options) {
              uint32_t current_val;
              Vector<String> text_split = option.split(":");
              if (text_split.size() != -1) {
                current_val = text_split[1].to_int();
              } else {
                current_val = 1 << i;
              }
              if ((v & current_val) == current_val) {
                value_texts.push_back(text_split[0]);
              }
            }
            if (value_texts.size() != 0) {
              String value_text = value_texts[0];
              for (const String &text : value_texts) {
                value_text += " | " + text;
              }
              button->set_text(value_text);
            } else {
              button->set_text(value);
            }
          } else {
            button->set_text(value);
          }
          button->connect(
              "pressed",
              callable_mp(this, &VisualScriptEditor::_default_value_edited)
                  .bind(button, E, i));
          hbc2->add_child(button);
        }
      } else {
        Control *c = memnew(Control);
        c->set_custom_minimum_size(Size2(10, 0) * EDSCALE);
        hbc->add_child(c);
      }

      hbc->add_spacer();
      hbc2->add_spacer();

      if (i < mixed_seq_ports) {
        Label *text2 = memnew(Label);
        text2->set_text(node->get_output_sequence_port_text(i));
        text2->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
        hbc->add_child(text2);
      }

      if (right_ok) {
        if (is_vslist) {
          Button *rmbtn = memnew(Button);
          rmbtn->set_icon(
              EditorNode::get_singleton()->get_gui_base()->get_theme_icon(
                  SNAME("Remove"), SNAME("EditorIcons")));
          hbc->add_child(rmbtn);
          rmbtn->connect(
              "pressed",
              callable_mp(this, &VisualScriptEditor::_remove_output_port)
                  .bind(E, i),
              CONNECT_DEFERRED);

          if (nd_list->is_output_port_type_editable()) {
            OptionButton *opbtn = memnew(OptionButton);
            for (int j = Variant::NIL; j < Variant::VARIANT_MAX; j++) {
              opbtn->add_item(Variant::get_type_name(Variant::Type(j)));
            }
            opbtn->select(right_type);
            opbtn->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
            hbc->add_child(opbtn);
            opbtn->connect(
                "item_selected",
                callable_mp(this, &VisualScriptEditor::_change_port_type)
                    .bind(E, i, false),
                CONNECT_DEFERRED);
          }

          if (nd_list->is_output_port_name_editable()) {
            LineEdit *name_box = memnew(LineEdit);
            hbc->add_child(name_box);
            name_box->set_custom_minimum_size(Size2(60 * EDSCALE, 0));
            name_box->set_text(right_name);
            name_box->set_expand_to_text_length_enabled(true);
            name_box->connect(
                "resized",
                callable_mp(this, &VisualScriptEditor::_update_node_size)
                    .bind(E));
            name_box->connect(
                "focus_exited",
                callable_mp(this, &VisualScriptEditor::_port_name_focus_out)
                    .bind(name_box, E, i, false));
          } else {
            hbc->add_child(memnew(Label(right_name)));
          }
        } else {
          hbc->add_child(memnew(Label(right_name)));
        }

        Ref<Texture2D> t;
        if (right_type >= 0 && right_type < Variant::VARIANT_MAX) {
          t = type_icons[right_type];
        }
        if (t.is_valid()) {
          TextureRect *tf = memnew(TextureRect);
          tf->set_texture(t);
          tf->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
          hbc->add_child(tf);
        }
      }

      gnode->add_child(vbc);

      bool dark_theme =
          get_theme_constant(SNAME("dark_theme"), SNAME("Editor"));
      if (i < mixed_seq_ports) {
        gnode->set_slot(slot_idx, left_ok, left_type,
                        _color_from_type(left_type, dark_theme), true,
                        TYPE_SEQUENCE, mono_color, Ref<Texture2D>(), seq_port);
      } else {
        gnode->set_slot(slot_idx, left_ok, left_type,
                        _color_from_type(left_type, dark_theme), right_ok,
                        right_type, _color_from_type(right_type, dark_theme));
      }

      slot_idx++;
    }
    graph->add_child(gnode);
    gnode->set_theme(vstheme);
    if (gnode->is_comment()) {
      graph->move_child(gnode, 0);
    }
  }

  _update_graph_connections();

  float graph_minimap_opacity = EditorSettings::get_singleton()->get(
      "editors/visual_editors/minimap_opacity");
  graph->set_minimap_opacity(graph_minimap_opacity);

  float graph_lines_curvature = EditorSettings::get_singleton()->get(
      "editors/visual_editors/lines_curvature");
  graph->set_connection_lines_curvature(graph_lines_curvature);

  // Use default_func instead of default_func for now I think that should be
  // good stop gap solution to ensure not breaking anything.
  graph->call_deferred(SNAME("set_scroll_ofs"), script->get_scroll() * EDSCALE);
  updating_graph = false;
}

void VisualScriptEditor::_change_port_type(int p_select, int p_id, int p_port,
                                           bool is_input) {
  Ref<VisualScriptLists> vsn = script->get_node(p_id);
  if (!vsn.is_valid()) {
    return;
  }

  undo_redo->create_action(TTR("Change Port Type"));
  if (is_input) {
    undo_redo->add_do_method(vsn.ptr(), "set_input_data_port_type", p_port,
                             Variant::Type(p_select));
    undo_redo->add_undo_method(vsn.ptr(), "set_input_data_port_type", p_port,
                               vsn->get_input_value_port_info(p_port).type);
  } else {
    undo_redo->add_do_method(vsn.ptr(), "set_output_data_port_type", p_port,
                             Variant::Type(p_select));
    undo_redo->add_undo_method(vsn.ptr(), "set_output_data_port_type", p_port,
                               vsn->get_output_value_port_info(p_port).type);
  }
  undo_redo->commit_action();
}

void VisualScriptEditor::_update_node_size(int p_id) {
  Node *node = graph->get_node(itos(p_id));
  if (Object::cast_to<Control>(node)) {
    Object::cast_to<Control>(node)->reset_size(); // Shrink if text is smaller.
  }
}

void VisualScriptEditor::_port_name_focus_out(const Node *p_name_box, int p_id,
                                              int p_port, bool is_input) {
  Ref<VisualScriptLists> vsn = script->get_node(p_id);
  if (!vsn.is_valid()) {
    return;
  }

  String text;

  if (Object::cast_to<LineEdit>(p_name_box)) {
    text = Object::cast_to<LineEdit>(p_name_box)->get_text();
  } else {
    return;
  }

  undo_redo->create_action(TTR("Change Port Name"));
  if (is_input) {
    undo_redo->add_do_method(vsn.ptr(), "set_input_data_port_name", p_port,
                             text);
    undo_redo->add_undo_method(vsn.ptr(), "set_input_data_port_name", p_port,
                               vsn->get_input_value_port_info(p_port).name);
  } else {
    undo_redo->add_do_method(vsn.ptr(), "set_output_data_port_name", p_port,
                             text);
    undo_redo->add_undo_method(vsn.ptr(), "set_output_data_port_name", p_port,
                               vsn->get_output_value_port_info(p_port).name);
  }
  undo_redo->commit_action();
}

void VisualScriptEditor::_update_members() {
  ERR_FAIL_COND(!script.is_valid());

  updating_members = true;

  members->clear();
  TreeItem *root = members->create_item();

  TreeItem *functions = members->create_item(root);
  functions->set_selectable(0, false);
  functions->set_text(0, TTR("Functions:"));
  functions->add_button(
      0, Control::get_theme_icon(SNAME("Override"), SNAME("EditorIcons")), 1,
      false, TTR("Override an existing built-in function."));
  functions->add_button(
      0, Control::get_theme_icon(SNAME("Add"), SNAME("EditorIcons")), 0, false,
      TTR("Create a new function."));
  functions->set_custom_color(
      0, Control::get_theme_color(SNAME("mono_color"), SNAME("Editor")));

  List<StringName> func_names;
  script->get_function_list(&func_names);
  func_names.sort_custom<StringName::AlphCompare>();
  for (const StringName &E : func_names) {
    TreeItem *ti = members->create_item(functions);
    ti->set_text(0, E);
    ti->set_selectable(0, true);
    ti->set_metadata(0, E);
    ti->add_button(
        0, Control::get_theme_icon(SNAME("Edit"), SNAME("EditorIcons")), 0);
    if (selected == E) {
      ti->select(0);
    }
  }

  TreeItem *variables = members->create_item(root);
  variables->set_selectable(0, false);
  variables->set_text(0, TTR("Variables:"));
  variables->add_button(
      0, Control::get_theme_icon(SNAME("Add"), SNAME("EditorIcons")), -1, false,
      TTR("Create a new variable."));
  variables->set_custom_color(
      0, Control::get_theme_color(SNAME("mono_color"), SNAME("Editor")));

  Ref<Texture2D> type_icons[Variant::VARIANT_MAX] = {
      Control::get_theme_icon(SNAME("Variant"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("bool"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("int"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("float"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("String"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Vector2"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Vector2i"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Rect2"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Rect2i"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Vector3"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Vector3i"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Transform2D"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Plane"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Quaternion"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("AABB"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Basis"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Transform3D"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Color"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("StringName"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("NodePath"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("RID"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("MiniObject"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Callable"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Signal"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Dictionary"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("Array"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedByteArray"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedInt32Array"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedInt64Array"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedFloat32Array"),
                              SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedFloat64Array"),
                              SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedStringArray"), SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedVector2Array"),
                              SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedVector3Array"),
                              SNAME("EditorIcons")),
      Control::get_theme_icon(SNAME("PackedColorArray"), SNAME("EditorIcons"))};

  List<StringName> var_names;
  script->get_variable_list(&var_names);
  var_names.sort_custom<StringName::AlphCompare>();
  for (const StringName &E : var_names) {
    TreeItem *ti = members->create_item(variables);

    ti->set_text(0, E);

    ti->set_suffix(0, "= " + _sanitized_variant_text(E));
    ti->set_icon(0, type_icons[script->get_variable_info(E).type]);

    ti->set_selectable(0, true);
    ti->set_editable(0, true);
    ti->set_metadata(0, E);
    if (selected == E) {
      ti->select(0);
    }
  }

  TreeItem *_signals = members->create_item(root);
  _signals->set_selectable(0, false);
  _signals->set_text(0, TTR("Signals:"));
  _signals->add_button(
      0, Control::get_theme_icon(SNAME("Add"), SNAME("EditorIcons")), -1, false,
      TTR("Create a new signal."));
  _signals->set_custom_color(
      0, Control::get_theme_color(SNAME("mono_color"), SNAME("Editor")));

  List<StringName> signal_names;
  script->get_custom_signal_list(&signal_names);
  for (const StringName &E : signal_names) {
    TreeItem *ti = members->create_item(_signals);
    ti->set_text(0, E);
    ti->set_selectable(0, true);
    ti->set_editable(0, true);
    ti->set_metadata(0, E);
    if (selected == E) {
      ti->select(0);
    }
  }

  String base_type = script->get_instance_base_type();
  String icon_type = base_type;
  if (!Control::has_theme_icon(base_type, SNAME("EditorIcons"))) {
    icon_type = "Object";
  }

  base_type_select->set_text(base_type);
  base_type_select->set_icon(
      Control::get_theme_icon(icon_type, SNAME("EditorIcons")));

  updating_members = false;
}

String
VisualScriptEditor::_sanitized_variant_text(const StringName &property_name) {
  Variant var = script->get_variable_default_value(property_name);

  if (script->get_variable_info(property_name).type != Variant::NIL) {
    Callable::CallError ce;
    const Variant *converted = &var;
    Variant n;
    Variant::construct(script->get_variable_info(property_name).type, n,
                       &converted, 1, ce);
    var = n;
  }

  return String(var);
}

void VisualScriptEditor::_member_selected() {
  if (updating_members) {
    return;
  }

  TreeItem *ti = members->get_selected();
  ERR_FAIL_COND(!ti);

  selected = ti->get_metadata(0);

  if (ti->get_parent() == members->get_root()->get_first_child()) {
#ifdef MACOS_ENABLED
    bool held_ctrl = Input::get_singleton()->is_key_pressed(Key::META);
#else
    bool held_ctrl = Input::get_singleton()->is_key_pressed(Key::CTRL);
#endif
    if (held_ctrl) {
      ERR_FAIL_COND(!script->has_function(selected));
      _center_on_node(script->get_function_node_id(selected));
    }
  }
}

void VisualScriptEditor::_member_edited() {
  if (updating_members) {
    return;
  }

  TreeItem *ti = members->get_edited();
  ERR_FAIL_COND(!ti);

  String name = ti->get_metadata(0);
  String new_name = ti->get_text(0);

  if (name == new_name) {
    return;
  }

  if (!new_name.is_valid_identifier()) {
    EditorNode::get_singleton()->show_warning(
        TTR("Name is not a valid identifier:") + " " + new_name);
    updating_members = true;
    ti->set_text(0, name);
    updating_members = false;
    return;
  }

  if (script->has_function(new_name) || script->has_variable(new_name) ||
      script->has_custom_signal(new_name)) {
    EditorNode::get_singleton()->show_warning(
        TTR("Name already in use by another func/var/signal:") + " " +
        new_name);
    updating_members = true;
    ti->set_text(0, name);
    updating_members = false;
    return;
  }

  TreeItem *root = members->get_root();

  if (ti->get_parent() == root->get_first_child()) {
    selected = new_name;

    int node_id = script->get_function_node_id(name);
    Ref<VisualScriptFunction> func;
    if (script->has_node(node_id)) {
      func = script->get_node(node_id);
    }
    undo_redo->create_action(TTR("Rename Function"));
    undo_redo->add_do_method(script.ptr(), "rename_function", name, new_name);
    undo_redo->add_undo_method(script.ptr(), "rename_function", new_name, name);
    if (func.is_valid()) {
      undo_redo->add_do_method(func.ptr(), "set_name", new_name);
      undo_redo->add_undo_method(func.ptr(), "set_name", name);
    }

    // Also fix all function calls.
    List<int> lst;
    script->get_node_list(&lst);
    for (int &F : lst) {
      Ref<VisualScriptFunctionCall> fncall = script->get_node(F);
      if (!fncall.is_valid()) {
        continue;
      }
      if (fncall->get_function() == name) {
        undo_redo->add_do_method(fncall.ptr(), "set_function", new_name);
        undo_redo->add_undo_method(fncall.ptr(), "set_function", name);
      }
    }

    undo_redo->add_do_method(this, "_update_members");
    undo_redo->add_undo_method(this, "_update_members");
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->add_do_method(this, "emit_signal", "edited_script_changed");
    undo_redo->add_undo_method(this, "emit_signal", "edited_script_changed");
    undo_redo->commit_action();

    return; // Or crash because it will become invalid.
  }

  if (ti->get_parent() == root->get_first_child()->get_next()) {
    selected = new_name;
    undo_redo->create_action(TTR("Rename Variable"));
    undo_redo->add_do_method(script.ptr(), "rename_variable", name, new_name);
    undo_redo->add_undo_method(script.ptr(), "rename_variable", new_name, name);

    // Also fix all variable setter & getter calls
    List<int> lst;
    script->get_node_list(&lst);
    for (int &P : lst) {
      Ref<VisualScriptPropertySet> pset = script->get_node(P);
      if (pset.is_valid() && pset->get_property() == name) {
        undo_redo->add_do_method(pset.ptr(), "set_property", new_name);
        undo_redo->add_undo_method(pset.ptr(), "set_property", name);
      }
      Ref<VisualScriptPropertyGet> pget = script->get_node(P);
      if (pget.is_valid() && pget->get_property() == name) {
        undo_redo->add_do_method(pget.ptr(), "set_property", new_name);
        undo_redo->add_undo_method(pget.ptr(), "set_property", name);
      }
    }

    undo_redo->add_do_method(this, "_update_members");
    undo_redo->add_undo_method(this, "_update_members");
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->add_do_method(this, "emit_signal", "edited_script_changed");
    undo_redo->add_undo_method(this, "emit_signal", "edited_script_changed");
    undo_redo->commit_action();

    return; // Or crash because it will become invalid.
  }

  if (ti->get_parent() == root->get_first_child()->get_next()->get_next()) {
    selected = new_name;
    undo_redo->create_action(TTR("Rename Signal"));
    undo_redo->add_do_method(script.ptr(), "rename_custom_signal", name,
                             new_name);
    undo_redo->add_undo_method(script.ptr(), "rename_custom_signal", new_name,
                               name);

    // Also fix all signal emitting nodes
    List<int> lst;
    script->get_node_list(&lst);
    for (int &P : lst) {
      Ref<VisualScriptEmitSignal> psig = script->get_node(P);
      if (psig.is_valid() && psig->get_signal() == name) {
        undo_redo->add_do_method(psig.ptr(), "set_signal", new_name);
        undo_redo->add_undo_method(psig.ptr(), "set_signal", name);
      }
    }

    undo_redo->add_do_method(this, "_update_members");
    undo_redo->add_undo_method(this, "_update_members");
    undo_redo->add_do_method(this, "emit_signal", "edited_script_changed");
    undo_redo->add_undo_method(this, "emit_signal", "edited_script_changed");
    undo_redo->commit_action();

    return; // Or crash because it will become invalid.
  }
}

void VisualScriptEditor::_create_function_dialog() {
  function_create_dialog->popup_centered();
  func_name_box->set_text("");
  func_name_box->grab_focus();
  for (int i = 0; i < func_input_vbox->get_child_count(); i++) {
    Node *nd = func_input_vbox->get_child(i);
    nd->queue_free();
  }
}

void VisualScriptEditor::_create_function() {
  String name = _validate_name((func_name_box->get_text().is_empty())
                                   ? "new_func"
                                   : func_name_box->get_text());
  selected = name;
  Vector2 pos = _get_available_pos();

  Ref<VisualScriptFunction> func_node;
  func_node.instantiate();
  func_node->set_name(name);

  for (int i = 0; i < func_input_vbox->get_child_count(); i++) {
    OptionButton *opbtn = Object::cast_to<OptionButton>(
        func_input_vbox->get_child(i)->get_child(3));
    LineEdit *lne =
        Object::cast_to<LineEdit>(func_input_vbox->get_child(i)->get_child(1));
    if (!opbtn || !lne) {
      continue;
    }
    Variant::Type arg_type = Variant::Type(opbtn->get_selected());
    String arg_name = lne->get_text();
    func_node->add_argument(arg_type, arg_name);
  }

  int func_node_id = script->get_available_id();

  undo_redo->create_action(TTR("Add Function"));
  undo_redo->add_do_method(script.ptr(), "add_function", name, func_node_id);
  undo_redo->add_undo_method(script.ptr(), "remove_function", name);
  undo_redo->add_do_method(script.ptr(), "add_node", func_node_id, func_node,
                           pos);
  undo_redo->add_undo_method(script.ptr(), "remove_node", func_node_id);
  undo_redo->add_do_method(this, "_update_members");
  undo_redo->add_undo_method(this, "_update_members");
  undo_redo->add_do_method(this, "_update_graph");
  undo_redo->add_undo_method(this, "_update_graph");
  undo_redo->add_do_method(this, "emit_signal", "edited_script_changed");
  undo_redo->add_undo_method(this, "emit_signal", "edited_script_changed");
  undo_redo->commit_action();

  _update_graph();
}

void VisualScriptEditor::_add_node_dialog() {
  _generic_search(graph->get_global_position() + Vector2(55, 80), true);
}

void VisualScriptEditor::_add_func_input() {
  HBoxContainer *hbox = memnew(HBoxContainer);
  hbox->set_h_size_flags(SIZE_EXPAND_FILL);

  Label *name_label = memnew(Label);
  name_label->set_text(TTR("Name:"));
  hbox->add_child(name_label);

  LineEdit *name_box = memnew(LineEdit);
  name_box->set_h_size_flags(SIZE_EXPAND_FILL);
  name_box->set_text("input");
  name_box->connect(
      "focus_entered",
      callable_mp(this, &VisualScriptEditor::_deselect_input_names));
  hbox->add_child(name_box);

  Label *type_label = memnew(Label);
  type_label->set_text(TTR("Type:"));
  hbox->add_child(type_label);

  OptionButton *type_box = memnew(OptionButton);
  type_box->set_custom_minimum_size(Size2(120 * EDSCALE, 0));
  for (int i = Variant::NIL; i < Variant::VARIANT_MAX; i++) {
    type_box->add_item(Variant::get_type_name(Variant::Type(i)));
  }
  type_box->select(1);
  hbox->add_child(type_box);

  Button *delete_button = memnew(Button);
  delete_button->set_icon(
      EditorNode::get_singleton()->get_gui_base()->get_theme_icon(
          SNAME("Remove"), SNAME("EditorIcons")));
  delete_button->set_tooltip_text(vformat(TTR("Delete input port")));
  hbox->add_child(delete_button);

  for (int i = 0; i < func_input_vbox->get_child_count(); i++) {
    LineEdit *line_edit =
        (LineEdit *)func_input_vbox->get_child(i)->get_child(1);
    line_edit->deselect();
  }

  func_input_vbox->add_child(hbox);
  hbox->set_meta("id", hbox->get_index());

  delete_button->connect(
      "pressed",
      callable_mp(this, &VisualScriptEditor::_remove_func_input).bind(hbox));

  name_box->select_all();
  name_box->grab_focus();
}

void VisualScriptEditor::_remove_func_input(Node *p_node) {
  func_input_vbox->remove_child(p_node);
  p_node->queue_free();
}

void VisualScriptEditor::_deselect_input_names() {
  int cn = func_input_vbox->get_child_count();
  for (int i = 0; i < cn; i++) {
    LineEdit *lne =
        Object::cast_to<LineEdit>(func_input_vbox->get_child(i)->get_child(1));
    if (lne) {
      lne->deselect();
    }
  }
}

void VisualScriptEditor::_member_button(Object *p_item, int p_column,
                                        int p_button,
                                        MouseButton p_mouse_button) {
  if (p_mouse_button != MouseButton::LEFT) {
    return;
  }

  TreeItem *ti = Object::cast_to<TreeItem>(p_item);

  TreeItem *root = members->get_root();

  if (ti->get_parent() == root) {
    // main buttons
    if (ti == root->get_first_child()) {
      // Add function, this one uses menu.

      if (p_button == 1) {
        // Ensure script base exists otherwise use custom base type.
        ERR_FAIL_COND(script.is_null());
        new_virtual_method_select->select_method_from_base_type(
            script->get_instance_base_type(), true);
        return;
      } else if (p_button == 0) {
        String name = _validate_name("new_function");
        selected = name;
        Vector2 pos = _get_available_pos();

        Ref<VisualScriptFunction> func_node;
        func_node.instantiate();
        func_node->set_name(name);
        int fn_id = script->get_available_id();

        undo_redo->create_action(TTR("Add Function"));
        undo_redo->add_do_method(script.ptr(), "add_function", name, fn_id);
        undo_redo->add_do_method(script.ptr(), "add_node", fn_id, func_node,
                                 pos);
        undo_redo->add_undo_method(script.ptr(), "remove_function", name);
        undo_redo->add_undo_method(script.ptr(), "remove_node", fn_id);
        undo_redo->add_do_method(this, "_update_members");
        undo_redo->add_undo_method(this, "_update_members");
        undo_redo->add_do_method(this, "_update_graph");
        undo_redo->add_undo_method(this, "_update_graph");
        undo_redo->add_do_method(this, "emit_signal", "edited_script_changed");
        undo_redo->add_undo_method(this, "emit_signal",
                                   "edited_script_changed");
        undo_redo->commit_action();

        _update_graph();
      }

      return; // Or crash because it will become invalid.
    }

    if (ti == root->get_first_child()->get_next()) {
      // Add variable.
      String name = _validate_name("new_variable");
      selected = name;

      undo_redo->create_action(TTR("Add Variable"));
      undo_redo->add_do_method(script.ptr(), "add_variable", name);
      undo_redo->add_undo_method(script.ptr(), "remove_variable", name);
      undo_redo->add_do_method(this, "_update_members");
      undo_redo->add_undo_method(this, "_update_members");
      undo_redo->add_do_method(this, "emit_signal", "edited_script_changed");
      undo_redo->add_undo_method(this, "emit_signal", "edited_script_changed");
      undo_redo->commit_action();
      return; // Or crash because it will become invalid.
    }

    if (ti == root->get_first_child()->get_next()->get_next()) {
      // Add variable.
      String name = _validate_name("new_signal");
      selected = name;

      undo_redo->create_action(TTR("Add Signal"));
      undo_redo->add_do_method(script.ptr(), "add_custom_signal", name);
      undo_redo->add_undo_method(script.ptr(), "remove_custom_signal", name);
      undo_redo->add_do_method(this, "_update_members");
      undo_redo->add_undo_method(this, "_update_members");
      undo_redo->add_do_method(this, "emit_signal", "edited_script_changed");
      undo_redo->add_undo_method(this, "emit_signal", "edited_script_changed");
      undo_redo->commit_action();
      return; // Or crash because it will become invalid.
    }
  } else if (ti->get_parent() == root->get_first_child()) {
    selected = ti->get_text(0);
    function_name_edit->set_position(
        get_screen_position() + get_local_mouse_position() - Vector2(60, -10));
    function_name_edit->popup();
    function_name_box->set_text(selected);
    function_name_box->select_all();
    function_name_box->grab_focus();
  }
}

void VisualScriptEditor::_add_input_port(int p_id) {
  Ref<VisualScriptLists> vsn = script->get_node(p_id);
  if (!vsn.is_valid()) {
    return;
  }

  updating_graph = true;

  undo_redo->create_action(TTR("Add Input Port"), UndoRedo::MERGE_ENDS);
  undo_redo->add_do_method(vsn.ptr(), "add_input_data_port", Variant::NIL,
                           "arg", -1);
  undo_redo->add_do_method(this, "_update_graph", p_id);

  undo_redo->add_undo_method(vsn.ptr(), "remove_input_data_port",
                             vsn->get_input_value_port_count());
  undo_redo->add_undo_method(this, "_update_graph", p_id);

  updating_graph = false;

  undo_redo->commit_action();
}

void VisualScriptEditor::_add_output_port(int p_id) {
  Ref<VisualScriptLists> vsn = script->get_node(p_id);
  if (!vsn.is_valid()) {
    return;
  }

  updating_graph = true;

  undo_redo->create_action(TTR("Add Output Port"), UndoRedo::MERGE_ENDS);
  undo_redo->add_do_method(vsn.ptr(), "add_output_data_port", Variant::NIL,
                           "arg", -1);
  undo_redo->add_do_method(this, "_update_graph", p_id);

  undo_redo->add_undo_method(vsn.ptr(), "remove_output_data_port",
                             vsn->get_output_value_port_count());
  undo_redo->add_undo_method(this, "_update_graph", p_id);

  updating_graph = false;

  undo_redo->commit_action();
}

void VisualScriptEditor::_remove_input_port(int p_id, int p_port) {
  Ref<VisualScriptLists> vsn = script->get_node(p_id);
  if (!vsn.is_valid()) {
    return;
  }

  updating_graph = true;

  undo_redo->create_action(TTR("Remove Input Port"), UndoRedo::MERGE_ENDS);

  int conn_from = -1, conn_port = -1;
  script->get_input_value_port_connection_source(p_id, p_port, &conn_from,
                                                 &conn_port);

  if (conn_from != -1) {
    undo_redo->add_do_method(script.ptr(), "data_disconnect", conn_from,
                             conn_port, p_id, p_port);
  }

  undo_redo->add_do_method(vsn.ptr(), "remove_input_data_port", p_port);
  undo_redo->add_do_method(this, "_update_graph", p_id);

  if (conn_from != -1) {
    undo_redo->add_undo_method(script.ptr(), "data_connect", conn_from,
                               conn_port, p_id, p_port);
  }

  undo_redo->add_undo_method(vsn.ptr(), "add_input_data_port",
                             vsn->get_input_value_port_info(p_port).type,
                             vsn->get_input_value_port_info(p_port).name,
                             p_port);
  undo_redo->add_undo_method(this, "_update_graph", p_id);

  updating_graph = false;

  undo_redo->commit_action();
}

void VisualScriptEditor::_remove_output_port(int p_id, int p_port) {
  Ref<VisualScriptLists> vsn = script->get_node(p_id);
  if (!vsn.is_valid()) {
    return;
  }

  updating_graph = true;

  undo_redo->create_action(TTR("Remove Output Port"), UndoRedo::MERGE_ENDS);

  List<VisualScript::DataConnection> data_connections;
  script->get_data_connection_list(&data_connections);

  HashMap<int, RBSet<int>> conn_map;
  for (const VisualScript::DataConnection &E : data_connections) {
    if (E.from_node == p_id && E.from_port == p_port) {
      // Push into the connections map.
      if (!conn_map.has(E.to_node)) {
        conn_map.insert(E.to_node, RBSet<int>());
      }
      conn_map[E.to_node].insert(E.to_port);
    }
  }

  undo_redo->add_do_method(vsn.ptr(), "remove_output_data_port", p_port);
  undo_redo->add_do_method(this, "_update_graph", p_id);

  for (const KeyValue<int, RBSet<int>> &E : conn_map) {
    for (const int &F : E.value) {
      undo_redo->add_undo_method(script.ptr(), "data_connect", p_id, p_port,
                                 E.key, F);
    }
  }

  undo_redo->add_undo_method(vsn.ptr(), "add_output_data_port",
                             vsn->get_output_value_port_info(p_port).type,
                             vsn->get_output_value_port_info(p_port).name,
                             p_port);
  undo_redo->add_undo_method(this, "_update_graph", p_id);

  updating_graph = false;

  undo_redo->commit_action();
}

void VisualScriptEditor::_expression_text_changed(const String &p_text,
                                                  int p_id) {
  Ref<VisualScriptExpression> vse = script->get_node(p_id);
  if (!vse.is_valid()) {
    return;
  }

  updating_graph = true;

  undo_redo->create_action(TTR("Change Expression"), UndoRedo::MERGE_ENDS);
  undo_redo->add_do_property(vse.ptr(), "expression", p_text);
  undo_redo->add_undo_property(vse.ptr(), "expression", vse->get("expression"));
  undo_redo->add_do_method(this, "_update_graph", p_id);
  undo_redo->add_undo_method(this, "_update_graph", p_id);
  undo_redo->commit_action();

  Node *node = graph->get_node(itos(p_id));
  if (Object::cast_to<Control>(node)) {
    Object::cast_to<Control>(node)->reset_size(); // Shrink if text is smaller.
  }

  updating_graph = false;
}

Vector2 VisualScriptEditor::_get_pos_in_graph(Vector2 p_point) const {
  Vector2 pos =
      (graph->get_scroll_ofs() + p_point) / (graph->get_zoom() * EDSCALE);
  if (graph->is_using_snap()) {
    int snap = graph->get_snap();
    pos = pos.snapped(Vector2(snap, snap));
  }
  return pos;
}

Vector2 VisualScriptEditor::_get_available_pos(bool p_centered,
                                               Vector2 p_pos) const {
  if (p_centered) {
    p_pos = _get_pos_in_graph(graph->get_size() * 0.5);
  }

  while (true) {
    bool exists = false;
    List<int> existing;
    script->get_node_list(&existing);
    for (int &E : existing) {
      Point2 pos = script->get_node_position(E);
      if (pos.distance_to(p_pos) < 50) {
        p_pos += Vector2(graph->get_snap(), graph->get_snap());
        exists = true;
        break;
      }
    }
    if (exists) {
      continue;
    }
    break;
  }

  return p_pos;
}

String VisualScriptEditor::_validate_name(const String &p_name) const {
  String valid = p_name;

  int counter = 1;
  while (true) {
    bool exists = script->has_function(valid) || script->has_variable(valid) ||
                  script->has_custom_signal(valid);

    if (exists) {
      counter++;
      valid = p_name + "_" + itos(counter);
      continue;
    }

    break;
  }

  return valid;
}

void VisualScriptEditor::_on_nodes_copy() {
  clipboard->nodes.clear();
  clipboard->data_connections.clear();
  clipboard->sequence_connections.clear();

  for (int i = 0; i < graph->get_child_count(); i++) {
    GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
    if (gn) {
      if (gn->is_selected()) {
        int id = gn->get_name().operator String().to_int();
        Ref<VisualScriptNode> node = script->get_node(id);
        if (Object::cast_to<VisualScriptFunction>(*node)) {
          EditorNode::get_singleton()->show_warning(
              TTR("Can't copy the function node."));
          return;
        }
        if (node.is_valid()) {
          clipboard->nodes[id] = node->duplicate(true);
          clipboard->nodes_positions[id] = script->get_node_position(id);
        }
      }
    }
  }

  if (clipboard->nodes.is_empty()) {
    return;
  }

  List<VisualScript::SequenceConnection> sequence_connections;
  script->get_sequence_connection_list(&sequence_connections);

  for (const VisualScript::SequenceConnection &E : sequence_connections) {
    if (clipboard->nodes.has(E.from_node) && clipboard->nodes.has(E.to_node)) {
      clipboard->sequence_connections.insert(E);
    }
  }

  List<VisualScript::DataConnection> data_connections;
  script->get_data_connection_list(&data_connections);

  for (const VisualScript::DataConnection &E : data_connections) {
    if (clipboard->nodes.has(E.from_node) && clipboard->nodes.has(E.to_node)) {
      clipboard->data_connections.insert(E);
    }
  }
}

void VisualScriptEditor::_on_nodes_paste() {
  if (clipboard->nodes.is_empty()) {
    EditorNode::get_singleton()->show_warning(TTR("Clipboard is empty!"));
    return;
  }

  HashMap<int, int> remap;

  undo_redo->create_action(TTR("Paste VisualScript Nodes"));
  int idc = script->get_available_id() + 1;

  RBSet<int> to_select;

  RBSet<Vector2> existing_positions;

  {
    List<int> nodes;
    script->get_node_list(&nodes);
    for (int &E : nodes) {
      Vector2 pos = script->get_node_position(E).snapped(Vector2(2, 2));
      existing_positions.insert(pos);
    }
  }

  bool first_paste = true;
  Vector2 position_offset = Vector2(0, 0);

  for (KeyValue<int, Ref<VisualScriptNode>> &E : clipboard->nodes) {
    Ref<VisualScriptNode> node = E.value->duplicate();

    int new_id = idc++;
    to_select.insert(new_id);

    remap[E.key] = new_id;

    Vector2 paste_pos = clipboard->nodes_positions[E.key];

    if (first_paste) {
      position_offset =
          _get_pos_in_graph(mouse_up_position - graph->get_global_position()) -
          paste_pos;
      first_paste = false;
    }

    paste_pos += position_offset;

    while (existing_positions.has(paste_pos.snapped(Vector2(2, 2)))) {
      paste_pos += Vector2(20, 20) * EDSCALE;
    }

    undo_redo->add_do_method(script.ptr(), "add_node", new_id, node, paste_pos);
    undo_redo->add_undo_method(script.ptr(), "remove_node", new_id);
  }

  for (const VisualScript::SequenceConnection &E :
       clipboard->sequence_connections) {
    undo_redo->add_do_method(script.ptr(), "sequence_connect",
                             remap[E.from_node], E.from_output,
                             remap[E.to_node]);
    undo_redo->add_undo_method(script.ptr(), "sequence_disconnect",
                               remap[E.from_node], E.from_output,
                               remap[E.to_node]);
  }

  for (const VisualScript::DataConnection &E : clipboard->data_connections) {
    undo_redo->add_do_method(script.ptr(), "data_connect", remap[E.from_node],
                             E.from_port, remap[E.to_node], E.to_port);
    undo_redo->add_undo_method(script.ptr(), "data_disconnect",
                               remap[E.from_node], E.from_port,
                               remap[E.to_node], E.to_port);
  }

  undo_redo->add_do_method(this, "_update_graph");
  undo_redo->add_undo_method(this, "_update_graph");

  undo_redo->commit_action();

  for (int i = 0; i < graph->get_child_count(); i++) {
    GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
    if (gn) {
      int id = gn->get_name().operator String().to_int();
      gn->set_selected(to_select.has(id));
    }
  }
}

void VisualScriptEditor::_on_nodes_delete() {
  // Delete all the selected nodes.

  List<int> to_erase;

  for (int i = 0; i < graph->get_child_count(); i++) {
    GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
    if (gn) {
      if (gn->is_selected() && gn->is_close_button_visible()) {
        to_erase.push_back(gn->get_name().operator String().to_int());
      }
    }
  }

  if (to_erase.is_empty()) {
    return;
  }

  undo_redo->create_action(TTR("Remove VisualScript Nodes"));

  for (int &F : to_erase) {
    int cr_node = F;

    undo_redo->add_do_method(script.ptr(), "remove_node", cr_node);
    undo_redo->add_undo_method(script.ptr(), "add_node", cr_node,
                               script->get_node(cr_node),
                               script->get_node_position(cr_node));

    List<VisualScript::SequenceConnection> sequence_conns;
    script->get_sequence_connection_list(&sequence_conns);

    for (const VisualScript::SequenceConnection &E : sequence_conns) {
      if (E.from_node == cr_node || E.to_node == cr_node) {
        undo_redo->add_undo_method(script.ptr(), "sequence_connect",
                                   E.from_node, E.from_output, E.to_node);
      }
    }

    List<VisualScript::DataConnection> data_conns;
    script->get_data_connection_list(&data_conns);

    for (const VisualScript::DataConnection &E : data_conns) {
      if (E.from_node == F || E.to_node == F) {
        undo_redo->add_undo_method(script.ptr(), "data_connect", E.from_node,
                                   E.from_port, E.to_node, E.to_port);
      }
    }
  }
  undo_redo->add_do_method(this, "_update_graph");
  undo_redo->add_undo_method(this, "_update_graph");

  undo_redo->commit_action();
}

void VisualScriptEditor::_on_nodes_duplicate() {
  RBSet<int> to_duplicate;

  for (int i = 0; i < graph->get_child_count(); i++) {
    GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
    if (gn) {
      if (gn->is_selected() && gn->is_close_button_visible()) {
        int id = gn->get_name().operator String().to_int();
        to_duplicate.insert(id);
      }
    }
  }

  if (to_duplicate.is_empty()) {
    return;
  }

  undo_redo->create_action(TTR("Duplicate VisualScript Nodes"));
  int idc = script->get_available_id() + 1;

  RBSet<int> to_select;
  HashMap<int, int> remap;

  for (const int &F : to_duplicate) {
    // Duplicate from the specific function but place it into the default func
    // as it would lack the connections.
    Ref<VisualScriptNode> node = script->get_node(F);

    Ref<VisualScriptNode> dupe = node->duplicate(true);

    int new_id = idc++;
    remap.insert(F, new_id);

    to_select.insert(new_id);
    undo_redo->add_do_method(script.ptr(), "add_node", new_id, dupe,
                             script->get_node_position(F) + Vector2(20, 20));
    undo_redo->add_undo_method(script.ptr(), "remove_node", new_id);
  }

  List<VisualScript::SequenceConnection> seqs;
  script->get_sequence_connection_list(&seqs);
  for (const VisualScript::SequenceConnection &E : seqs) {
    if (to_duplicate.has(E.from_node) && to_duplicate.has(E.to_node)) {
      undo_redo->add_do_method(script.ptr(), "sequence_connect",
                               remap[E.from_node], E.from_output,
                               remap[E.to_node]);
    }
  }

  List<VisualScript::DataConnection> data;
  script->get_data_connection_list(&data);
  for (const VisualScript::DataConnection &E : data) {
    if (to_duplicate.has(E.from_node) && to_duplicate.has(E.to_node)) {
      undo_redo->add_do_method(script.ptr(), "data_connect", remap[E.from_node],
                               E.from_port, remap[E.to_node], E.to_port);
    }
  }

  undo_redo->add_do_method(this, "_update_graph");
  undo_redo->add_undo_method(this, "_update_graph");

  undo_redo->commit_action();

  for (int i = 0; i < graph->get_child_count(); i++) {
    GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
    if (gn) {
      int id = gn->get_name().operator String().to_int();
      gn->set_selected(to_select.has(id));
    }
  }

  if (to_select.size()) {
    EditorNode::get_singleton()->push_item(
        script->get_node(to_select.front()->get()).ptr());
  }
}

void VisualScriptEditor::_generic_search(Vector2 pos, bool node_centered) {
  if (node_centered) {
    port_action_pos = graph->get_size() / 2.0f;
  } else {
    port_action_pos = graph->get_viewport()->get_mouse_position() -
                      graph->get_global_position();
  }

  new_connect_node_select->select_from_visual_script(
      script, false); // do not reset text
}

void VisualScriptEditor::input(const Ref<InputEvent> &p_event) {
  ERR_FAIL_COND(p_event.is_null());

  // GUI input for VS Editor Plugin
  Ref<InputEventMouseButton> key = p_event;

  if (key.is_valid() && key->is_pressed()) {
    mouse_up_position = get_screen_position() + get_local_mouse_position();
  }
}

void VisualScriptEditor::_graph_gui_input(const Ref<InputEvent> &p_event) {
  Ref<InputEventMouseButton> key = p_event;

  if (key.is_valid() && key->is_pressed() &&
      key->get_button_mask() == MouseButton::RIGHT) {
    bool is_empty_selection = true;

    for (int i = 0; i < graph->get_child_count(); i++) {
      GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
      if (gn && gn->is_selected()) {
        is_empty_selection = false;
        break;
      }
    }
    if (is_empty_selection && clipboard->nodes.is_empty()) {
      _generic_search();
    } else {
      popup_menu->set_item_disabled(int(EDIT_CUT_NODES), is_empty_selection);
      popup_menu->set_item_disabled(int(EDIT_COPY_NODES), is_empty_selection);
      popup_menu->set_item_disabled(int(EDIT_PASTE_NODES),
                                    clipboard->nodes.is_empty());
      popup_menu->set_item_disabled(int(EDIT_DELETE_NODES), is_empty_selection);
      popup_menu->set_item_disabled(int(EDIT_DUPLICATE_NODES),
                                    is_empty_selection);
      popup_menu->set_item_disabled(int(EDIT_CLEAR_COPY_BUFFER),
                                    clipboard->nodes.is_empty());

      popup_menu->set_position(mouse_up_position);
      popup_menu->popup();
    }
  }
}

void VisualScriptEditor::_members_gui_input(const Ref<InputEvent> &p_event) {
  Ref<InputEventKey> key = p_event;
  if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
    if (members->has_focus()) {
      TreeItem *ti = members->get_selected();
      if (ti) {
        TreeItem *root = members->get_root();
        if (ti->get_parent() == root->get_first_child()) {
          member_type = MEMBER_FUNCTION;
        }
        if (ti->get_parent() == root->get_first_child()->get_next()) {
          member_type = MEMBER_VARIABLE;
        }
        if (ti->get_parent() ==
            root->get_first_child()->get_next()->get_next()) {
          member_type = MEMBER_SIGNAL;
        }
        member_name = ti->get_text(0);
      }
      if (ED_IS_SHORTCUT("ui_graph_delete", p_event)) {
        _member_option(MEMBER_REMOVE);
      }
      if (ED_IS_SHORTCUT("visual_script_editor/edit_member", p_event)) {
        _member_option(MEMBER_EDIT);
      }
    }
  }

  Ref<InputEventMouseButton> btn = p_event;
  if (btn.is_valid() && btn->is_double_click()) {
    TreeItem *ti = members->get_selected();
    if (ti && ti->get_parent() ==
                  members->get_root()
                      ->get_first_child()) { // to check if it's a function
      _center_on_node(script->get_function_node_id(ti->get_metadata(0)));
    }
  }
}

void VisualScriptEditor::_rename_function(const String &name,
                                          const String &new_name) {
  if (!new_name.is_valid_identifier()) {
    EditorNode::get_singleton()->show_warning(
        TTR("Name is not a valid identifier:") + " " + new_name);
    return;
  }

  if (script->has_function(new_name) || script->has_variable(new_name) ||
      script->has_custom_signal(new_name)) {
    EditorNode::get_singleton()->show_warning(
        TTR("Name already in use by another func/var/signal:") + " " +
        new_name);
    return;
  }

  int node_id = script->get_function_node_id(name);
  Ref<VisualScriptFunction> func;
  if (script->has_node(node_id)) {
    func = script->get_node(node_id);
  }
  undo_redo->create_action(TTR("Rename Function"));
  undo_redo->add_do_method(script.ptr(), "rename_function", name, new_name);
  undo_redo->add_undo_method(script.ptr(), "rename_function", new_name, name);
  if (func.is_valid()) {
    undo_redo->add_do_method(func.ptr(), "set_name", new_name);
    undo_redo->add_undo_method(func.ptr(), "set_name", name);
  }

  // Also fix all function calls.
  List<int> lst;
  script->get_node_list(&lst);
  for (int &F : lst) {
    Ref<VisualScriptFunctionCall> fncall = script->get_node(F);
    if (!fncall.is_valid()) {
      continue;
    }
    if (fncall->get_function() == name) {
      undo_redo->add_do_method(fncall.ptr(), "set_function", new_name);
      undo_redo->add_undo_method(fncall.ptr(), "set_function", name);
    }
  }

  undo_redo->add_do_method(this, "_update_members");
  undo_redo->add_undo_method(this, "_update_members");
  undo_redo->add_do_method(this, "_update_graph");
  undo_redo->add_undo_method(this, "_update_graph");
  undo_redo->add_do_method(this, "emit_signal", "edited_script_changed");
  undo_redo->add_undo_method(this, "emit_signal", "edited_script_changed");
  undo_redo->commit_action();
}

void VisualScriptEditor::_fn_name_box_input(const Ref<InputEvent> &p_event) {
  if (!function_name_edit->is_visible()) {
    return;
  }

  Ref<InputEventKey> key = p_event;
  if (key.is_valid() && key->is_pressed() && key->get_keycode() == Key::ENTER) {
    function_name_edit->hide();
    _on_fn_name_box_confirmed();
    function_name_box->clear();
  }
}

void VisualScriptEditor::_on_fn_name_box_confirmed() {
  _rename_function(selected, function_name_box->get_text());
}

Variant VisualScriptEditor::get_drag_data_fw(const Point2 &p_point,
                                             Control *p_from) {
  if (p_from == members) {
    TreeItem *it = members->get_item_at_position(p_point);
    if (!it) {
      return Variant();
    }

    String type = it->get_metadata(0);

    if (type.is_empty()) {
      return Variant();
    }

    Dictionary dd;
    TreeItem *root = members->get_root();

    if (it->get_parent() == root->get_first_child()) {
      dd["type"] = "visual_script_function_drag";
      dd["function"] = type;
    } else if (it->get_parent() == root->get_first_child()->get_next()) {
      dd["type"] = "visual_script_variable_drag";
      dd["variable"] = type;
    } else if (it->get_parent() ==
               root->get_first_child()->get_next()->get_next()) {
      dd["type"] = "visual_script_signal_drag";
      dd["signal"] = type;

    } else {
      return Variant();
    }

    Label *label = memnew(Label);
    label->set_text(it->get_text(0));
    set_drag_preview(label);
    return dd;
  }
  return Variant();
}

bool VisualScriptEditor::can_drop_data_fw(const Point2 &p_point,
                                          const Variant &p_data,
                                          Control *p_from) const {
  if (p_from == graph) {
    Dictionary d = p_data;
    if (d.has("type") &&
        (String(d["type"]) == "visual_script_node_drag" ||
         String(d["type"]) == "visual_script_function_drag" ||
         String(d["type"]) == "visual_script_variable_drag" ||
         String(d["type"]) == "visual_script_signal_drag" ||
         String(d["type"]) == "obj_property" ||
         String(d["type"]) == "resource" || String(d["type"]) == "files" ||
         String(d["type"]) == "nodes")) {
      if (String(d["type"]) == "obj_property") {
#ifdef MACOS_ENABLED
        const_cast<VisualScriptEditor *>(this)->_show_hint(
            vformat(TTR("Hold %s to drop a Getter. Hold Shift to drop a "
                        "generic signature."),
                    find_keycode_name(Key::META)));
#else
        const_cast<VisualScriptEditor *>(this)->_show_hint(
            TTR("Hold Ctrl to drop a Getter. Hold Shift to drop a generic "
                "signature."));
#endif
      }

      if (String(d["type"]) == "nodes") {
#ifdef MACOS_ENABLED
        const_cast<VisualScriptEditor *>(this)->_show_hint(
            vformat(TTR("Hold %s to drop a simple reference to the node."),
                    find_keycode_name(Key::META)));
#else
        const_cast<VisualScriptEditor *>(this)->_show_hint(
            TTR("Hold Ctrl to drop a simple reference to the node."));
#endif
      }

      if (String(d["type"]) == "visual_script_variable_drag") {
#ifdef MACOS_ENABLED
        const_cast<VisualScriptEditor *>(this)->_show_hint(
            vformat(TTR("Hold %s to drop a Variable Setter."),
                    find_keycode_name(Key::META)));
#else
        const_cast<VisualScriptEditor *>(this)->_show_hint(
            TTR("Hold Ctrl to drop a Variable Setter."));
#endif
      }

      return true;
    }
  }

  return false;
}

static Node *_find_script_node(Node *p_edited_scene, Node *p_current_node,
                               const Ref<Script> &script) {
  if (p_edited_scene != p_current_node &&
      p_current_node->get_owner() != p_edited_scene) {
    return nullptr;
  }

  Ref<Script> scr = p_current_node->get_script();

  if (scr.is_valid() && scr == script) {
    return p_current_node;
  }

  for (int i = 0; i < p_current_node->get_child_count(); i++) {
    Node *n =
        _find_script_node(p_edited_scene, p_current_node->get_child(i), script);
    if (n) {
      return n;
    }
  }

  return nullptr;
}

void VisualScriptEditor::drop_data_fw(const Point2 &p_point,
                                      const Variant &p_data, Control *p_from) {
  if (p_from != graph) {
    return;
  }

  Dictionary d = p_data;

  if (!d.has("type")) {
    return;
  }

  if (String(d["type"]) == "visual_script_node_drag") {
    if (!d.has("node_type") || String(d["node_type"]) == "Null") {
      return;
    }

    Vector2 pos = _get_pos_in_graph(p_point);

    int new_id = _create_new_node_from_name(d["node_type"], pos);

    Node *node = graph->get_node(itos(new_id));
    if (node) {
      graph->set_selected(node);
      _node_selected(node);
    }
  }

  if (String(d["type"]) == "visual_script_variable_drag") {
#ifdef MACOS_ENABLED
    bool use_set = Input::get_singleton()->is_key_pressed(Key::META);
#else
    bool use_set = Input::get_singleton()->is_key_pressed(Key::CTRL);
#endif
    Vector2 pos = _get_pos_in_graph(p_point);

    Ref<VisualScriptNode> vnode;
    if (use_set) {
      Ref<VisualScriptPropertySet> pset;
      pset.instantiate();
      vnode = pset;
    } else {
      Ref<VisualScriptPropertyGet> pget;
      pget.instantiate();
      vnode = pget;
    }

    int new_id = script->get_available_id();
    undo_redo->create_action(TTR("Add Node"));
    undo_redo->add_do_method(vnode.ptr(), "set_property", d["variable"]);
    undo_redo->add_do_method(vnode.ptr(), "set_base_script",
                             script->get_path());

    undo_redo->add_do_method(script.ptr(), "add_node", new_id, vnode, pos);
    undo_redo->add_undo_method(script.ptr(), "remove_node", new_id);
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->commit_action();

    Node *node = graph->get_node(itos(new_id));
    if (node) {
      graph->set_selected(node);
      _node_selected(node);
    }
  }

  if (String(d["type"]) == "visual_script_function_drag") {
    Vector2 pos = _get_pos_in_graph(p_point);

    Ref<VisualScriptFunctionCall> vnode;
    vnode.instantiate();
    vnode->set_call_mode(VisualScriptFunctionCall::CALL_MODE_SELF);

    int new_id = script->get_available_id();

    undo_redo->create_action(TTR("Add Node"));
    undo_redo->add_do_method(script.ptr(), "add_node", new_id, vnode, pos);
    undo_redo->add_do_method(vnode.ptr(), "set_base_type",
                             script->get_instance_base_type());
    undo_redo->add_do_method(vnode.ptr(), "set_function", d["function"]);

    undo_redo->add_undo_method(script.ptr(), "remove_node", new_id);
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->commit_action();

    Node *node = graph->get_node(itos(new_id));
    if (node) {
      graph->set_selected(node);
      _node_selected(node);
    }
  }

  if (String(d["type"]) == "visual_script_signal_drag") {
    Vector2 pos = _get_pos_in_graph(p_point);

    Ref<VisualScriptEmitSignal> vnode;
    vnode.instantiate();
    vnode->set_signal(d["signal"]);

    int new_id = script->get_available_id();

    undo_redo->create_action(TTR("Add Node"));
    undo_redo->add_do_method(script.ptr(), "add_node", new_id, vnode, pos);
    undo_redo->add_undo_method(script.ptr(), "remove_node", new_id);
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->commit_action();

    Node *node = graph->get_node(itos(new_id));
    if (node) {
      graph->set_selected(node);
      _node_selected(node);
    }
  }

  if (String(d["type"]) == "resource") {
    Vector2 pos = _get_pos_in_graph(p_point);

    Ref<VisualScriptPreload> prnode;
    prnode.instantiate();
    prnode->set_preload(d["resource"]);

    int new_id = script->get_available_id();

    undo_redo->create_action(TTR("Add Preload Node"));
    undo_redo->add_do_method(script.ptr(), "add_node", new_id, prnode, pos);
    undo_redo->add_undo_method(script.ptr(), "remove_node", new_id);
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->commit_action();

    Node *node = graph->get_node(itos(new_id));
    if (node) {
      graph->set_selected(node);
      _node_selected(node);
    }
  }

  if (String(d["type"]) == "files") {
#ifdef MACOS_ENABLED
    bool use_preload = Input::get_singleton()->is_key_pressed(Key::META);
#else
    bool use_preload = Input::get_singleton()->is_key_pressed(Key::CTRL);
#endif
    Vector2 pos = _get_pos_in_graph(p_point);

    Array files = d["files"];

    List<int> new_ids;
    int new_id = script->get_available_id();

    if (files.size()) {
      undo_redo->create_action(TTR("Add Node(s)"));

      for (int i = 0; i < files.size(); i++) {
        Ref<Resource> res = ResourceLoader::load(files[i]);
        if (!res.is_valid()) {
          continue;
        }
        Ref<Script> drop_script = ResourceLoader::load(files[i]);
        if (drop_script.is_valid() && drop_script->is_tool() &&
            drop_script->get_instance_base_type() == "VisualScriptCustomNode" &&
            !use_preload) {
          Ref<VisualScriptCustomNode> vscn;
          vscn.instantiate();
          vscn->set_script(drop_script);

          undo_redo->add_do_method(script.ptr(), "add_node", new_id, vscn, pos);
          undo_redo->add_undo_method(script.ptr(), "remove_node", new_id);
        } else {
          Ref<VisualScriptPreload> prnode;
          prnode.instantiate();
          prnode->set_preload(res);

          undo_redo->add_do_method(script.ptr(), "add_node", new_id, prnode,
                                   pos);
          undo_redo->add_undo_method(script.ptr(), "remove_node", new_id);
        }
        new_ids.push_back(new_id);
        new_id++;
        pos += Vector2(20, 20);
      }

      undo_redo->add_do_method(this, "_update_graph");
      undo_redo->add_undo_method(this, "_update_graph");
      undo_redo->commit_action();
    }

    for (int &E : new_ids) {
      Node *node = graph->get_node(itos(E));
      if (node) {
        graph->set_selected(node);
        _node_selected(node);
      }
    }
  }

  if (String(d["type"]) == "nodes") {
    Node *sn = _find_script_node(get_tree()->get_edited_scene_root(),
                                 get_tree()->get_edited_scene_root(), script);

    if (!sn) {
      EditorNode::get_singleton()->show_warning(
          vformat(TTR("Can't drop nodes because script '%s' is not used in "
                      "this scene."),
                  get_name()));
      return;
    }

#ifdef MACOS_ENABLED
    bool use_node = Input::get_singleton()->is_key_pressed(Key::META);
#else
    bool use_node = Input::get_singleton()->is_key_pressed(Key::CTRL);
#endif

    Array nodes = d["nodes"];

    Vector2 pos = _get_pos_in_graph(p_point);

    undo_redo->create_action(TTR("Add Node(s) From Tree"));
    int base_id = script->get_available_id();

    if (use_node || nodes.size() > 1) {
      for (int i = 0; i < nodes.size(); i++) {
        NodePath np = nodes[i];
        Node *node = get_node(np);
        if (!node) {
          continue;
        }

        Ref<VisualScriptNode> n;

        Ref<VisualScriptSceneNode> scene_node;
        scene_node.instantiate();
        scene_node->set_node_path(sn->get_path_to(node));
        n = scene_node;

        undo_redo->add_do_method(script.ptr(), "add_node", base_id, n, pos);
        undo_redo->add_undo_method(script.ptr(), "remove_node", base_id);

        base_id++;
        pos += Vector2(25, 25);
      }

    } else {
      NodePath np = nodes[0];
      Node *node = get_node(np);
      drop_position = pos;
      drop_node = node;
      drop_path = sn->get_path_to(node);
      new_connect_node_select->select_from_instance(node, false);
    }
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->commit_action();
  }

  if (String(d["type"]) == "obj_property") {
    Node *sn = _find_script_node(get_tree()->get_edited_scene_root(),
                                 get_tree()->get_edited_scene_root(), script);

    if (!sn && !Input::get_singleton()->is_key_pressed(Key::SHIFT)) {
      EditorNode::get_singleton()->show_warning(vformat(
          TTR("Can't drop properties because script '%s' is not used in this "
              "scene.\nDrop holding 'Shift' to just copy the signature."),
          get_name()));
      return;
    }

    Object *obj = d["object"];

    if (!obj) {
      return;
    }

    Node *node = Object::cast_to<Node>(obj);
    Vector2 pos = _get_pos_in_graph(p_point);

#ifdef MACOS_ENABLED
    bool use_get = Input::get_singleton()->is_key_pressed(Key::META);
#else
    bool use_get = Input::get_singleton()->is_key_pressed(Key::CTRL);
#endif

    if (!node || Input::get_singleton()->is_key_pressed(Key::SHIFT)) {
      if (use_get) {
        undo_redo->create_action(TTR("Add Getter Property"));
      } else {
        undo_redo->create_action(TTR("Add Setter Property"));
      }

      int base_id = script->get_available_id();

      Ref<VisualScriptNode> vnode;

      if (!use_get) {
        Ref<VisualScriptPropertySet> pset;
        pset.instantiate();
        pset->set_call_mode(VisualScriptPropertySet::CALL_MODE_INSTANCE);
        pset->set_base_type(obj->get_class());
        vnode = pset;
      } else {
        Ref<VisualScriptPropertyGet> pget;
        pget.instantiate();
        pget->set_call_mode(VisualScriptPropertyGet::CALL_MODE_INSTANCE);
        pget->set_base_type(obj->get_class());
        vnode = pget;
      }

      undo_redo->add_do_method(script.ptr(), "add_node", base_id, vnode, pos);
      undo_redo->add_do_method(vnode.ptr(), "set_property", d["property"]);
      if (!obj->get_script().is_null()) {
        undo_redo->add_do_method(vnode.ptr(), "set_base_script",
                                 Ref<Script>(obj->get_script())->get_path());
      }
      if (!use_get) {
        undo_redo->add_do_method(vnode.ptr(), "set_default_input_value", 0,
                                 d["value"]);
      }

      undo_redo->add_undo_method(script.ptr(), "remove_node", base_id);

      undo_redo->add_do_method(this, "_update_graph");
      undo_redo->add_undo_method(this, "_update_graph");
      undo_redo->commit_action();

    } else {
      if (use_get) {
        undo_redo->create_action(TTR("Add Getter Property"));
      } else {
        undo_redo->create_action(TTR("Add Setter Property"));
      }

      int base_id = script->get_available_id();

      Ref<VisualScriptNode> vnode;

      if (!use_get) {
        Ref<VisualScriptPropertySet> pset;
        pset.instantiate();
        if (sn == node) {
          pset->set_call_mode(VisualScriptPropertySet::CALL_MODE_SELF);
        } else {
          pset->set_call_mode(VisualScriptPropertySet::CALL_MODE_NODE_PATH);
          pset->set_base_path(sn->get_path_to(node));
        }
        vnode = pset;
      } else {
        Ref<VisualScriptPropertyGet> pget;
        pget.instantiate();
        if (sn == node) {
          pget->set_call_mode(VisualScriptPropertyGet::CALL_MODE_SELF);
        } else {
          pget->set_call_mode(VisualScriptPropertyGet::CALL_MODE_NODE_PATH);
          pget->set_base_path(sn->get_path_to(node));
        }
        vnode = pget;
      }
      undo_redo->add_do_method(script.ptr(), "add_node", base_id, vnode, pos);
      undo_redo->add_do_method(vnode.ptr(), "set_property", d["property"]);
      if (!obj->get_script().is_null()) {
        undo_redo->add_do_method(vnode.ptr(), "set_base_script",
                                 Ref<Script>(obj->get_script())->get_path());
      }
      if (!use_get) {
        undo_redo->add_do_method(vnode.ptr(), "set_default_input_value", 0,
                                 d["value"]);
      }

      undo_redo->add_undo_method(script.ptr(), "remove_node", base_id);

      undo_redo->add_do_method(this, "_update_graph");
      undo_redo->add_undo_method(this, "_update_graph");
      undo_redo->commit_action();
    }
  }
}

void VisualScriptEditor::_draw_color_over_button(Object *obj, Color p_color) {
  Button *button = Object::cast_to<Button>(obj);
  if (!button) {
    return;
  }

  Ref<StyleBox> normal = get_theme_stylebox(SNAME("normal"), SNAME("Button"));
  button->draw_rect(Rect2(normal->get_offset(),
                          button->get_size() - normal->get_minimum_size()),
                    p_color);
}

void VisualScriptEditor::_button_resource_previewed(
    const String &p_path, const Ref<Texture2D> &p_preview,
    const Ref<Texture2D> &p_small_preview, Variant p_ud) {
  Array ud = p_ud;
  ERR_FAIL_COND(ud.size() != 2);

  ObjectID id = ud[0];
  Object *obj = ObjectDB::get_instance(id);

  if (!obj) {
    return;
  }

  Button *b = Object::cast_to<Button>(obj);
  ERR_FAIL_COND(!b);

  if (p_preview.is_null()) {
    b->set_text(ud[1]);
  } else {
    b->set_icon(p_preview);
  }
}

/////////////////////////

void VisualScriptEditor::apply_code() {}

Ref<Resource> VisualScriptEditor::get_edited_resource() const { return script; }

void VisualScriptEditor::set_edited_resource(const Ref<Resource> &p_res) {
  ERR_FAIL_COND(script.is_valid());
  ERR_FAIL_COND(p_res.is_null());
  script = p_res;
  signal_editor->script = script;
  signal_editor->undo_redo = undo_redo;
  variable_editor->script = script;
  variable_editor->undo_redo = undo_redo;

  script->connect("node_ports_changed",
                  callable_mp(this, &VisualScriptEditor::_node_ports_changed));

  _update_graph();
  call_deferred(SNAME("_update_members"));
}

void VisualScriptEditor::enable_editor(Control *p_shortcut_context) {}

Vector<String> VisualScriptEditor::get_functions() { return Vector<String>(); }

void VisualScriptEditor::reload_text() {}

String VisualScriptEditor::get_name() {
  String name;

  name = script->get_path().get_file();
  if (name.is_empty()) {
    // This appears for newly created built-in scripts before saving the scene.
    name = TTR("[unsaved]");
  } else if (script->is_built_in()) {
    const String &script_name = script->get_name();
    if (!script_name.is_empty()) {
      // If the built-in script has a custom resource name defined,
      // display the built-in script name as follows: `ResourceName
      // (scene_file.tscn)`
      name = vformat("%s (%s)", script_name, name.get_slice("::", 0));
    }
  }

  if (is_unsaved()) {
    name += "(*)";
  }

  return name;
}

Ref<Texture2D> VisualScriptEditor::get_theme_icon() {
  String icon_name = "VisualScript";
  if (script->is_built_in()) {
    icon_name += "Internal";
  }

  if (Control::has_theme_icon(icon_name, "EditorIcons")) {
    return Control::get_theme_icon(icon_name, SNAME("EditorIcons"));
  }

  return Control::get_theme_icon(SNAME("VisualScript"), SNAME("EditorIcons"));
}

bool VisualScriptEditor::is_unsaved() {
  bool unsaved = script->is_edited() || script->are_subnodes_edited() ||
                 script->get_path().is_empty(); // In memory.
  return unsaved;
}

Variant VisualScriptEditor::get_edit_state() {
  Dictionary d;
  d["scroll"] = graph->get_scroll_ofs();
  d["zoom"] = graph->get_zoom();
  d["using_snap"] = graph->is_using_snap();
  d["snap"] = graph->get_snap();
  return d;
}

void VisualScriptEditor::set_edit_state(const Variant &p_state) {
  Dictionary d = p_state;

  _update_graph();
  _update_members();

  if (d.has("scroll")) {
    graph->set_scroll_ofs(d["scroll"]);
  }
  if (d.has("zoom")) {
    graph->set_zoom(d["zoom"]);
  }
  if (d.has("snap")) {
    graph->set_snap(d["snap"]);
  }
  if (d.has("snap_enabled")) {
    graph->set_use_snap(d["snap_enabled"]);
  }
}

void VisualScriptEditor::_center_on_node(int p_id) {
  Node *n = graph->get_node(itos(p_id));
  GraphNode *gn = Object::cast_to<GraphNode>(n);

  // Clear selection.
  for (int i = 0; i < graph->get_child_count(); i++) {
    GraphNode *gnd = Object::cast_to<GraphNode>(graph->get_child(i));
    if (gnd) {
      gnd->set_selected(false);
    }
  }

  if (gn) {
    gn->set_selected(true);
    Vector2 new_scroll = gn->get_position_offset() * graph->get_zoom() -
                         graph->get_size() * 0.5 + gn->get_size() * 0.5;
    graph->set_scroll_ofs(new_scroll);
    script->set_scroll(new_scroll / EDSCALE);
    script->set_edited(true);
  }
}

void VisualScriptEditor::goto_line(int p_line, bool p_with_error) {
  p_line += 1; // Add one because script lines begin from 0.

  if (p_with_error) {
    error_line = p_line;
  }

  if (script->has_node(p_line)) {
    _update_graph();
    _update_members();

    call_deferred(SNAME("call_deferred"), "_center_on_node",
                  p_line); // The editor might be just created and size might
                           // not exist yet.
  }
}

void VisualScriptEditor::set_executing_line(int p_line) {
  // todo: add a way to show which node is executing right now.
}

void VisualScriptEditor::clear_executing_line() {
  // todo: add a way to show which node is executing right now.
}

void VisualScriptEditor::trim_trailing_whitespace() {}

void VisualScriptEditor::insert_final_newline() {}

void VisualScriptEditor::convert_indent_to_spaces() {}

void VisualScriptEditor::convert_indent_to_tabs() {}

void VisualScriptEditor::ensure_focus() { graph->grab_focus(); }

void VisualScriptEditor::tag_saved_version() {}

void VisualScriptEditor::reload(bool p_soft) { _update_graph(); }

PackedInt32Array VisualScriptEditor::get_breakpoints() {
  PackedInt32Array breakpoints;
  List<StringName> functions;
  script->get_function_list(&functions);
  for (int i = 0; i < functions.size(); i++) {
    List<int> nodes;
    script->get_node_list(&nodes);
    for (int &F : nodes) {
      Ref<VisualScriptNode> vsn = script->get_node(F);
      if (vsn->is_breakpoint()) {
        breakpoints.push_back(
            F - 1); // Subtract 1 because breakpoints in text start from zero.
      }
    }
  }
  return breakpoints;
}

void VisualScriptEditor::add_callback(const String &p_function,
                                      PackedStringArray p_args) {
  if (script->has_function(p_function)) {
    _update_members();
    _update_graph();
    _center_on_node(script->get_function_node_id(p_function));
    return;
  }

  Ref<VisualScriptFunction> func;
  func.instantiate();
  for (int i = 0; i < p_args.size(); i++) {
    String name = p_args[i];
    Variant::Type type = Variant::NIL;

    if (name.contains(":")) {
      String tt = name.get_slice(":", 1);
      name = name.get_slice(":", 0);
      for (int j = 0; j < Variant::VARIANT_MAX; j++) {
        String tname = Variant::get_type_name(Variant::Type(j));
        if (tname == tt) {
          type = Variant::Type(j);
          break;
        }
      }
    }

    func->add_argument(type, name);
  }
  int fn_id = script->get_available_id();
  func->set_name(p_function);
  script->add_function(p_function, fn_id);
  script->add_node(fn_id, func);

  _update_members();
  _update_graph();

  _center_on_node(script->get_function_node_id(p_function));
}

bool VisualScriptEditor::show_members_overview() { return false; }

void VisualScriptEditor::update_settings() { _update_graph(); }

void VisualScriptEditor::set_debugger_active(bool p_active) {
  if (!p_active) {
    error_line = -1;
    _update_graph(); // clear line break
  }
}

Control *VisualScriptEditor::get_base_editor() const { return graph; }

void VisualScriptEditor::set_tooltip_request_func(
    const Callable &p_toolip_callback) {}

Control *VisualScriptEditor::get_edit_menu() { return edit_menu; }

void VisualScriptEditor::_change_base_type() {
  select_base_type->popup_create(true, true, script->get_instance_base_type(),
                                 script->get_path().get_file());
}

void VisualScriptEditor::_toggle_tool_script() {
  script->set_tool_enabled(!script->is_tool());
}

void VisualScriptEditor::clear_edit_menu() {
  memdelete(edit_menu);
  memdelete(members_section);
}

void VisualScriptEditor::_change_base_type_callback() {
  String bt = select_base_type->get_selected_type();

  ERR_FAIL_COND(bt.is_empty());
  undo_redo->create_action(TTR("Change Base Type"));
  undo_redo->add_do_method(script.ptr(), "set_instance_base_type", bt);
  undo_redo->add_undo_method(script.ptr(), "set_instance_base_type",
                             script->get_instance_base_type());
  undo_redo->add_do_method(this, "_update_members");
  undo_redo->add_undo_method(this, "_update_members");
  undo_redo->commit_action();
}

void VisualScriptEditor::_node_selected(Node *p_node) {
  Ref<VisualScriptNode> vnode = p_node->get_meta("__vnode");
  if (vnode.is_null()) {
    return;
  }

  EditorNode::get_singleton()->push_item(vnode.ptr()); // edit node in inspector
}

static bool _get_out_slot(const Ref<VisualScriptNode> &p_node, int p_slot,
                          int &r_real_slot, bool &r_sequence) {
  if (p_slot < p_node->get_output_sequence_port_count()) {
    r_sequence = true;
    r_real_slot = p_slot;

    return true;
  }

  r_real_slot = p_slot - p_node->get_output_sequence_port_count();
  r_sequence = false;

  return (r_real_slot < p_node->get_output_value_port_count());
}

static bool _get_in_slot(const Ref<VisualScriptNode> &p_node, int p_slot,
                         int &r_real_slot, bool &r_sequence) {
  if (p_slot == 0 && p_node->has_input_sequence_port()) {
    r_sequence = true;
    r_real_slot = 0;
    return true;
  }

  r_real_slot = p_slot - (p_node->has_input_sequence_port() ? 1 : 0);
  r_sequence = false;

  return r_real_slot < p_node->get_input_value_port_count();
}

void VisualScriptEditor::_begin_node_move() {
  undo_redo->create_action(TTR("Move Node(s)"));
}

void VisualScriptEditor::_end_node_move() { undo_redo->commit_action(); }

void VisualScriptEditor::_move_node(int p_id, const Vector2 &p_to) {
  if (!script->has_node(p_id)) {
    return;
  }

  Node *node = graph->get_node(itos(p_id));

  if (Object::cast_to<GraphNode>(node)) {
    Object::cast_to<GraphNode>(node)->set_position_offset(p_to);
  }

  script->set_node_position(p_id, p_to / EDSCALE);
}

void VisualScriptEditor::_node_moved(Vector2 p_from, Vector2 p_to, int p_id) {
  undo_redo->add_do_method(this, "_move_node", p_id, p_to);
  undo_redo->add_undo_method(this, "_move_node", p_id, p_from);
}

void VisualScriptEditor::_remove_node(int p_id) {
  undo_redo->create_action(TTR("Remove VisualScript Node"));

  undo_redo->add_do_method(script.ptr(), "remove_node", p_id);
  undo_redo->add_undo_method(script.ptr(), "add_node", p_id,
                             script->get_node(p_id),
                             script->get_node_position(p_id));

  List<VisualScript::SequenceConnection> sequence_conns;
  script->get_sequence_connection_list(&sequence_conns);

  for (const VisualScript::SequenceConnection &E : sequence_conns) {
    if (E.from_node == p_id || E.to_node == p_id) {
      undo_redo->add_undo_method(script.ptr(), "sequence_connect", E.from_node,
                                 E.from_output, E.to_node);
    }
  }

  List<VisualScript::DataConnection> data_conns;
  script->get_data_connection_list(&data_conns);

  for (const VisualScript::DataConnection &E : data_conns) {
    if (E.from_node == p_id || E.to_node == p_id) {
      undo_redo->add_undo_method(script.ptr(), "data_connect", E.from_node,
                                 E.from_port, E.to_node, E.to_port);
    }
  }

  undo_redo->add_do_method(this, "_update_graph");
  undo_redo->add_undo_method(this, "_update_graph");

  undo_redo->commit_action();
}

void VisualScriptEditor::_node_ports_changed(int p_id) { _update_graph(p_id); }

bool VisualScriptEditor::node_has_sequence_connections(int p_id) {
  List<VisualScript::SequenceConnection> sequence_conns;
  script->get_sequence_connection_list(&sequence_conns);

  for (const VisualScript::SequenceConnection &E : sequence_conns) {
    int from = E.from_node;
    int to = E.to_node;

    if (to == p_id || from == p_id) {
      return true;
    }
  }

  return false;
}

void VisualScriptEditor::_graph_connected(const String &p_from, int p_from_slot,
                                          const String &p_to, int p_to_slot) {
  Ref<VisualScriptNode> from_node = script->get_node(p_from.to_int());
  ERR_FAIL_COND(!from_node.is_valid());

  bool from_seq;
  int from_port;

  if (!_get_out_slot(from_node, p_from_slot, from_port, from_seq)) {
    return; // can't connect this, it's invalid
  }

  Ref<VisualScriptNode> to_node = script->get_node(p_to.to_int());
  ERR_FAIL_COND(!to_node.is_valid());

  bool to_seq;
  int to_port;

  if (!_get_in_slot(to_node, p_to_slot, to_port, to_seq)) {
    return; // can't connect this, it's invalid
  }

  ERR_FAIL_COND(from_seq != to_seq);

  // Checking to prevent warnings.
  if (from_seq) {
    if (script->has_sequence_connection(p_from.to_int(), from_port,
                                        p_to.to_int())) {
      return;
    }
  } else if (script->has_data_connection(p_from.to_int(), from_port,
                                         p_to.to_int(), to_port)) {
    return;
  }

  // Preventing connection to itself.
  if (p_from.to_int() == p_to.to_int()) {
    return;
  }

  // Do all the checks here.
  StringName func; // This the func where we store the one the nodes at the end
                   // of the resolution on having multiple nodes.

  undo_redo->create_action(TTR("Connect Nodes"));

  if (from_seq) {
    undo_redo->add_do_method(script.ptr(), "sequence_connect", p_from.to_int(),
                             from_port, p_to.to_int());
    // This undo error on undo after move can't be removed without painful
    // gymnastics
    undo_redo->add_undo_method(script.ptr(), "sequence_disconnect",
                               p_from.to_int(), from_port, p_to.to_int());
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
  } else {
    bool converted = false;

    Ref<VisualScriptOperator> oper = to_node;
    if (oper.is_valid() && oper->get_typed() == Variant::NIL) {
      // It's an operator Node and if the type is already nil
      if (from_node->get_output_value_port_info(from_port).type !=
          Variant::NIL) {
        oper->set_typed(from_node->get_output_value_port_info(from_port).type);
      }
    }

    Ref<VisualScriptOperator> operf = from_node;
    if (operf.is_valid() && operf->get_typed() == Variant::NIL) {
      // It's an operator Node and if the type is already nil
      if (to_node->get_input_value_port_info(to_port).type != Variant::NIL) {
        operf->set_typed(to_node->get_input_value_port_info(to_port).type);
      }
    }

    // Disconnect current, and connect the new one
    if (script->is_input_value_port_connected(p_to.to_int(), to_port)) {
      if (can_swap && data_disconnect_node == p_to.to_int()) {
        int conn_from;
        int conn_port;
        script->get_input_value_port_connection_source(p_to.to_int(), to_port,
                                                       &conn_from, &conn_port);
        undo_redo->add_do_method(script.ptr(), "data_disconnect", conn_from,
                                 conn_port, p_to.to_int(), to_port);
        undo_redo->add_do_method(script.ptr(), "data_connect", conn_from,
                                 conn_port, data_disconnect_node,
                                 data_disconnect_port);
        undo_redo->add_undo_method(script.ptr(), "data_disconnect", conn_from,
                                   conn_port, data_disconnect_node,
                                   data_disconnect_port);
        undo_redo->add_undo_method(script.ptr(), "data_connect", conn_from,
                                   conn_port, p_to.to_int(), to_port);
        can_swap = false; // swapped
      } else {
        int conn_from;
        int conn_port;
        script->get_input_value_port_connection_source(p_to.to_int(), to_port,
                                                       &conn_from, &conn_port);
        undo_redo->add_do_method(script.ptr(), "data_disconnect", conn_from,
                                 conn_port, p_to.to_int(), to_port);
        undo_redo->add_undo_method(script.ptr(), "data_connect", conn_from,
                                   conn_port, p_to.to_int(), to_port);
      }
    }
    if (!converted) {
      undo_redo->add_do_method(script.ptr(), "data_connect", p_from.to_int(),
                               from_port, p_to.to_int(), to_port);
      undo_redo->add_undo_method(script.ptr(), "data_disconnect",
                                 p_from.to_int(), from_port, p_to.to_int(),
                                 to_port);

      // Update nodes in graph
      undo_redo->add_do_method(this, "_update_graph", p_from.to_int());
      undo_redo->add_do_method(this, "_update_graph", p_to.to_int());
      undo_redo->add_undo_method(this, "_update_graph", p_from.to_int());
      undo_redo->add_undo_method(this, "_update_graph", p_to.to_int());
    } else {
      undo_redo->add_do_method(this, "_update_graph");
      undo_redo->add_undo_method(this, "_update_graph");
    }
  }

  undo_redo->commit_action();
}

void VisualScriptEditor::_graph_disconnected(const String &p_from,
                                             int p_from_slot,
                                             const String &p_to,
                                             int p_to_slot) {
  Ref<VisualScriptNode> from_node = script->get_node(p_from.to_int());
  ERR_FAIL_COND(!from_node.is_valid());

  bool from_seq;
  int from_port;

  if (!_get_out_slot(from_node, p_from_slot, from_port, from_seq)) {
    return; // Can't connect this, it's invalid.
  }

  Ref<VisualScriptNode> to_node = script->get_node(p_to.to_int());
  ERR_FAIL_COND(!to_node.is_valid());

  bool to_seq;
  int to_port;

  if (!_get_in_slot(to_node, p_to_slot, to_port, to_seq)) {
    return; // Can't connect this, it's invalid.
  }

  ERR_FAIL_COND(from_seq != to_seq);

  undo_redo->create_action(TTR("Disconnect Nodes"));

  if (from_seq) {
    undo_redo->add_do_method(script.ptr(), "sequence_disconnect",
                             p_from.to_int(), from_port, p_to.to_int());
    undo_redo->add_undo_method(script.ptr(), "sequence_connect",
                               p_from.to_int(), from_port, p_to.to_int());
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
  } else {
    can_swap = true;
    data_disconnect_node = p_to.to_int();
    data_disconnect_port = to_port;

    undo_redo->add_do_method(script.ptr(), "data_disconnect", p_from.to_int(),
                             from_port, p_to.to_int(), to_port);
    undo_redo->add_undo_method(script.ptr(), "data_connect", p_from.to_int(),
                               from_port, p_to.to_int(), to_port);
    // Update relevant nodes in the graph.
    undo_redo->add_do_method(this, "_update_graph", p_from.to_int());
    undo_redo->add_do_method(this, "_update_graph", p_to.to_int());
    undo_redo->add_undo_method(this, "_update_graph", p_from.to_int());
    undo_redo->add_undo_method(this, "_update_graph", p_to.to_int());
  }

  undo_redo->commit_action();
}

void VisualScriptEditor::_graph_connect_to_empty(const String &p_from,
                                                 int p_from_slot,
                                                 const Vector2 &p_release_pos) {
  Node *node = graph->get_node(p_from);
  GraphNode *gn = Object::cast_to<GraphNode>(node);
  if (!gn) {
    return;
  }

  Ref<VisualScriptNode> vsn = script->get_node(p_from.to_int());
  if (!vsn.is_valid()) {
    return;
  }
  if (vsn->get_output_value_port_count() ||
      vsn->get_output_sequence_port_count()) {
    port_action_pos = p_release_pos;
  }

  if (p_from_slot < vsn->get_output_sequence_port_count()) {
    port_action_node = p_from.to_int();
    port_action_output = p_from_slot;
    _port_action_menu(CREATE_ACTION);
  } else {
    port_action_output = p_from_slot - vsn->get_output_sequence_port_count();
    port_action_node = p_from.to_int();
    _port_action_menu(CREATE_CALL_SET_GET);
  }
}

VisualScriptNode::TypeGuess
VisualScriptEditor::_guess_output_type(int p_port_action_node,
                                       int p_port_action_output,
                                       RBSet<int> &visited_nodes) {
  VisualScriptNode::TypeGuess tg;
  tg.type = Variant::NIL;

  if (visited_nodes.has(p_port_action_node)) {
    return tg; // no loop
  }

  visited_nodes.insert(p_port_action_node);

  Ref<VisualScriptNode> node = script->get_node(p_port_action_node);

  if (!node.is_valid() ||
      node->get_output_value_port_count() <= p_port_action_output) {
    return tg;
  }

  Vector<VisualScriptNode::TypeGuess> in_guesses;

  for (int i = 0; i < node->get_input_value_port_count(); i++) {
    PropertyInfo pi = node->get_input_value_port_info(i);
    VisualScriptNode::TypeGuess g;
    g.type = pi.type;

    if (g.type == Variant::NIL || g.type == Variant::OBJECT) {
      // Any or object input, must further guess what this is.
      int from_node;
      int from_port;

      if (script->get_input_value_port_connection_source(
              p_port_action_node, i, &from_node, &from_port)) {
        g = _guess_output_type(from_node, from_port, visited_nodes);
      } else {
        Variant defval = node->get_default_input_value(i);
        if (defval.get_type() == Variant::OBJECT) {
          Object *obj = defval;

          if (obj) {
            g.type = Variant::OBJECT;
            g.gdclass = obj->get_class();
            g.script = obj->get_script();
          }
        }
      }
    }

    in_guesses.push_back(g);
  }

  return node->guess_output_type(in_guesses.ptrw(), p_port_action_output);
}

void VisualScriptEditor::_port_action_menu(int p_option) {
  RBSet<int> vn;

  switch (p_option) {
  case CREATE_CALL_SET_GET: {
    Ref<VisualScriptFunctionCall> n;
    n.instantiate();

    VisualScriptNode::TypeGuess tg =
        _guess_output_type(port_action_node, port_action_output, vn);

    if (tg.gdclass != StringName()) {
      n->set_base_type(tg.gdclass);
    } else {
      n->set_base_type("Object");
    }
    String type_string;
    String base_script = "";
    if (script->get_node(port_action_node)->get_output_value_port_count() > 0) {
      type_string = script->get_node(port_action_node)
                        ->get_output_value_port_info(port_action_output)
                        .hint_string;
      VisualScriptFunctionCall *vsfc =
          Object::cast_to<VisualScriptFunctionCall>(
              *script->get_node(port_action_node));
      if (vsfc) {
        base_script = vsfc->get_base_script();
      } else {
        VisualScriptPropertyGet *vspg =
            Object::cast_to<VisualScriptPropertyGet>(
                *script->get_node(port_action_node));
        if (vspg) {
          base_script = vspg->get_base_script();
        } else {
          VisualScriptPropertySet *vsps =
              Object::cast_to<VisualScriptPropertySet>(
                  *script->get_node(port_action_node));
          if (vsps) {
            base_script = vsps->get_base_script();
          }
        }
      }
    }
    if (tg.type == Variant::OBJECT) {
      if (tg.script.is_valid()) {
        new_connect_node_select->select_from_script(tg.script);
      } else if (type_string != String()) {
        new_connect_node_select->select_from_base_type(type_string,
                                                       base_script);
      } else {
        new_connect_node_select->select_from_base_type(n->get_base_type(),
                                                       base_script);
      }
    } else if (tg.type == Variant::NIL) {
      new_connect_node_select->select_from_base_type("", base_script);
    } else {
      new_connect_node_select->select_from_basic_type(tg.type);
    }
    // Ensure that the dialog fits inside the graph.
    Vector2 pos = mouse_up_position;
    Size2 bounds = graph->get_global_position() + graph->get_size() -
                   new_connect_node_select->get_size();
    pos.x = pos.x > bounds.x ? bounds.x : pos.x;
    pos.y = pos.y > bounds.y ? bounds.y : pos.y;
    new_connect_node_select->set_position(pos);
  } break;
  case CREATE_ACTION: {
    VisualScriptNode::TypeGuess tg =
        _guess_output_type(port_action_node, port_action_output, vn);
    PropertyInfo property_info;
    if (script->get_node(port_action_node)->get_output_value_port_count() > 0) {
      property_info = script->get_node(port_action_node)
                          ->get_output_value_port_info(port_action_output);
    }
    if (tg.type == Variant::OBJECT) {
      if (property_info.type == Variant::OBJECT &&
          !property_info.hint_string.is_empty()) {
        new_connect_node_select->select_from_action(property_info.hint_string);
      } else {
        new_connect_node_select->select_from_action("");
      }
    } else if (tg.type == Variant::NIL) {
      new_connect_node_select->select_from_action("");
    } else {
      new_connect_node_select->select_from_action(
          Variant::get_type_name(tg.type));
    }
    // Ensure that the dialog fits inside the graph.
    Vector2 pos = mouse_up_position;
    Size2 bounds = graph->get_global_position() + graph->get_size() -
                   new_connect_node_select->get_size();
    pos.x = pos.x > bounds.x ? bounds.x : pos.x;
    pos.y = pos.y > bounds.y ? bounds.y : pos.y;
    new_connect_node_select->set_position(pos);
  } break;
  }
}

void VisualScriptEditor::connect_data(Ref<VisualScriptNode> vnode_old,
                                      Ref<VisualScriptNode> vnode, int new_id) {
  undo_redo->create_action(TTR("Connect Node Data"));
  VisualScriptReturn *vnode_return =
      Object::cast_to<VisualScriptReturn>(vnode.ptr());
  if (vnode_return != nullptr && vnode_old->get_output_value_port_count() > 0) {
    vnode_return->set_enable_return_value(true);
  }
  if (vnode_old->get_output_value_port_count() <= 0) {
    undo_redo->commit_action();
    return;
  }
  if (vnode->get_input_value_port_count() <= 0) {
    undo_redo->commit_action();
    return;
  }
  int port = port_action_output;
  int value_count = vnode_old->get_output_value_port_count();
  if (port >= value_count) {
    port = 0;
  }
  undo_redo->add_do_method(script.ptr(), "data_connect", port_action_node, port,
                           new_id, 0);
  undo_redo->add_undo_method(script.ptr(), "data_disconnect", port_action_node,
                             port, new_id, 0);
  undo_redo->commit_action();
}

void VisualScriptEditor::_selected_connect_node(const String &p_text,
                                                const String &p_category,
                                                const bool p_connecting) {
#ifdef MACOS_ENABLED
  bool held_ctrl = Input::get_singleton()->is_key_pressed(Key::META);
#else
  bool held_ctrl = Input::get_singleton()->is_key_pressed(Key::CTRL);
#endif
  Vector2 pos = _get_pos_in_graph(port_action_pos);

  RBSet<int> vn;
  bool port_node_exists = true;

  if (drop_position != Vector2()) {
    pos = drop_position;
  }
  drop_position = Vector2();

  Ref<VisualScriptNode> vnode;
  Ref<VisualScriptNode> vnode_old;
  if (port_node_exists && p_connecting) {
    vnode_old = script->get_node(port_action_node);
  }

  if (p_category.begins_with("VisualScriptNode")) {
    Ref<VisualScriptNode> n =
        VisualScriptLanguage::singleton->create_node_from_name(p_text);

    if (Object::cast_to<VisualScriptTypeCast>(n.ptr()) &&
        vnode_old.is_valid()) {
      Variant::Type type =
          vnode_old->get_output_value_port_info(port_action_output).type;
      String hint_name =
          vnode_old->get_output_value_port_info(port_action_output).hint_string;

      if (type == Variant::OBJECT) {
        Object::cast_to<VisualScriptTypeCast>(n.ptr())->set_base_type(
            hint_name);
      } else if (type == Variant::NIL) {
        Object::cast_to<VisualScriptTypeCast>(n.ptr())->set_base_type("");
      } else {
        Object::cast_to<VisualScriptTypeCast>(n.ptr())->set_base_type(
            Variant::get_type_name(type));
      }
    }
    vnode = n;
  }

  if (p_category == String("Class") && !p_connecting) {
    Ref<VisualScriptFunctionCall> n;
    n.instantiate();
    n->set_call_mode(VisualScriptFunctionCall::CALL_MODE_SINGLETON);
    n->set_singleton("ClassDB");
    n->set_function("instantiate");
    // Did not find a way to edit the input port value
    vnode = n;
  } else if (p_category == String("class_method")) {
    Ref<VisualScriptFunctionCall> n;
    n.instantiate();
    if (!drop_path.is_empty()) {
      if (drop_path == ".") {
        n->set_call_mode(VisualScriptFunctionCall::CALL_MODE_SELF);
      } else {
        n->set_call_mode(VisualScriptFunctionCall::CALL_MODE_NODE_PATH);
        n->set_base_path(drop_path);
      }
    } else {
      n->set_call_mode(VisualScriptFunctionCall::CALL_MODE_INSTANCE);
    }
    if (drop_node) {
      n->set_base_type(drop_node->get_class());
      if (drop_node->get_script_instance()) {
        n->set_base_script(
            drop_node->get_script_instance()->get_script()->get_path());
      }
    }
    vnode = n;
  } else if (p_category == String("class_property")) {
    Vector<String> property_path = p_text.split(":");
    if (held_ctrl) {
      Ref<VisualScriptPropertySet> n;
      n.instantiate();
      n->set_property(property_path[1]);
      if (!drop_path.is_empty()) {
        if (drop_path == ".") {
          n->set_call_mode(VisualScriptPropertySet::CALL_MODE_SELF);
        } else {
          n->set_call_mode(VisualScriptPropertySet::CALL_MODE_NODE_PATH);
          n->set_base_path(drop_path);
        }
      }
      if (drop_node) {
        n->set_base_type(drop_node->get_class());
        if (drop_node->get_script_instance()) {
          n->set_base_script(
              drop_node->get_script_instance()->get_script()->get_path());
        }
      }
      vnode = n;
    } else {
      Ref<VisualScriptPropertyGet> n;
      n.instantiate();
      n->set_property(property_path[1]);
      if (!drop_path.is_empty()) {
        if (drop_path == ".") {
          n->set_call_mode(VisualScriptPropertyGet::CALL_MODE_SELF);
        } else {
          n->set_call_mode(VisualScriptPropertyGet::CALL_MODE_NODE_PATH);
          n->set_base_path(drop_path);
        }
      }
      if (drop_node) {
        n->set_base_type(drop_node->get_class());
        if (drop_node->get_script_instance()) {
          n->set_base_script(
              drop_node->get_script_instance()->get_script()->get_path());
        }
      }
      vnode = n;
    }
  } else if (p_category == String("class_constant")) {
    Vector<String> property_path = p_text.split(":");
    if (ClassDB::class_exists(property_path[0])) {
      Ref<VisualScriptClassConstant> n;
      n.instantiate();
      n->set_base_type(property_path[0]);
      n->set_class_constant(property_path[1]);
      vnode = n;
    } else {
      Ref<VisualScriptBasicTypeConstant> n;
      n.instantiate();
      if (property_path[0] == "Nil") {
        n->set_basic_type(Variant::NIL);
      } else if (property_path[0] == "bool") {
        n->set_basic_type(Variant::BOOL);
      } else if (property_path[0] == "int") {
        n->set_basic_type(Variant::INT);
      } else if (property_path[0] == "float") {
        n->set_basic_type(Variant::FLOAT);
      } else if (property_path[0] == "String") {
        n->set_basic_type(Variant::STRING);
      } else if (property_path[0] == "Vector2") {
        n->set_basic_type(Variant::VECTOR2);
      } else if (property_path[0] == "Vector2i") {
        n->set_basic_type(Variant::VECTOR2I);
      } else if (property_path[0] == "Rect2") {
        n->set_basic_type(Variant::RECT2);
      } else if (property_path[0] == "Rect2i") {
        n->set_basic_type(Variant::RECT2I);
      } else if (property_path[0] == "Transform2D") {
        n->set_basic_type(Variant::TRANSFORM2D);
      } else if (property_path[0] == "Vector3") {
        n->set_basic_type(Variant::VECTOR3);
      } else if (property_path[0] == "Vector3i") {
        n->set_basic_type(Variant::VECTOR3I);
      } else if (property_path[0] == "Plane") {
        n->set_basic_type(Variant::PLANE);
      } else if (property_path[0] == "ABB") {
        n->set_basic_type(Variant::AABB);
      } else if (property_path[0] == "Quaternion") {
        n->set_basic_type(Variant::QUATERNION);
      } else if (property_path[0] == "Basis") {
        n->set_basic_type(Variant::BASIS);
      } else if (property_path[0] == "Transform3D") {
        n->set_basic_type(Variant::TRANSFORM3D);
      } else if (property_path[0] == "Color") {
        n->set_basic_type(Variant::COLOR);
      } else if (property_path[0] == "RID") {
        n->set_basic_type(Variant::RID);
      } else if (property_path[0] == "Object") {
        n->set_basic_type(Variant::OBJECT);
      } else if (property_path[0] == "Callable") {
        n->set_basic_type(Variant::CALLABLE);
      } else if (property_path[0] == "Signal") {
        n->set_basic_type(Variant::SIGNAL);
      } else if (property_path[0] == "StringName") {
        n->set_basic_type(Variant::STRING_NAME);
      } else if (property_path[0] == "NodePath") {
        n->set_basic_type(Variant::NODE_PATH);
      } else if (property_path[0] == "Dictionary") {
        n->set_basic_type(Variant::DICTIONARY);
      } else if (property_path[0] == "Array") {
        n->set_basic_type(Variant::ARRAY);
      } else if (property_path[0] == "PackedByteArray") {
        n->set_basic_type(Variant::PACKED_BYTE_ARRAY);
      } else if (property_path[0] == "PackedInt32Array") {
        n->set_basic_type(Variant::PACKED_INT32_ARRAY);
      } else if (property_path[0] == "PackedInt64Array") {
        n->set_basic_type(Variant::PACKED_INT64_ARRAY);
      } else if (property_path[0] == "PackedFloat32Array") {
        n->set_basic_type(Variant::PACKED_FLOAT32_ARRAY);
      } else if (property_path[0] == "PackedStringArray") {
        n->set_basic_type(Variant::PACKED_STRING_ARRAY);
      } else if (property_path[0] == "PackedVector2Array") {
        n->set_basic_type(Variant::PACKED_VECTOR2_ARRAY);
      } else if (property_path[0] == "PackedVector3Array") {
        n->set_basic_type(Variant::PACKED_VECTOR3_ARRAY);
      } else if (property_path[0] == "PackedColorArray") {
        n->set_basic_type(Variant::PACKED_COLOR_ARRAY);
      }
      n->set_basic_type_constant(property_path[1]);
      vnode = n;
    }

  } else if (p_category == String("class_signal")) {
    Vector<String> property_path = p_text.split(":");
    ERR_FAIL_COND(!(script->has_custom_signal(property_path[1]) ||
                    ClassDB::has_signal(script->get_instance_base_type(),
                                        property_path[1])));

    Ref<VisualScriptEmitSignal> n;
    n.instantiate();
    n->set_signal(property_path[1]);
    vnode = n;
  }
  if (vnode == nullptr) {
    print_error("Category not handled: " + p_category.quote());
  }

  if (Object::cast_to<VisualScriptFunctionCall>(vnode.ptr()) &&
      p_category != "Class" && p_category != "VisualScriptNode") {
    Vector<String> property_path = p_text.split(":");
    String class_of_method = property_path[0];
    String method_name = property_path[1];

    Ref<VisualScriptFunctionCall> vsfc = vnode;
    vsfc->set_function(method_name);

    if (port_node_exists && p_connecting) {
      VisualScriptNode::TypeGuess tg =
          _guess_output_type(port_action_node, port_action_output, vn);

      if (tg.type == Variant::OBJECT) {
        vsfc->set_call_mode(VisualScriptFunctionCall::CALL_MODE_INSTANCE);
        vsfc->set_base_type(String(""));
        if (tg.gdclass != StringName()) {
          vsfc->set_base_type(tg.gdclass);
        } else if (script->get_node(port_action_node).is_valid()) {
          PropertyHint hint =
              script->get_node(port_action_node)
                  ->get_output_value_port_info(port_action_output)
                  .hint;
          String base_type =
              script->get_node(port_action_node)
                  ->get_output_value_port_info(port_action_output)
                  .hint_string;

          if (!base_type.is_empty() && hint == PROPERTY_HINT_TYPE_STRING) {
            vsfc->set_base_type(base_type);
          }
          if (method_name == "call" || method_name == "call_deferred") {
            vsfc->set_function(String(""));
          }
        }
        if (tg.script.is_valid()) {
          vsfc->set_base_script(tg.script->get_path());
        }
      } else if (tg.type == Variant::NIL) {
        vsfc->set_call_mode(VisualScriptFunctionCall::CALL_MODE_INSTANCE);
        vsfc->set_base_type(String(""));
      } else {
        vsfc->set_call_mode(VisualScriptFunctionCall::CALL_MODE_BASIC_TYPE);
        vsfc->set_basic_type(tg.type);
      }
    }
  }

  if (port_node_exists && p_connecting) {
    if (Object::cast_to<VisualScriptPropertySet>(vnode.ptr())) {
      Ref<VisualScriptPropertySet> vsp = vnode;

      VisualScriptNode::TypeGuess tg =
          _guess_output_type(port_action_node, port_action_output, vn);
      if (tg.type == Variant::OBJECT) {
        vsp->set_call_mode(VisualScriptPropertySet::CALL_MODE_INSTANCE);
        vsp->set_base_type(String(""));
        if (tg.gdclass != StringName()) {
          vsp->set_base_type(tg.gdclass);

        } else if (script->get_node(port_action_node).is_valid()) {
          PropertyHint hint =
              script->get_node(port_action_node)
                  ->get_output_value_port_info(port_action_output)
                  .hint;
          String base_type =
              script->get_node(port_action_node)
                  ->get_output_value_port_info(port_action_output)
                  .hint_string;

          if (!base_type.is_empty() && hint == PROPERTY_HINT_TYPE_STRING) {
            vsp->set_base_type(base_type);
          }
        }
        if (tg.script.is_valid()) {
          vsp->set_base_script(tg.script->get_path());
        }
      } else if (tg.type == Variant::NIL) {
        vsp->set_call_mode(VisualScriptPropertySet::CALL_MODE_INSTANCE);
        vsp->set_base_type(String(""));
      } else {
        vsp->set_call_mode(VisualScriptPropertySet::CALL_MODE_BASIC_TYPE);
        vsp->set_basic_type(tg.type);
      }
    }

    if (Object::cast_to<VisualScriptPropertyGet>(vnode.ptr())) {
      Ref<VisualScriptPropertyGet> vsp = vnode;

      VisualScriptNode::TypeGuess tg =
          _guess_output_type(port_action_node, port_action_output, vn);
      if (tg.type == Variant::OBJECT) {
        vsp->set_call_mode(VisualScriptPropertyGet::CALL_MODE_INSTANCE);
        vsp->set_base_type(String(""));
        if (tg.gdclass != StringName()) {
          vsp->set_base_type(tg.gdclass);

        } else if (script->get_node(port_action_node).is_valid()) {
          PropertyHint hint =
              script->get_node(port_action_node)
                  ->get_output_value_port_info(port_action_output)
                  .hint;
          String base_type =
              script->get_node(port_action_node)
                  ->get_output_value_port_info(port_action_output)
                  .hint_string;
          if (!base_type.is_empty() && hint == PROPERTY_HINT_TYPE_STRING) {
            vsp->set_base_type(base_type);
          }
        }
        if (tg.script.is_valid()) {
          vsp->set_base_script(tg.script->get_path());
        }
      } else if (tg.type == Variant::NIL) {
        vsp->set_call_mode(VisualScriptPropertyGet::CALL_MODE_INSTANCE);
        vsp->set_base_type(String(""));
      } else {
        vsp->set_call_mode(VisualScriptPropertyGet::CALL_MODE_BASIC_TYPE);
        vsp->set_basic_type(tg.type);
      }
    }
  }
  if (vnode == nullptr) {
    print_error("Not able to create node from category: \"" + p_category +
                "\" and text \"" + p_text + "\" Not created");
    return;
  }

  int new_id = script->get_available_id();
  undo_redo->create_action(TTR("Add Node"));
  undo_redo->add_do_method(script.ptr(), "add_node", new_id, vnode, pos);
  undo_redo->add_undo_method(script.ptr(), "remove_node", new_id);
  undo_redo->add_do_method(this, "_update_graph", new_id);
  undo_redo->add_undo_method(this, "_update_graph", new_id);
  undo_redo->commit_action();

  port_action_new_node = new_id;

  String base_script = "";
  String base_type = "";
  if (port_node_exists) {
    if (vnode_old.is_valid()) {
      if (Object::cast_to<VisualScriptTypeCast>(vnode_old.ptr())) {
        base_type = Object::cast_to<VisualScriptTypeCast>(vnode_old.ptr())
                        ->get_base_type();
        base_script = Object::cast_to<VisualScriptTypeCast>(vnode_old.ptr())
                          ->get_base_script();
      } else if (Object::cast_to<VisualScriptFunctionCall>(vnode_old.ptr())) {
        base_type = Object::cast_to<VisualScriptFunctionCall>(vnode_old.ptr())
                        ->get_base_type();
        base_script = Object::cast_to<VisualScriptFunctionCall>(vnode_old.ptr())
                          ->get_base_script();
      } else if (Object::cast_to<VisualScriptPropertySet>(vnode_old.ptr())) {
        base_type = Object::cast_to<VisualScriptPropertySet>(vnode_old.ptr())
                        ->get_base_type();
        base_script = Object::cast_to<VisualScriptPropertySet>(vnode_old.ptr())
                          ->get_base_script();
      } else if (Object::cast_to<VisualScriptPropertyGet>(vnode_old.ptr())) {
        base_type = Object::cast_to<VisualScriptPropertyGet>(vnode_old.ptr())
                        ->get_base_type();
        base_script = Object::cast_to<VisualScriptPropertyGet>(vnode_old.ptr())
                          ->get_base_script();
      }
    }

    Vector<String> property_path = p_text.split(":");
    if (ClassDB::is_parent_class(script->get_instance_base_type(),
                                 property_path[0]) ||
        script->get_path().ends_with(property_path[0].unquote())) {
      if (!p_connecting) {
        base_type = script->get_instance_base_type();
        base_script = script->get_path();
      }
    } else {
      base_type = property_path[0];
      base_script = "";
    }

    if (drop_node) {
      Ref<Script> script = drop_node->get_script();
      if (script != nullptr) {
        base_script = script->get_path();
      }
    }

    if (vnode_old.is_valid() && p_connecting) {
      if (base_type == "") {
        base_type = property_path[0];
      } else if (ClassDB::is_parent_class(property_path[0], base_type)) {
        base_type = property_path[0];
      }
      connect_seq(vnode_old, vnode, port_action_new_node);
      connect_data(vnode_old, vnode, port_action_new_node);
    }
  }
  if (Object::cast_to<VisualScriptTypeCast>(vnode.ptr())) {
    Object::cast_to<VisualScriptTypeCast>(vnode.ptr())
        ->set_base_type(base_type);
    Object::cast_to<VisualScriptTypeCast>(vnode.ptr())
        ->set_base_script(base_script);
  } else if (Object::cast_to<VisualScriptFunctionCall>(vnode.ptr())) {
    if (base_type_map.has(base_type)) {
      Object::cast_to<VisualScriptFunctionCall>(vnode.ptr())
          ->set_basic_type(base_type_map[base_type]);
      Object::cast_to<VisualScriptFunctionCall>(vnode.ptr())
          ->set_call_mode(VisualScriptFunctionCall::CALL_MODE_BASIC_TYPE);
    } else {
      Object::cast_to<VisualScriptFunctionCall>(vnode.ptr())
          ->set_base_type(base_type);
      Object::cast_to<VisualScriptFunctionCall>(vnode.ptr())
          ->set_base_script(base_script);
    }
  } else if (Object::cast_to<VisualScriptPropertySet>(vnode.ptr())) {
    Object::cast_to<VisualScriptPropertySet>(vnode.ptr())
        ->set_base_type(base_type);
    Object::cast_to<VisualScriptPropertySet>(vnode.ptr())
        ->set_base_script(base_script);
  } else if (Object::cast_to<VisualScriptPropertyGet>(vnode.ptr())) {
    Object::cast_to<VisualScriptPropertyGet>(vnode.ptr())
        ->set_base_type(base_type);
    Object::cast_to<VisualScriptPropertyGet>(vnode.ptr())
        ->set_base_script(base_script);
  }

  drop_path = String();
  drop_node = nullptr;

  _update_graph(port_action_new_node);
}

void VisualScriptEditor::connect_seq(Ref<VisualScriptNode> vnode_old,
                                     Ref<VisualScriptNode> vnode_new,
                                     int new_id) {
  VisualScriptOperator *vnode_operator =
      Object::cast_to<VisualScriptOperator>(vnode_new.ptr());
  if (vnode_operator != nullptr && !vnode_operator->has_input_sequence_port()) {
    return;
  }
  VisualScriptConstructor *vnode_constructor =
      Object::cast_to<VisualScriptConstructor>(vnode_new.ptr());
  if (vnode_constructor != nullptr) {
    return;
  }
  if (vnode_old->get_output_sequence_port_count() <= 0) {
    return;
  }
  if (!vnode_new->has_input_sequence_port()) {
    return;
  }

  undo_redo->create_action(TTR("Connect Node Sequence"));
  int pass_port = -vnode_old->get_output_sequence_port_count() + 1;
  int return_port = port_action_output - 1;
  if (vnode_old->get_output_value_port_info(port_action_output).name ==
          String("pass") &&
      !script->get_output_sequence_ports_connected(port_action_node)
           .has(pass_port)) {
    undo_redo->add_do_method(script.ptr(), "sequence_connect", port_action_node,
                             pass_port, new_id);
    undo_redo->add_undo_method(script.ptr(), "sequence_disconnect",
                               port_action_node, pass_port, new_id);
  } else if (vnode_old->get_output_value_port_info(port_action_output).name ==
                 String("return") &&
             !script->get_output_sequence_ports_connected(port_action_node)
                  .has(return_port)) {
    undo_redo->add_do_method(script.ptr(), "sequence_connect", port_action_node,
                             return_port, new_id);
    undo_redo->add_undo_method(script.ptr(), "sequence_disconnect",
                               port_action_node, return_port, new_id);
  } else {
    for (int port = 0; port < vnode_old->get_output_sequence_port_count();
         port++) {
      int count = vnode_old->get_output_sequence_port_count();
      if (port_action_output < count &&
          !script->get_output_sequence_ports_connected(port_action_node)
               .has(port_action_output)) {
        undo_redo->add_do_method(script.ptr(), "sequence_connect",
                                 port_action_node, port_action_output, new_id);
        undo_redo->add_undo_method(script.ptr(), "sequence_disconnect",
                                   port_action_node, port_action_output,
                                   new_id);
        break;
      } else if (!script->get_output_sequence_ports_connected(port_action_node)
                      .has(port)) {
        undo_redo->add_do_method(script.ptr(), "sequence_connect",
                                 port_action_node, port, new_id);
        undo_redo->add_undo_method(script.ptr(), "sequence_disconnect",
                                   port_action_node, port, new_id);
        break;
      }
    }
  }

  undo_redo->commit_action();
}

void VisualScriptEditor::_selected_new_virtual_method(const String &p_text,
                                                      const String &p_category,
                                                      const bool p_connecting) {
  String name = p_text.substr(p_text.find_char(':') + 1);
  if (script->has_function(name)) {
    EditorNode::get_singleton()->show_warning(
        vformat(TTR("Script already has function '%s'"), name));
    return;
  }

  MethodInfo minfo;
  {
    List<MethodInfo> methods;
    bool found = false;
    ClassDB::get_virtual_methods(script->get_instance_base_type(), &methods);
    for (const MethodInfo &E : methods) {
      if (E.name == name) {
        minfo = E;
        found = true;
      }
    }

    ERR_FAIL_COND(!found);
  }

  selected = name;
  Ref<VisualScriptFunction> func_node;
  func_node.instantiate();
  func_node->set_name(name);
  int fn_id = script->get_available_id();
  undo_redo->create_action(TTR("Add Function"));
  undo_redo->add_do_method(script.ptr(), "add_function", name, fn_id);

  for (int i = 0; i < minfo.arguments.size(); i++) {
    func_node->add_argument(minfo.arguments[i].type, minfo.arguments[i].name,
                            -1, minfo.arguments[i].hint,
                            minfo.arguments[i].hint_string);
  }

  Vector2 pos = _get_available_pos();

  undo_redo->add_do_method(script.ptr(), "add_node", fn_id, func_node, pos);
  undo_redo->add_undo_method(script.ptr(), "remove_node", fn_id);
  if (minfo.return_val.type != Variant::NIL ||
      minfo.return_val.usage & PROPERTY_USAGE_NIL_IS_VARIANT) {
    Ref<VisualScriptReturn> ret_node;
    ret_node.instantiate();
    ret_node->set_return_type(minfo.return_val.type);
    ret_node->set_enable_return_value(true);
    ret_node->set_name(name);
    int nid = script->get_available_id() + 1;
    undo_redo->add_do_method(script.ptr(), "add_node", nid, ret_node,
                             _get_available_pos(false, pos + Vector2(500, 0)));
    undo_redo->add_undo_method(script.ptr(), "remove_node", nid);
  }

  undo_redo->add_undo_method(script.ptr(), "remove_function", name);
  undo_redo->add_do_method(this, "_update_members");
  undo_redo->add_undo_method(this, "_update_members");
  undo_redo->add_do_method(this, "_update_graph");
  undo_redo->add_undo_method(this, "_update_graph");

  undo_redo->commit_action();

  _update_graph();
}

void VisualScriptEditor::_cancel_connect_node() {
  // Ensure the cancel is done.
  port_action_new_node = -1;
}

int VisualScriptEditor::_create_new_node_from_name(const String &p_text,
                                                   const Vector2 &p_point) {
  Ref<VisualScriptNode> vnode =
      VisualScriptLanguage::singleton->create_node_from_name(p_text);
  int new_id = script->get_available_id();
  undo_redo->create_action(TTR("Add Node"));
  undo_redo->add_do_method(script.ptr(), "add_node", new_id, vnode, p_point);
  undo_redo->add_undo_method(script.ptr(), "remove_node", new_id);
  undo_redo->add_do_method(this, "_update_graph");
  undo_redo->add_undo_method(this, "_update_graph");
  undo_redo->commit_action();
  return new_id;
}

void VisualScriptEditor::_default_value_changed(const StringName &p_property,
                                                const Variant &p_value,
                                                const String &p_field,
                                                bool p_changing) {
  Ref<VisualScriptNode> vsn = script->get_node(editing_id);
  if (vsn.is_null()) {
    return;
  }

  undo_redo->create_action(TTR("Change Input Value"));
  undo_redo->add_do_method(vsn.ptr(), "set_default_input_value", editing_input,
                           p_value);
  undo_redo->add_undo_method(vsn.ptr(), "set_default_input_value",
                             editing_input,
                             vsn->get_default_input_value(editing_input));

  undo_redo->add_do_method(this, "_update_graph", editing_id);
  undo_redo->add_undo_method(this, "_update_graph", editing_id);
  undo_redo->commit_action();
}

void VisualScriptEditor::_default_value_edited(Node *p_button, int p_id,
                                               int p_input_port) {
  Ref<VisualScriptNode> vsn = script->get_node(p_id);
  if (vsn.is_null()) {
    return;
  }

  PropertyInfo pinfo = vsn->get_input_value_port_info(p_input_port);
  Variant existing = vsn->get_default_input_value(p_input_port);
  if (pinfo.type != Variant::NIL && existing.get_type() != pinfo.type) {
    Callable::CallError ce;
    Variant e = existing;
    const Variant *existingp = &e;
    Variant::construct(pinfo.type, existing, &existingp, 1, ce);
  }

  if (pinfo.type == Variant::NODE_PATH) {
    Node *edited_scene = get_tree()->get_edited_scene_root();
    if (edited_scene) { // Fixing an old crash bug ( Visual Script Crashes on
                        // editing NodePath with an empty scene open).
      Node *script_node = _find_script_node(edited_scene, edited_scene, script);

      if (script_node) {
        // Pick a node relative to the script, IF the script exists.
        pinfo.hint = PROPERTY_HINT_NODE_PATH_TO_EDITED_NODE;
        pinfo.hint_string = script_node->get_path();
      } else {
        // Pick a path relative to edited scene.
        pinfo.hint = PROPERTY_HINT_NODE_PATH_TO_EDITED_NODE;
        pinfo.hint_string = get_tree()->get_edited_scene_root()->get_path();
      }
    }
  }

  edited_default_property_holder->set_edited_property(existing);

  if (default_property_editor) {
    default_property_editor->disconnect(
        "property_changed",
        callable_mp(this, &VisualScriptEditor::_default_value_changed));
    default_property_editor_popup->remove_child(default_property_editor);
  }

  default_property_editor = EditorInspector::instantiate_property_editor(
      edited_default_property_holder.ptr(), pinfo.type, "edited_property",
      pinfo.hint, pinfo.hint_string, PROPERTY_USAGE_NONE);
  if (default_property_editor) {
    default_property_editor->set_object_and_property(
        edited_default_property_holder.ptr(), "edited_property");
    default_property_editor->update_property();
    default_property_editor->set_name_split_ratio(0);
    default_property_editor_popup->add_child(default_property_editor);

    default_property_editor->connect(
        "property_changed",
        callable_mp(this, &VisualScriptEditor::_default_value_changed));

    Button *button = Object::cast_to<Button>(p_button);
    if (button) {
      default_property_editor_popup->set_position(
          button->get_screen_position() +
          Vector2(0, button->get_size().height) * graph->get_zoom());
    }

    default_property_editor_popup->reset_size();

    if (pinfo.hint == PROPERTY_HINT_MULTILINE_TEXT || !button) {
      default_property_editor_popup->popup_centered_ratio();
    } else {
      default_property_editor_popup->popup();
    }
  }

  editing_id = p_id;
  editing_input = p_input_port;
}

void VisualScriptEditor::_show_hint(const String &p_hint) {
  hint_text->set_text(p_hint);
  hint_text->show();
  hint_text_timer->start();
}

void VisualScriptEditor::_hide_timer() { hint_text->hide(); }

void VisualScriptEditor::_toggle_scripts_pressed() {
  ScriptEditor::get_singleton()->toggle_scripts_panel();
  update_toggle_scripts_button();
}

void VisualScriptEditor::_notification(int p_what) {
  switch (p_what) {
  case NOTIFICATION_ENTER_TREE:
  case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
    graph->get_panner()->setup((ViewPanner::ControlScheme)EDITOR_GET(
                                   "editors/panning/sub_editors_panning_scheme")
                                   .
                                   operator int(),
                               ED_GET_SHORTCUT("canvas_item_editor/pan_view"),
                               bool(EditorSettings::get_singleton()->get(
                                   "editors/panning/simple_panning")));
    graph->set_warped_panning(bool(EditorSettings::get_singleton()->get(
        "editors/panning/warped_mouse_panning")));
    graph->set_minimap_opacity(EditorSettings::get_singleton()->get(
        "editors/visual_editors/minimap_opacity"));
    graph->set_connection_lines_curvature(EditorSettings::get_singleton()->get(
        "editors/visual_editors/lines_curvature"));
    _update_graph();
  } break;

  case NOTIFICATION_READY: {
    variable_editor->connect(
        "changed", callable_mp(this, &VisualScriptEditor::_update_members));
    variable_editor->connect(
        "changed",
        callable_mp(this, &VisualScriptEditor::_update_graph).bind(-1),
        CONNECT_DEFERRED);
    signal_editor->connect(
        "changed", callable_mp(this, &VisualScriptEditor::_update_members));
    signal_editor->connect(
        "changed",
        callable_mp(this, &VisualScriptEditor::_update_graph).bind(-1),
        CONNECT_DEFERRED);
    [[fallthrough]];
  }
  case NOTIFICATION_THEME_CHANGED: {
    if (p_what != NOTIFICATION_READY && !is_visible_in_tree()) {
      return;
    }

    update_toggle_scripts_button();

    edit_variable_edit->add_theme_style_override(
        "bg", get_theme_stylebox(SNAME("bg"), SNAME("Tree")));
    edit_signal_edit->add_theme_style_override(
        "bg", get_theme_stylebox(SNAME("bg"), SNAME("Tree")));
    func_input_scroll->add_theme_style_override(
        "bg", get_theme_stylebox(SNAME("bg"), SNAME("Tree")));

    Ref<Theme> tm = EditorNode::get_singleton()->get_theme_base()->get_theme();

    bool dark_theme = tm->get_constant("dark_theme", "Editor");

    if (dark_theme) {
      node_colors["flow_control"] = Color(0.96, 0.96, 0.96);
      node_colors["functions"] = Color(0.96, 0.52, 0.51);
      node_colors["data"] = Color(0.5, 0.96, 0.81);
      node_colors["operators"] = Color(0.67, 0.59, 0.87);
      node_colors["custom"] = Color(0.5, 0.73, 0.96);
      node_colors["constants"] = Color(0.96, 0.5, 0.69);
    } else {
      node_colors["flow_control"] = Color(0.26, 0.26, 0.26);
      node_colors["functions"] = Color(0.95, 0.4, 0.38);
      node_colors["data"] = Color(0.07, 0.73, 0.51);
      node_colors["operators"] = Color(0.51, 0.4, 0.82);
      node_colors["custom"] = Color(0.31, 0.63, 0.95);
      node_colors["constants"] = Color(0.94, 0.18, 0.49);
    }

    for (const KeyValue<StringName, Color> &E : node_colors) {
      const Ref<StyleBoxFlat> sb =
          tm->get_stylebox(SNAME("frame"), SNAME("GraphNode"));

      if (!sb.is_null()) {
        Ref<StyleBoxFlat> frame_style = sb->duplicate();
        // Adjust the border color to be close to the GraphNode's background
        // color. This keeps the node's title area from being too distracting.
        Color color =
            dark_theme ? E.value.darkened(0.75) : E.value.lightened(0.75);
        color.a = 0.9;
        frame_style->set_border_color(color);
        node_styles[E.key] = frame_style;
      }
    }

    if (is_visible_in_tree() && script.is_valid()) {
      _update_members();
      _update_graph();
    }
  } break;

  case NOTIFICATION_VISIBILITY_CHANGED: {
    update_toggle_scripts_button();
    members_section->set_visible(is_visible_in_tree());
  } break;
  }
}

void VisualScriptEditor::_graph_ofs_changed(const Vector2 &p_ofs) {
  if (updating_graph || !script.is_valid()) {
    return;
  }

  updating_graph = true;

  script->set_scroll(graph->get_scroll_ofs() / EDSCALE);
  script->set_edited(true);
  updating_graph = false;
}

void VisualScriptEditor::_comment_node_resized(const Vector2 &p_new_size,
                                               int p_node) {
  if (updating_graph) {
    return;
  }
  Ref<VisualScriptComment> vsc = script->get_node(p_node);
  if (vsc.is_null()) {
    return;
  }

  Node *node = graph->get_node(itos(p_node));
  GraphNode *gn = Object::cast_to<GraphNode>(node);
  if (!gn) {
    return;
  }

  Vector2 new_size = p_new_size;
  if (graph->is_using_snap()) {
    Vector2 snap = Vector2(graph->get_snap(), graph->get_snap());
    Vector2 min_size = (gn->get_minimum_size() + (snap * 0.5)).snapped(snap);
    new_size = new_size.snapped(snap).max(min_size);
  }

  updating_graph = true;

  graph->set_block_minimum_size_adjust(true); // faster resize

  undo_redo->create_action(TTR("Resize Comment"), UndoRedo::MERGE_ENDS);
  undo_redo->add_do_method(vsc.ptr(), "set_size", new_size / EDSCALE);
  undo_redo->add_undo_method(vsc.ptr(), "set_size", vsc->get_size());
  undo_redo->commit_action();

  gn->set_custom_minimum_size(new_size);
  gn->reset_size();
  graph->set_block_minimum_size_adjust(false);
  updating_graph = false;
}

void VisualScriptEditor::_menu_option(int p_what) {
  switch (p_what) {
  case EDIT_ADD_NODE: {
    _generic_search();
  } break;
  case EDIT_DELETE_NODES: {
    _on_nodes_delete();
  } break;
  case EDIT_TOGGLE_BREAKPOINT: {
    List<String> reselect;
    for (int i = 0; i < graph->get_child_count(); i++) {
      GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
      if (gn) {
        if (gn->is_selected()) {
          int id = String(gn->get_name()).to_int();
          Ref<VisualScriptNode> vsn = script->get_node(id);
          if (vsn.is_valid()) {
            vsn->set_breakpoint(!vsn->is_breakpoint());
            reselect.push_back(gn->get_name());
          }
        }
      }
    }

    _update_graph();

    for (const String &E : reselect) {
      GraphNode *gn = Object::cast_to<GraphNode>(graph->get_node(E));
      gn->set_selected(true);
    }

  } break;
  case EDIT_FIND_NODE_TYPE: {
    _generic_search();
  } break;
  case EDIT_COPY_NODES: {
    _on_nodes_copy();
  } break;
  case EDIT_CUT_NODES: {
    _on_nodes_copy();
    _on_nodes_delete();
  } break;
  case EDIT_PASTE_NODES: {
    _on_nodes_paste();
  } break;
  case EDIT_DUPLICATE_NODES: {
    _on_nodes_duplicate();
  } break;
  case EDIT_CREATE_FUNCTION: {
    // Create Function.
    HashMap<int, Ref<VisualScriptNode>> nodes;
    RBSet<int> selections;
    for (int i = 0; i < graph->get_child_count(); i++) {
      GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
      if (gn) {
        if (gn->is_selected()) {
          int id = String(gn->get_name()).to_int();
          Ref<VisualScriptNode> node = script->get_node(id);
          if (Object::cast_to<VisualScriptFunction>(*node)) {
            EditorNode::get_singleton()->show_warning(
                TTR("Can't create function with a function node."));
            return;
          }
          if (node.is_valid()) {
            nodes.insert(id, node);
            selections.insert(id);
          }
        }
      }
    }

    if (nodes.size() == 0) {
      return; // nothing to be done if there are no valid nodes selected
    }

    RBSet<VisualScript::SequenceConnection> seqmove;
    RBSet<VisualScript::DataConnection> datamove;

    RBSet<VisualScript::SequenceConnection> seqext;
    RBSet<VisualScript::DataConnection> dataext;

    int start_node = -1;
    RBSet<int> end_nodes;
    if (nodes.size() == 1) {
      Ref<VisualScriptNode> nd = script->get_node(nodes.begin()->key);
      if (nd.is_valid() && nd->has_input_sequence_port()) {
        start_node = nodes.begin()->key;
      } else {
        EditorNode::get_singleton()->show_warning(
            TTR("Select at least one node with sequence port."));
        return;
      }
    } else {
      List<VisualScript::SequenceConnection> seqs;
      script->get_sequence_connection_list(&seqs);

      if (seqs.size() == 0) {
        // In case there are no sequence connections,
        // select the top most node cause that's probably how,
        // the user wants to connect the nodes.
        int top_nd = -1;
        Vector2 top;
        for (const KeyValue<int, Ref<VisualScriptNode>> &E : nodes) {
          Ref<VisualScriptNode> nd = script->get_node(E.key);
          if (nd.is_valid() && nd->has_input_sequence_port()) {
            if (top_nd < 0) {
              top_nd = E.key;
              top = script->get_node_position(top_nd);
            }
            Vector2 pos = script->get_node_position(E.key);
            if (top.y > pos.y) {
              top_nd = E.key;
              top = pos;
            }
          }
        }
        Ref<VisualScriptNode> nd = script->get_node(top_nd);
        if (nd.is_valid() && nd->has_input_sequence_port()) {
          start_node = top_nd;
        } else {
          EditorNode::get_singleton()->show_warning(
              TTR("Select at least one node with sequence port."));
          return;
        }
      } else {
        // Pick the node with input sequence.
        RBSet<int> nodes_from;
        RBSet<int> nodes_to;
        for (const VisualScript::SequenceConnection &E : seqs) {
          if (nodes.has(E.from_node) && nodes.has(E.to_node)) {
            seqmove.insert(E);
            nodes_from.insert(E.from_node);
          } else if (nodes.has(E.from_node) && !nodes.has(E.to_node)) {
            seqext.insert(E);
          } else if (!nodes.has(E.from_node) && nodes.has(E.to_node)) {
            if (start_node == -1) {
              seqext.insert(E);
              start_node = E.to_node;
            } else {
              EditorNode::get_singleton()->show_warning(
                  TTR("Try to only have one sequence input in selection."));
              return;
            }
          }
          nodes_to.insert(E.to_node);
        }

        // To use to add return nodes.
        _get_ends(start_node, seqs, selections, end_nodes);

        if (start_node == -1) {
          // If we still don't have a start node then,
          // run through the nodes and select the first tree node,
          // i.e. node without any input sequence but output sequence.
          for (const int &E : nodes_from) {
            if (!nodes_to.has(E)) {
              start_node = E;
            }
          }
        }
      }
    }

    if (start_node == -1) {
      return; // This should not happen, but just in case something goes wrong.
    }

    List<Variant::Type> inputs; // input types
    List<Pair<int, int>> input_connections;
    {
      List<VisualScript::DataConnection> dats;
      script->get_data_connection_list(&dats);
      for (const VisualScript::DataConnection &E : dats) {
        if (nodes.has(E.from_node) && nodes.has(E.to_node)) {
          datamove.insert(E);
        } else if (!nodes.has(E.from_node) && nodes.has(E.to_node)) {
          // Add all these as inputs for the Function.
          Ref<VisualScriptNode> node = script->get_node(E.to_node);
          if (node.is_valid()) {
            dataext.insert(E);
            PropertyInfo pi = node->get_input_value_port_info(E.to_port);
            inputs.push_back(pi.type);
            input_connections.push_back(Pair<int, int>(E.to_node, E.to_port));
          }
        } else if (nodes.has(E.from_node) && !nodes.has(E.to_node)) {
          dataext.insert(E);
        }
      }
    }
    int fn_id = script->get_available_id();
    {
      String new_fn = _validate_name("new_function");

      Vector2 pos = _get_available_pos(
          false, script->get_node_position(start_node) - Vector2(80, 150));

      Ref<VisualScriptFunction> func_node;
      func_node.instantiate();
      func_node->set_name(new_fn);

      undo_redo->create_action(TTR("Create Function"));

      undo_redo->add_do_method(script.ptr(), "add_function", new_fn, fn_id);
      undo_redo->add_do_method(script.ptr(), "add_node", fn_id, func_node, pos);
      undo_redo->add_undo_method(script.ptr(), "remove_function", new_fn);
      undo_redo->add_undo_method(script.ptr(), "remove_node", fn_id);
      undo_redo->add_do_method(this, "_update_members");
      undo_redo->add_undo_method(this, "_update_members");
      undo_redo->add_do_method(this, "emit_signal", "edited_script_changed");
      undo_redo->add_undo_method(this, "emit_signal", "edited_script_changed");
      // Might make the system more intelligent by checking port from info.
      int i = 0;
      List<Pair<int, int>>::Element *F = input_connections.front();
      for (List<Variant::Type>::Element *E = inputs.front(); E && F;
           E = E->next(), F = F->next()) {
        func_node->add_argument(E->get(), "arg_" + String::num_int64(i), i);
        undo_redo->add_do_method(script.ptr(), "data_connect", fn_id, i,
                                 F->get().first, F->get().second);
        i++; // increment i
      }
      // Ensure Preview Selection is of newly created function node.
      if (selections.size()) {
        EditorNode::get_singleton()->push_item(func_node.ptr());
      }
    }
    // Move the nodes.

    // Handles reconnection of sequence connections on undo, start here in case
    // of issues.
    for (const VisualScript::SequenceConnection &E : seqext) {
      undo_redo->add_do_method(script.ptr(), "sequence_disconnect", E.from_node,
                               E.from_output, E.to_node);
      undo_redo->add_undo_method(script.ptr(), "sequence_connect", E.from_node,
                                 E.from_output, E.to_node);
    }
    for (const VisualScript::DataConnection &E : dataext) {
      undo_redo->add_do_method(script.ptr(), "data_disconnect", E.from_node,
                               E.from_port, E.to_node, E.to_port);
      undo_redo->add_undo_method(script.ptr(), "data_connect", E.from_node,
                                 E.from_port, E.to_node, E.to_port);
    }

    // I don't really think we need support for non sequenced functions at this
    // moment.
    undo_redo->add_do_method(script.ptr(), "sequence_connect", fn_id, 0,
                             start_node);

    // Could fail with the new changes, start here when searching for bugs in
    // create function shortcut.
    int m = 1;
    for (const int &G : end_nodes) {
      Ref<VisualScriptReturn> ret_node;
      ret_node.instantiate();

      int ret_id = fn_id + (m++);
      selections.insert(ret_id);
      Vector2 posi = _get_available_pos(false, script->get_node_position(G) +
                                                   Vector2(80, -100));
      undo_redo->add_do_method(script.ptr(), "add_node", ret_id, ret_node,
                               posi);
      undo_redo->add_undo_method(script.ptr(), "remove_node", ret_id);

      undo_redo->add_do_method(script.ptr(), "sequence_connect", G, 0, ret_id);
      // Add data outputs from each of the end_nodes.
      Ref<VisualScriptNode> vsn = script->get_node(G);
      if (vsn.is_valid() && vsn->get_output_value_port_count() > 0) {
        ret_node->set_enable_return_value(true);
        // Use the zeroth data port cause that's the likely one that is planned
        // to be used.
        ret_node->set_return_type(vsn->get_output_value_port_info(0).type);
        undo_redo->add_do_method(script.ptr(), "data_connect", G, 0, ret_id, 0);
      }
    }

    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");

    undo_redo->commit_action();

    // Make sure all Nodes get marked for selection so that they can be moved
    // together.
    selections.insert(fn_id);
    for (int k = 0; k < graph->get_child_count(); k++) {
      GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(k));
      if (gn) {
        int id = gn->get_name().operator String().to_int();
        gn->set_selected(selections.has(id));
      }
    }

  } break;
  case REFRESH_GRAPH: {
    _update_graph();
  } break;
  case EDIT_CLEAR_COPY_BUFFER: {
    clipboard->nodes.clear();
    clipboard->nodes_positions.clear();
    clipboard->data_connections.clear();
    clipboard->sequence_connections.clear();
  } break;
  }
}

// This is likely going to be very slow and I am not sure if I should keep it,
// but I hope that it will not be a problem considering that we won't be
// creating functions so frequently, and cyclic connections would be a problem
// but hopefully we won't let them get to this point.
void VisualScriptEditor::_get_ends(
    int p_node, const List<VisualScript::SequenceConnection> &p_seqs,
    const RBSet<int> &p_selected, RBSet<int> &r_end_nodes) {
  for (const VisualScript::SequenceConnection &E : p_seqs) {
    int from = E.from_node;
    int to = E.to_node;

    if (from == p_node && p_selected.has(to)) {
      // This is an interior connection move forward to the to node.
      _get_ends(to, p_seqs, p_selected, r_end_nodes);
    } else if (from == p_node && !p_selected.has(to)) {
      r_end_nodes.insert(from);
    }
  }
}

void VisualScriptEditor::_member_rmb_selected(const Vector2 &p_pos,
                                              MouseButton p_button) {
  if (p_button != MouseButton::RIGHT) {
    return;
  }

  TreeItem *ti = members->get_selected();
  ERR_FAIL_COND(!ti);

  member_popup->clear();
  member_popup->set_position(members->get_screen_position() + p_pos);
  member_popup->reset_size();

  function_name_edit->set_position(members->get_screen_position() + p_pos);
  function_name_edit->reset_size();

  TreeItem *root = members->get_root();

  Ref<Texture2D> del_icon =
      Control::get_theme_icon(SNAME("Remove"), SNAME("EditorIcons"));

  Ref<Texture2D> edit_icon =
      Control::get_theme_icon(SNAME("Edit"), SNAME("EditorIcons"));

  if (ti->get_parent() == root->get_first_child()) {
    member_type = MEMBER_FUNCTION;
    member_name = ti->get_text(0);
    member_popup->add_icon_shortcut(
        edit_icon, ED_GET_SHORTCUT("visual_script_editor/edit_member"),
        MEMBER_EDIT);
    member_popup->add_separator();
    member_popup->add_icon_shortcut(
        del_icon, ED_GET_SHORTCUT("ui_graph_delete"), MEMBER_REMOVE);
    member_popup->popup();
    return;
  }

  if (ti->get_parent() == root->get_first_child()->get_next()) {
    member_type = MEMBER_VARIABLE;
    member_name = ti->get_text(0);
    member_popup->add_icon_shortcut(
        edit_icon, ED_GET_SHORTCUT("visual_script_editor/edit_member"),
        MEMBER_EDIT);
    member_popup->add_separator();
    member_popup->add_icon_shortcut(
        del_icon, ED_GET_SHORTCUT("ui_graph_delete"), MEMBER_REMOVE);
    member_popup->popup();
    return;
  }

  if (ti->get_parent() == root->get_first_child()->get_next()->get_next()) {
    member_type = MEMBER_SIGNAL;
    member_name = ti->get_text(0);
    member_popup->add_icon_shortcut(
        edit_icon, ED_GET_SHORTCUT("visual_script_editor/edit_member"),
        MEMBER_EDIT);
    member_popup->add_separator();
    member_popup->add_icon_shortcut(
        del_icon, ED_GET_SHORTCUT("ui_graph_delete"), MEMBER_REMOVE);
    member_popup->popup();
    return;
  }
}

void VisualScriptEditor::_member_option(int p_option) {
  switch (member_type) {
  case MEMBER_FUNCTION: {
    if (p_option == MEMBER_REMOVE) {
      // Delete the function.
      String name = member_name;
      List<String> lst;
      int fn_node = script->get_function_node_id(name);
      undo_redo->create_action(TTR("Remove Function"));
      undo_redo->add_do_method(script.ptr(), "remove_function", name);
      undo_redo->add_do_method(script.ptr(), "remove_node", fn_node);
      undo_redo->add_undo_method(script.ptr(), "add_function", name, fn_node);
      undo_redo->add_undo_method(script.ptr(), "add_node", fn_node,
                                 script->get_node(fn_node),
                                 script->get_node_position(fn_node));
      List<VisualScript::SequenceConnection> seqcons;
      script->get_sequence_connection_list(&seqcons);
      for (const VisualScript::SequenceConnection &E : seqcons) {
        if (E.from_node == fn_node) {
          undo_redo->add_undo_method(script.ptr(), "sequence_connect", fn_node,
                                     E.from_output, E.to_node);
        }
      }
      List<VisualScript::DataConnection> datcons;
      script->get_data_connection_list(&datcons);
      for (const VisualScript::DataConnection &E : datcons) {
        if (E.from_node == fn_node) {
          undo_redo->add_undo_method(script.ptr(), "data_connect", fn_node,
                                     E.from_port, E.to_node, E.to_port);
        }
      }
      undo_redo->add_do_method(this, "_update_members");
      undo_redo->add_undo_method(this, "_update_members");
      undo_redo->add_do_method(this, "_update_graph");
      undo_redo->add_undo_method(this, "_update_graph");
      undo_redo->commit_action();
    } else if (p_option == MEMBER_EDIT) {
      selected = members->get_selected()->get_text(0);
      function_name_edit->popup();
      function_name_box->set_text(selected);
      function_name_box->select_all();
      function_name_box->grab_focus();
    }
  } break;
  case MEMBER_VARIABLE: {
    String name = member_name;

    if (p_option == MEMBER_REMOVE) {
      undo_redo->create_action(TTR("Remove Variable"));
      undo_redo->add_do_method(script.ptr(), "remove_variable", name);
      undo_redo->add_undo_method(script.ptr(), "add_variable", name,
                                 script->get_variable_default_value(name));
      undo_redo->add_undo_method(
          script.ptr(), "set_variable_info", name,
          script->call("get_variable_info", name)); // return as dict
      undo_redo->add_do_method(this, "_update_members");
      undo_redo->add_undo_method(this, "_update_members");
      undo_redo->commit_action();
    } else if (p_option == MEMBER_EDIT) {
      variable_editor->edit(name);
      edit_variable_dialog->set_title(TTR("Editing Variable:") + " " + name);
      edit_variable_dialog->popup_centered(Size2(400, 200) * EDSCALE);
    }
  } break;
  case MEMBER_SIGNAL: {
    String name = member_name;

    if (p_option == MEMBER_REMOVE) {
      undo_redo->create_action(TTR("Remove Signal"));
      undo_redo->add_do_method(script.ptr(), "remove_custom_signal", name);
      undo_redo->add_undo_method(script.ptr(), "add_custom_signal", name);

      for (int i = 0; i < script->custom_signal_get_argument_count(name); i++) {
        undo_redo->add_undo_method(
            script.ptr(), "custom_signal_add_argument", name,
            script->custom_signal_get_argument_name(name, i),
            script->custom_signal_get_argument_type(name, i));
      }

      undo_redo->add_do_method(this, "_update_members");
      undo_redo->add_undo_method(this, "_update_members");
      undo_redo->commit_action();
    } else if (p_option == MEMBER_EDIT) {
      signal_editor->edit(name);
      edit_signal_dialog->set_title(TTR("Editing Signal:") + " " + name);
      edit_signal_dialog->popup_centered(Size2(400, 300) * EDSCALE);
    }
  } break;
  }
}

void VisualScriptEditor::add_syntax_highlighter(
    Ref<EditorSyntaxHighlighter> p_highlighter) {}

void VisualScriptEditor::set_syntax_highlighter(
    Ref<EditorSyntaxHighlighter> p_highlighter) {}

void VisualScriptEditor::update_toggle_scripts_button() {
  if (is_layout_rtl()) {
    toggle_scripts_button->set_icon(Control::get_theme_icon(
        ScriptEditor::get_singleton()->is_scripts_panel_toggled()
            ? SNAME("Forward")
            : SNAME("Back"),
        SNAME("EditorIcons")));
  } else {
    toggle_scripts_button->set_icon(Control::get_theme_icon(
        ScriptEditor::get_singleton()->is_scripts_panel_toggled()
            ? SNAME("Back")
            : SNAME("Forward"),
        SNAME("EditorIcons")));
  }
  toggle_scripts_button->set_tooltip_text(vformat(
      "%s (%s)", TTR("Toggle Scripts Panel"),
      ED_GET_SHORTCUT("script_editor/toggle_scripts_panel")->get_as_text()));
}

void VisualScriptEditor::_bind_methods() {
  ClassDB::bind_method("_move_node", &VisualScriptEditor::_move_node);
  ClassDB::bind_method("_update_graph", &VisualScriptEditor::_update_graph,
                       DEFVAL(-1));

  ClassDB::bind_method("_center_on_node", &VisualScriptEditor::_center_on_node);
  ClassDB::bind_method("_button_resource_previewed",
                       &VisualScriptEditor::_button_resource_previewed);
  ClassDB::bind_method("_port_action_menu",
                       &VisualScriptEditor::_port_action_menu);

  ClassDB::bind_method("_create_new_node_from_name",
                       &VisualScriptEditor::_create_new_node_from_name);

  ClassDB::bind_method("_get_drag_data_fw",
                       &VisualScriptEditor::get_drag_data_fw);
  ClassDB::bind_method("_can_drop_data_fw",
                       &VisualScriptEditor::can_drop_data_fw);
  ClassDB::bind_method("_drop_data_fw", &VisualScriptEditor::drop_data_fw);

  ClassDB::bind_method("_update_graph_connections",
                       &VisualScriptEditor::_update_graph_connections);
  ClassDB::bind_method("_update_members", &VisualScriptEditor::_update_members);

  ClassDB::bind_method("_generic_search", &VisualScriptEditor::_generic_search);
}

VisualScriptEditor::VisualScriptEditor() {
  if (!clipboard) {
    clipboard = memnew(Clipboard);
  }

  edit_menu = memnew(MenuButton);
  edit_menu->set_shortcut_context(this);
  edit_menu->set_text(TTR("Edit"));
  edit_menu->set_switch_on_hover(true);
  edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_graph_delete"),
                                       EDIT_DELETE_NODES);
  edit_menu->get_popup()->add_shortcut(
      ED_GET_SHORTCUT("visual_script_editor/toggle_breakpoint"),
      EDIT_TOGGLE_BREAKPOINT);
  edit_menu->get_popup()->add_shortcut(
      ED_GET_SHORTCUT("visual_script_editor/find_node_type"),
      EDIT_FIND_NODE_TYPE);
  edit_menu->get_popup()->add_separator();
  edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_copy"),
                                       EDIT_COPY_NODES);
  edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_cut"),
                                       EDIT_CUT_NODES);
  edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_paste"),
                                       EDIT_PASTE_NODES);
  edit_menu->get_popup()->add_separator();
  edit_menu->get_popup()->add_shortcut(
      ED_GET_SHORTCUT("visual_script_editor/create_function"),
      EDIT_CREATE_FUNCTION);
  edit_menu->get_popup()->add_shortcut(
      ED_GET_SHORTCUT("visual_script_editor/refresh_nodes"), REFRESH_GRAPH);
  edit_menu->get_popup()->connect(
      "id_pressed", callable_mp(this, &VisualScriptEditor::_menu_option));

  members_section = memnew(VBoxContainer);
  // Add but wait until done setting up this.
  ScriptEditor::get_singleton()->get_left_list_split()->call_deferred(
      SNAME("add_child"), members_section);
  members_section->set_v_size_flags(SIZE_EXPAND_FILL);

  CheckButton *tool_script_check = memnew(CheckButton);
  tool_script_check->set_text(TTR("Make Tool:"));
  members_section->add_child(tool_script_check);
  tool_script_check->connect(
      "pressed", callable_mp(this, &VisualScriptEditor::_toggle_tool_script));

  ///       Members        ///

  members = memnew(Tree);
  members_section->add_margin_child(TTR("Members:"), members, true);
  members->set_custom_minimum_size(Size2(0, 50 * EDSCALE));
  members->set_hide_root(true);
  members->connect("button_clicked",
                   callable_mp(this, &VisualScriptEditor::_member_button));
  members->connect("item_edited",
                   callable_mp(this, &VisualScriptEditor::_member_edited));
  members->connect("cell_selected",
                   callable_mp(this, &VisualScriptEditor::_member_selected),
                   CONNECT_DEFERRED);
  members->connect("gui_input",
                   callable_mp(this, &VisualScriptEditor::_members_gui_input));
  members->connect(
      "item_mouse_selected",
      callable_mp(this, &VisualScriptEditor::_member_rmb_selected));
  members->set_allow_rmb_select(true);
  members->set_allow_reselect(true);
  members->set_hide_folding(true);
  members->set_drag_forwarding(this);

  member_popup = memnew(PopupMenu);
  add_child(member_popup);
  member_popup->connect("id_pressed",
                        callable_mp(this, &VisualScriptEditor::_member_option));

  function_name_edit = memnew(AcceptDialog);
  function_name_edit->set_title(TTR("Rename Function"));
  function_name_box = memnew(LineEdit);
  function_name_edit->add_child(function_name_box);
  function_name_box->connect(
      "gui_input", callable_mp(this, &VisualScriptEditor::_fn_name_box_input));
  function_name_edit->get_ok_button()->connect(
      "pressed",
      callable_mp(this, &VisualScriptEditor::_on_fn_name_box_confirmed));
  function_name_box->set_expand_to_text_length_enabled(true);
  add_child(function_name_edit);

  ///       Actual Graph          ///

  graph = memnew(GraphEdit);
  add_child(graph);
  graph->set_v_size_flags(Control::SIZE_EXPAND_FILL);
  graph->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
  graph->set_show_zoom_label(true);
  graph->connect("node_selected",
                 callable_mp(this, &VisualScriptEditor::_node_selected));
  graph->connect("begin_node_move",
                 callable_mp(this, &VisualScriptEditor::_begin_node_move));
  graph->connect("end_node_move",
                 callable_mp(this, &VisualScriptEditor::_end_node_move));
  graph->connect("copy_nodes_request",
                 callable_mp(this, &VisualScriptEditor::_on_nodes_copy));
  graph->connect("paste_nodes_request",
                 callable_mp(this, &VisualScriptEditor::_on_nodes_paste));
  graph->connect("delete_nodes_request",
                 callable_mp(this, &VisualScriptEditor::_on_nodes_delete));
  graph->connect("duplicate_nodes_request",
                 callable_mp(this, &VisualScriptEditor::_on_nodes_duplicate));
  graph->connect("gui_input",
                 callable_mp(this, &VisualScriptEditor::_graph_gui_input));
  graph->set_drag_forwarding(this);
  float graph_minimap_opacity = EditorSettings::get_singleton()->get(
      "editors/visual_editors/minimap_opacity");
  graph->set_minimap_opacity(graph_minimap_opacity);
  float graph_lines_curvature = EditorSettings::get_singleton()->get(
      "editors/visual_editors/lines_curvature");
  graph->set_connection_lines_curvature(graph_lines_curvature);
  graph->hide();
  graph->connect("scroll_offset_changed",
                 callable_mp(this, &VisualScriptEditor::_graph_ofs_changed));

  status_bar = memnew(HBoxContainer);
  add_child(status_bar);
  status_bar->set_h_size_flags(SIZE_EXPAND_FILL);
  status_bar->set_custom_minimum_size(Size2(0, 24 * EDSCALE));

  toggle_scripts_button = memnew(Button);
  toggle_scripts_button->set_flat(true);
  toggle_scripts_button->connect(
      "pressed",
      callable_mp(this, &VisualScriptEditor::_toggle_scripts_pressed));
  status_bar->add_child(toggle_scripts_button);

  /// Add Buttons to Top Bar/Zoom bar.
  HBoxContainer *graph_hbc = graph->get_zoom_hbox();

  Label *base_lbl = memnew(Label);
  base_lbl->set_text(TTR("Change Base Type:") + " ");
  graph_hbc->add_child(base_lbl);

  base_type_select = memnew(Button);
  base_type_select->connect(
      "pressed", callable_mp(this, &VisualScriptEditor::_change_base_type));
  graph_hbc->add_child(base_type_select);

  Button *add_nds = memnew(Button);
  add_nds->set_text(TTR("Add Nodes..."));
  graph_hbc->add_child(add_nds);
  add_nds->connect("pressed",
                   callable_mp(this, &VisualScriptEditor::_add_node_dialog));

  Button *fn_btn = memnew(Button);
  fn_btn->set_text(TTR("Add Function..."));
  graph_hbc->add_child(fn_btn);
  fn_btn->connect(
      "pressed",
      callable_mp(this, &VisualScriptEditor::_create_function_dialog));

  // Add Function Dialog.
  VBoxContainer *function_vb = memnew(VBoxContainer);
  function_vb->set_v_size_flags(SIZE_EXPAND_FILL);
  function_vb->set_custom_minimum_size(Size2(450, 300) * EDSCALE);

  HBoxContainer *func_name_hbox = memnew(HBoxContainer);
  function_vb->add_child(func_name_hbox);

  Label *func_name_label = memnew(Label);
  func_name_label->set_text(TTR("Name:"));
  func_name_hbox->add_child(func_name_label);

  func_name_box = memnew(LineEdit);
  func_name_box->set_h_size_flags(SIZE_EXPAND_FILL);
  func_name_box->set_placeholder(TTR("function_name"));
  func_name_box->set_text("");
  func_name_box->connect(
      "focus_entered",
      callable_mp(this, &VisualScriptEditor::_deselect_input_names));
  func_name_hbox->add_child(func_name_box);

  // Add minor setting for function if needed, here!

  function_vb->add_child(memnew(HSeparator));

  Button *add_input_button = memnew(Button);
  add_input_button->set_h_size_flags(SIZE_EXPAND_FILL);
  add_input_button->set_text(TTR("Add Input"));
  add_input_button->connect(
      "pressed", callable_mp(this, &VisualScriptEditor::_add_func_input));
  function_vb->add_child(add_input_button);

  func_input_scroll = memnew(ScrollContainer);
  func_input_scroll->set_v_size_flags(SIZE_EXPAND_FILL);
  function_vb->add_child(func_input_scroll);

  func_input_vbox = memnew(VBoxContainer);
  func_input_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
  func_input_scroll->add_child(func_input_vbox);

  function_create_dialog = memnew(ConfirmationDialog);
  function_create_dialog->set_title(TTR("Create Function"));
  function_create_dialog->add_child(function_vb);
  function_create_dialog->set_ok_button_text(TTR("Create"));
  function_create_dialog->get_ok_button()->connect(
      "pressed", callable_mp(this, &VisualScriptEditor::_create_function));
  add_child(function_create_dialog);

  select_func_text = memnew(Label);
  select_func_text->set_text(
      TTR("Select or create a function to edit its graph."));
  select_func_text->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
  select_func_text->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
  select_func_text->set_h_size_flags(SIZE_EXPAND_FILL);
  add_child(select_func_text);

  hint_text = memnew(Label);
  hint_text->set_anchor_and_offset(SIDE_TOP, ANCHOR_END, -100);
  hint_text->set_anchor_and_offset(SIDE_BOTTOM, ANCHOR_END, 0);
  hint_text->set_anchor_and_offset(SIDE_RIGHT, ANCHOR_END, 0);
  hint_text->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
  hint_text->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
  graph->add_child(hint_text);

  hint_text_timer = memnew(Timer);
  hint_text_timer->set_wait_time(4);
  hint_text_timer->connect("timeout",
                           callable_mp(this, &VisualScriptEditor::_hide_timer));
  add_child(hint_text_timer);

  // Allowed casts (connections).
  for (int i = 0; i < Variant::VARIANT_MAX; i++) {
    graph->add_valid_connection_type(Variant::NIL, i);
    graph->add_valid_connection_type(i, Variant::NIL);
    for (int j = 0; j < Variant::VARIANT_MAX; j++) {
      if (Variant::can_convert(Variant::Type(i), Variant::Type(j))) {
        graph->add_valid_connection_type(i, j);
      }
    }

    graph->add_valid_right_disconnect_type(i);
  }

  graph->add_valid_left_disconnect_type(TYPE_SEQUENCE);

  graph->connect("connection_request",
                 callable_mp(this, &VisualScriptEditor::_graph_connected));
  graph->connect("disconnection_request",
                 callable_mp(this, &VisualScriptEditor::_graph_disconnected));
  graph->connect(
      "connection_to_empty",
      callable_mp(this, &VisualScriptEditor::_graph_connect_to_empty));

  edit_signal_dialog = memnew(AcceptDialog);
  edit_signal_dialog->set_ok_button_text(TTR("Close"));
  add_child(edit_signal_dialog);

  signal_editor = memnew(VisualScriptEditorSignalEdit);
  edit_signal_edit = memnew(EditorInspector);
  edit_signal_dialog->add_child(edit_signal_edit);

  edit_signal_edit->edit(signal_editor);

  edit_variable_dialog = memnew(AcceptDialog);
  edit_variable_dialog->set_ok_button_text(TTR("Close"));
  add_child(edit_variable_dialog);

  variable_editor = memnew(VisualScriptEditorVariableEdit);
  edit_variable_edit = memnew(EditorInspector);
  edit_variable_dialog->add_child(edit_variable_edit);

  edit_variable_edit->edit(variable_editor);

  select_base_type = memnew(CreateDialog);
  select_base_type->set_base_type("Object"); // Anything goes.
  select_base_type->connect(
      "create",
      callable_mp(this, &VisualScriptEditor::_change_base_type_callback));
  add_child(select_base_type);

  undo_redo = EditorNode::get_singleton()->get_undo_redo();

  set_process_input(true);

  default_property_editor_popup = memnew(PopupPanel);
  default_property_editor_popup->set_min_size(Size2i(180, 0) * EDSCALE);
  add_child(default_property_editor_popup);

  edited_default_property_holder.instantiate();

  new_connect_node_select = memnew(VisualScriptPropertySelector);
  add_child(new_connect_node_select);
  new_connect_node_select->connect(
      "selected",
      callable_mp(this, &VisualScriptEditor::_selected_connect_node));
  new_connect_node_select->get_cancel_button()->connect(
      "pressed", callable_mp(this, &VisualScriptEditor::_cancel_connect_node));

  new_virtual_method_select = memnew(VisualScriptPropertySelector);
  add_child(new_virtual_method_select);
  new_virtual_method_select->connect(
      "selected",
      callable_mp(this, &VisualScriptEditor::_selected_new_virtual_method));

  popup_menu = memnew(PopupMenu);
  add_child(popup_menu);
  popup_menu->add_item(TTR("Add Node"), EDIT_ADD_NODE);
  popup_menu->add_separator();
  popup_menu->add_item(TTR("Cut"), EDIT_CUT_NODES);
  popup_menu->add_item(TTR("Copy"), EDIT_COPY_NODES);
  popup_menu->add_item(TTR("Paste"), EDIT_PASTE_NODES);
  popup_menu->add_item(TTR("Delete"), EDIT_DELETE_NODES);
  popup_menu->add_item(TTR("Duplicate"), EDIT_DUPLICATE_NODES);
  popup_menu->add_item(TTR("Clear Copy Buffer"), EDIT_CLEAR_COPY_BUFFER);
  popup_menu->connect("id_pressed",
                      callable_mp(this, &VisualScriptEditor::_menu_option));

  base_type_map.insert("String", Variant::STRING);
  base_type_map.insert("Vector2", Variant::VECTOR2);
  base_type_map.insert("Vector2i", Variant::VECTOR2I);
  base_type_map.insert("Rect2", Variant::RECT2);
  base_type_map.insert("Rect2i", Variant::RECT2I);
  base_type_map.insert("Vector3", Variant::VECTOR3);
  base_type_map.insert("Vector3i", Variant::VECTOR3I);
  base_type_map.insert("Vector4", Variant::VECTOR4);
  base_type_map.insert("Vector4i", Variant::VECTOR4I);
  base_type_map.insert("Transform2D", Variant::TRANSFORM2D);
  base_type_map.insert("Plane", Variant::PLANE);
  base_type_map.insert("Quaternion", Variant::QUATERNION);
  base_type_map.insert("AABB", Variant::AABB);
  base_type_map.insert("Basis", Variant::BASIS);
  base_type_map.insert("Transform3D", Variant::TRANSFORM3D);
  base_type_map.insert("Projection", Variant::PROJECTION);
  base_type_map.insert("Color", Variant::COLOR);
  base_type_map.insert("NodePath", Variant::NODE_PATH);
  base_type_map.insert("RID", Variant::RID);
  base_type_map.insert("Callable", Variant::CALLABLE);
  base_type_map.insert("Dictionary", Variant::DICTIONARY);
  base_type_map.insert("Array", Variant::ARRAY);
  base_type_map.insert("PackedByteArray", Variant::PACKED_BYTE_ARRAY);
  base_type_map.insert("PackedInt32Array", Variant::PACKED_INT32_ARRAY);
  base_type_map.insert("PackedFloat32Array", Variant::PACKED_FLOAT32_ARRAY);
  base_type_map.insert("PackedInt64Array", Variant::PACKED_INT64_ARRAY);
  base_type_map.insert("PackedFloat64Array", Variant::PACKED_FLOAT64_ARRAY);
  base_type_map.insert("PackedStringArray", Variant::PACKED_STRING_ARRAY);
  base_type_map.insert("PackedVector2Array", Variant::PACKED_VECTOR2_ARRAY);
  base_type_map.insert("PackedVector3Array", Variant::PACKED_VECTOR3_ARRAY);
  base_type_map.insert("PackedColorArray", Variant::PACKED_COLOR_ARRAY);
}

VisualScriptEditor::~VisualScriptEditor() {
  undo_redo->clear_history(); // Avoid crashes.
  memdelete(signal_editor);
  memdelete(variable_editor);
}

static ScriptEditorBase *create_editor(const Ref<Resource> &p_resource) {
  if (Object::cast_to<VisualScript>(*p_resource)) {
    return memnew(VisualScriptEditor);
  }

  return nullptr;
}

VisualScriptEditor::Clipboard *VisualScriptEditor::clipboard = nullptr;

void VisualScriptEditor::free_clipboard() {
  if (clipboard) {
    memdelete(clipboard);
  }
}

static void register_editor_callback() {
  ScriptEditor::register_create_script_editor_function(create_editor);

  ED_SHORTCUT("visual_script_editor/toggle_breakpoint",
              TTR("Toggle Breakpoint"), Key::F9);
  ED_SHORTCUT("visual_script_editor/find_node_type", TTR("Find Node Type"),
              KeyModifierMask::CMD_OR_CTRL + Key::F);
  ED_SHORTCUT("visual_script_editor/create_function", TTR("Make Function"),
              KeyModifierMask::CMD_OR_CTRL + Key::G);
  ED_SHORTCUT("visual_script_editor/refresh_nodes", TTR("Refresh Graph"),
              KeyModifierMask::CMD_OR_CTRL + Key::R);
  ED_SHORTCUT("visual_script_editor/edit_member", TTR("Edit Member"),
              KeyModifierMask::CMD_OR_CTRL + Key::E);
}

void VisualScriptEditor::register_editor() {
  // Too early to register stuff here, request a callback.
  EditorNode::add_plugin_init_callback(register_editor_callback);
}

void VisualScriptEditor::validate() {}

// VisualScriptCustomNodes

Ref<VisualScriptNode>
VisualScriptCustomNodes::create_node_custom(const String &p_name) {
  Ref<VisualScriptCustomNode> node;
  node.instantiate();
  node->set_script(singleton->custom_nodes[p_name]);
  return node;
}

VisualScriptCustomNodes *VisualScriptCustomNodes::singleton = nullptr;
HashMap<String, Ref<RefCounted>> VisualScriptCustomNodes::custom_nodes;

VisualScriptCustomNodes::VisualScriptCustomNodes() { singleton = this; }

VisualScriptCustomNodes::~VisualScriptCustomNodes() { custom_nodes.clear(); }

void VisualScriptCustomNodes::add_custom_node(const String &p_name,
                                              const String &p_category,
                                              const Ref<Script> &p_script) {
  String node_name = "custom/" + p_category + "/" + p_name;
  custom_nodes.insert(node_name, p_script);
  VisualScriptLanguage::singleton->add_register_func(
      node_name, &VisualScriptCustomNodes::create_node_custom);
  emit_signal(SNAME("custom_nodes_updated"));
}

void VisualScriptCustomNodes::remove_custom_node(const String &p_name,
                                                 const String &p_category) {
  String node_name = "custom/" + p_category + "/" + p_name;
  custom_nodes.erase(node_name);
  VisualScriptLanguage::singleton->remove_register_func(node_name);
  emit_signal(SNAME("custom_nodes_updated"));
}

void VisualScriptCustomNodes::_bind_methods() {
  ClassDB::bind_method(
      D_METHOD("add_custom_node", "name", "category", "script"),
      &VisualScriptCustomNodes::add_custom_node);
  ClassDB::bind_method(D_METHOD("remove_custom_node", "name", "category"),
                       &VisualScriptCustomNodes::remove_custom_node);
  ADD_SIGNAL(MethodInfo("custom_nodes_updated"));
}

#endif
