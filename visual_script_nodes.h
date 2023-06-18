/*************************************************************************/
/*  visual_script_nodes.h                                                */
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

#ifndef VISUAL_SCRIPT_NODES_H
#define VISUAL_SCRIPT_NODES_H

#ifdef GDEXTENSION
#include <godot/gdnative_interface.h>
using namespace godot;

#else
#include "core/object/gdvirtual.gen.inc"
#include "core/object/script_language.h"
#include "scene/main/multiplayer_api.h"
#endif

#include "visual_script.h"

class VisualScriptFunction : public VisualScriptNode {
	GDCLASS(VisualScriptFunction, VisualScriptNode);

	struct Argument {
		String name;
		Variant::Type type;
		PropertyHint hint;
		String hint_string;
	};

	Vector<Argument> arguments;

	bool stack_less;
	int stack_size;
	MultiplayerAPI::RPCMode rpc_mode;
	bool sequenced;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_text() const override;
	virtual String get_category() const override { return "flow_control"; }

	void add_argument(Variant::Type p_type, const String &p_name, int p_index = -1, const PropertyHint p_hint = PROPERTY_HINT_NONE, const String &p_hint_string = String(""));
	void set_argument_type(int p_argidx, Variant::Type p_type);
	Variant::Type get_argument_type(int p_argidx) const;
	void set_argument_name(int p_argidx, const String &p_name);
	String get_argument_name(int p_argidx) const;
	void remove_argument(int p_argidx);
	int get_argument_count() const;

	void set_stack_less(bool p_enable);
	bool is_stack_less() const;

	void set_sequenced(bool p_enable);
	bool is_sequenced() const;

	void set_stack_size(int p_size);
	int get_stack_size() const;

	void set_rpc_mode(MultiplayerAPI::RPCMode p_mode);
	MultiplayerAPI::RPCMode get_rpc_mode() const;

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	virtual void reset_state() override;

	VisualScriptFunction();
};

class VisualScriptLists : public VisualScriptNode {
	GDCLASS(VisualScriptLists, VisualScriptNode)

	struct Port {
		String name;
		Variant::Type type;
	};

protected:
	Vector<Port> inputports;
	Vector<Port> outputports;

	enum {
		OUTPUT_EDITABLE = 0x0001,
		OUTPUT_NAME_EDITABLE = 0x0002,
		OUTPUT_TYPE_EDITABLE = 0x0004,
		INPUT_EDITABLE = 0x0008,
		INPUT_NAME_EDITABLE = 0x000F,
		INPUT_TYPE_EDITABLE = 0x0010,
	};

	int flags;

	bool sequenced;

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	static void _bind_methods();

public:
	virtual void reset_state() override;

	virtual bool is_output_port_editable() const;
	virtual bool is_output_port_name_editable() const;
	virtual bool is_output_port_type_editable() const;

	virtual bool is_input_port_editable() const;
	virtual bool is_input_port_name_editable() const;
	virtual bool is_input_port_type_editable() const;

	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	void add_input_data_port(Variant::Type p_type, const String &p_name, int p_index = -1);
	void set_input_data_port_type(int p_idx, Variant::Type p_type);
	void set_input_data_port_name(int p_idx, const String &p_name);
	void remove_input_data_port(int p_argidx);

	void add_output_data_port(Variant::Type p_type, const String &p_name, int p_index = -1);
	void set_output_data_port_type(int p_idx, Variant::Type p_type);
	void set_output_data_port_name(int p_idx, const String &p_name);
	void remove_output_data_port(int p_argidx);

	void set_sequenced(bool p_enable);
	bool is_sequenced() const;

	VisualScriptLists();
};

class VisualScriptComposeArray : public VisualScriptLists {
	GDCLASS(VisualScriptComposeArray, VisualScriptLists)

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_text() const override;
	virtual String get_category() const override { return "functions"; }

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptComposeArray();
};

class VisualScriptOperator : public VisualScriptNode {
	GDCLASS(VisualScriptOperator, VisualScriptNode);

	Variant::Type typed;
	Variant::Operator op;

protected:
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override { return "operators"; }

	void set_operator(Variant::Operator p_op);
	Variant::Operator get_operator() const;

	void set_typed(Variant::Type p_op);
	Variant::Type get_typed() const;

	static String get_operator_name(Variant::Operator p_op);

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptOperator();
};

class VisualScriptSelect : public VisualScriptNode {
	GDCLASS(VisualScriptSelect, VisualScriptNode);

	Variant::Type typed;

protected:
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_text() const override;
	virtual String get_category() const override { return "operators"; }

	void set_typed(Variant::Type p_op);
	Variant::Type get_typed() const;

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptSelect();
};

class VisualScriptVariableGet : public VisualScriptNode {
	GDCLASS(VisualScriptVariableGet, VisualScriptNode);

	StringName variable;

protected:
	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override { return "data"; }

	void set_variable(StringName p_variable);
	StringName get_variable() const;

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptVariableGet();
};

class VisualScriptVariableSet : public VisualScriptNode {
	GDCLASS(VisualScriptVariableSet, VisualScriptNode);

	StringName variable;

protected:
	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override { return "data"; }

	void set_variable(StringName p_variable);
	StringName get_variable() const;

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptVariableSet();
};

class VisualScriptConstant : public VisualScriptNode {
	GDCLASS(VisualScriptConstant, VisualScriptNode);

	Variant::Type type;
	Variant value;

protected:
	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override { return "constants"; }

	void set_constant_type(Variant::Type p_type);
	Variant::Type get_constant_type() const;

	void set_constant_value(Variant p_value);
	Variant get_constant_value() const;

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptConstant();
};

class VisualScriptPreload : public VisualScriptNode {
	GDCLASS(VisualScriptPreload, VisualScriptNode);

	Ref<Resource> preload;

protected:
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override { return "data"; }

	void set_preload(const Ref<Resource> &p_preload);
	Ref<Resource> get_preload() const;

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptPreload();
};

class VisualScriptIndexGet : public VisualScriptNode {
	GDCLASS(VisualScriptIndexGet, VisualScriptNode);

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override { return "operators"; }

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptIndexGet();
};

class VisualScriptIndexSet : public VisualScriptNode {
	GDCLASS(VisualScriptIndexSet, VisualScriptNode);

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override { return "operators"; }

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptIndexSet();
};

class VisualScriptGlobalConstant : public VisualScriptNode {
	GDCLASS(VisualScriptGlobalConstant, VisualScriptNode);

	int index;

	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override { return "constants"; }

	void set_global_constant(int p_which);
	int get_global_constant();

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptGlobalConstant();
};

class VisualScriptClassConstant : public VisualScriptNode {
	GDCLASS(VisualScriptClassConstant, VisualScriptNode);

	StringName base_type;
	StringName name;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override { return "constants"; }

	void set_class_constant(const StringName &p_which);
	StringName get_class_constant();

	void set_base_type(const StringName &p_which);
	StringName get_base_type();

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptClassConstant();
};

class VisualScriptBasicTypeConstant : public VisualScriptNode {
	GDCLASS(VisualScriptBasicTypeConstant, VisualScriptNode);

	Variant::Type type;
	StringName name;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_text() const override;
	virtual String get_category() const override { return "constants"; }

	void set_basic_type_constant(const StringName &p_which);
	StringName get_basic_type_constant() const;

	void set_basic_type(Variant::Type p_which);
	Variant::Type get_basic_type() const;

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptBasicTypeConstant();
};

class VisualScriptMathConstant : public VisualScriptNode {
	GDCLASS(VisualScriptMathConstant, VisualScriptNode);

public:
	enum MathConstant {
		MATH_CONSTANT_ONE,
		MATH_CONSTANT_PI,
		MATH_CONSTANT_HALF_PI,
		MATH_CONSTANT_TAU,
		MATH_CONSTANT_E,
		MATH_CONSTANT_SQRT2,
		MATH_CONSTANT_INF,
		MATH_CONSTANT_NAN,
		MATH_CONSTANT_MAX
	};

private:
	static const char *const_name[MATH_CONSTANT_MAX];
	static double const_value[MATH_CONSTANT_MAX];
	MathConstant constant;

protected:
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override { return "constants"; }

	void set_math_constant(MathConstant p_which);
	MathConstant get_math_constant();

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptMathConstant();
};

class VisualScriptEngineSingleton : public VisualScriptNode {
	GDCLASS(VisualScriptEngineSingleton, VisualScriptNode);

	String singleton;

protected:
	void _validate_property(PropertyInfo &p_property) const;

	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override { return "data"; }

	void set_singleton(const String &p_string);
	String get_singleton();

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	virtual TypeGuess guess_output_type(TypeGuess *p_inputs, int p_output) const override;

	VisualScriptEngineSingleton();
};

class VisualScriptSceneNode : public VisualScriptNode {
	GDCLASS(VisualScriptSceneNode, VisualScriptNode);

	NodePath path;

protected:
	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override { return "data"; }

	void set_node_path(const NodePath &p_path);
	NodePath get_node_path();

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	virtual TypeGuess guess_output_type(TypeGuess *p_inputs, int p_output) const override;

	VisualScriptSceneNode();
};

class VisualScriptSceneTree : public VisualScriptNode {
	GDCLASS(VisualScriptSceneTree, VisualScriptNode);

protected:
	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override { return "data"; }

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	virtual TypeGuess guess_output_type(TypeGuess *p_inputs, int p_output) const override;

	VisualScriptSceneTree();
};

class VisualScriptResourcePath : public VisualScriptNode {
	GDCLASS(VisualScriptResourcePath, VisualScriptNode);

	String path;

protected:
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override { return "data"; }

	void set_resource_path(const String &p_path);
	String get_resource_path();

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptResourcePath();
};

class VisualScriptSelf : public VisualScriptNode {
	GDCLASS(VisualScriptSelf, VisualScriptNode);

protected:
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override { return "data"; }

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	virtual TypeGuess guess_output_type(TypeGuess *p_inputs, int p_output) const override;

	VisualScriptSelf();
};

class VisualScriptCustomNode : public VisualScriptNode {
	GDCLASS(VisualScriptCustomNode, VisualScriptNode);

protected:
	static void _bind_methods();
	friend class VisualScriptNodeInstanceCustomNode;
	GDVIRTUAL0RC(int, _get_output_sequence_port_count)
	GDVIRTUAL0RC(bool, _has_input_sequence_port)
	GDVIRTUAL1RC(String, _get_output_sequence_port_text, int)

	GDVIRTUAL0RC(int, _get_input_value_port_count)
	GDVIRTUAL1RC(int, _get_input_value_port_type, int)
	GDVIRTUAL1RC(String, _get_input_value_port_name, int)
	GDVIRTUAL1RC(int, _get_input_value_port_hint, int)
	GDVIRTUAL1RC(String, _get_input_value_port_hint_string, int)

	GDVIRTUAL0RC(int, _get_output_value_port_count)
	GDVIRTUAL1RC(int, _get_output_value_port_type, int)
	GDVIRTUAL1RC(String, _get_output_value_port_name, int)
	GDVIRTUAL1RC(int, _get_output_value_port_hint, int)
	GDVIRTUAL1RC(String, _get_output_value_port_hint_string, int)

	GDVIRTUAL0RC(String, _get_caption)
	GDVIRTUAL0RC(String, _get_text)
	GDVIRTUAL0RC(String, _get_category)

	GDVIRTUAL0RC(int, _get_working_memory_size)

	GDVIRTUAL4RC(Variant, _step, Array, Array, int, Array)

public:
	enum StartMode { //replicated for step
		START_MODE_BEGIN_SEQUENCE,
		START_MODE_CONTINUE_SEQUENCE,
		START_MODE_RESUME_YIELD
	};

	enum { //replicated for step
		STEP_SHIFT = 1 << 24,
		STEP_MASK = STEP_SHIFT - 1,
		STEP_PUSH_STACK_BIT = STEP_SHIFT, //push bit to stack
		STEP_GO_BACK_BIT = STEP_SHIFT << 1, //go back to previous node
		STEP_NO_ADVANCE_BIT = STEP_SHIFT << 2, //do not advance past this node
		STEP_EXIT_FUNCTION_BIT = STEP_SHIFT << 3, //return from function
		STEP_YIELD_BIT = STEP_SHIFT << 4, //yield (will find VisualScriptFunctionState state in first working memory)
	};

	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_text() const override;
	virtual String get_category() const override;

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	virtual TypeGuess guess_output_type(TypeGuess *p_inputs, int p_output) const override;

	void _script_changed();

	VisualScriptCustomNode();
};

class VisualScriptSubCall : public VisualScriptNode {
	GDCLASS(VisualScriptSubCall, VisualScriptNode);

protected:
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_text() const override;
	virtual String get_category() const override;

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptSubCall();
};

class VisualScriptComment : public VisualScriptNode {
	GDCLASS(VisualScriptComment, VisualScriptNode);

	String title;
	String description;
	Size2 size;

protected:
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_text() const override;
	virtual String get_category() const override;

	void set_title(const String &p_title);
	String get_title() const;

	void set_description(const String &p_description);
	String get_description() const;

	void set_size(const Size2 &p_size);
	Size2 get_size() const;

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptComment();
};

class VisualScriptConstructor : public VisualScriptNode {
	GDCLASS(VisualScriptConstructor, VisualScriptNode);

	Variant::Type type;
	MethodInfo constructor;

protected:
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override;

	void set_constructor_type(Variant::Type p_type);
	Variant::Type get_constructor_type() const;

	void set_constructor(const Dictionary &p_info);
	Dictionary get_constructor() const;

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptConstructor();
};

class VisualScriptLocalVar : public VisualScriptNode {
	GDCLASS(VisualScriptLocalVar, VisualScriptNode);

	StringName name;
	Variant::Type type;

protected:
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override;

	void set_var_name(const StringName &p_name);
	StringName get_var_name() const;

	void set_var_type(Variant::Type p_type);
	Variant::Type get_var_type() const;

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptLocalVar();
};

class VisualScriptLocalVarSet : public VisualScriptNode {
	GDCLASS(VisualScriptLocalVarSet, VisualScriptNode);

	StringName name;
	Variant::Type type;

protected:
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_text() const override;
	virtual String get_category() const override;

	void set_var_name(const StringName &p_name);
	StringName get_var_name() const;

	void set_var_type(Variant::Type p_type);
	Variant::Type get_var_type() const;

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptLocalVarSet();
};

class VisualScriptInputAction : public VisualScriptNode {
	GDCLASS(VisualScriptInputAction, VisualScriptNode);

public:
	enum Mode {
		MODE_PRESSED,
		MODE_RELEASED,
		MODE_JUST_PRESSED,
		MODE_JUST_RELEASED,
	};

	StringName name;
	Mode mode;

protected:
	void _validate_property(PropertyInfo &p_property) const;

	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override;

	void set_action_name(const StringName &p_name);
	StringName get_action_name() const;

	void set_action_mode(Mode p_mode);
	Mode get_action_mode() const;

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptInputAction();
};

class VisualScriptDeconstruct : public VisualScriptNode {
	GDCLASS(VisualScriptDeconstruct, VisualScriptNode);

	struct Element {
		StringName name;
		Variant::Type type;
	};

	Vector<Element> elements;

	void _update_elements();
	Variant::Type type;

	void _set_elem_cache(const Array &p_elements);
	Array _get_elem_cache() const;

	void _validate_property(PropertyInfo &p_property) const;

protected:
	static void _bind_methods();

public:
	virtual int get_output_sequence_port_count() const override;
	virtual bool has_input_sequence_port() const override;

	virtual String get_output_sequence_port_text(int p_port) const override;

	virtual int get_input_value_port_count() const override;
	virtual int get_output_value_port_count() const override;

	virtual PropertyInfo get_input_value_port_info(int p_idx) const override;
	virtual PropertyInfo get_output_value_port_info(int p_idx) const override;

	virtual String get_caption() const override;
	virtual String get_category() const override;

	void set_deconstruct_type(Variant::Type p_type);
	Variant::Type get_deconstruct_type() const;

	virtual VisualScriptNodeInstance *instantiate(VisualScriptInstance *p_instance) override;

	VisualScriptDeconstruct();
};

void register_visual_script_nodes();
void unregister_visual_script_nodes();

#ifdef GDEXTENSION
VARIANT_ENUM_CAST(VisualScriptInputAction, Mode);
VARIANT_ENUM_CAST(VisualScriptMathConstant, MathConstant);
VARIANT_ENUM_CAST(VisualScriptCustomNode, StartMode);

#else
VARIANT_ENUM_CAST(VisualScriptInputAction::Mode);
VARIANT_ENUM_CAST(VisualScriptMathConstant::MathConstant);
VARIANT_ENUM_CAST(VisualScriptCustomNode::StartMode);

#endif

#endif // VISUAL_SCRIPT_NODES_H
