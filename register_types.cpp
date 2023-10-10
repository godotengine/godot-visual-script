/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "core/config/engine.h"
#include "core/io/resource_loader.h"
#include "visual_script.h"
#include "visual_script_expression.h"
#include "visual_script_flow_control.h"
#include "visual_script_func_nodes.h"
#include "visual_script_nodes.h"
#include "visual_script_yield_nodes.h"

VisualScriptLanguage *visual_script_language = nullptr;

#ifdef TOOLS_ENABLED
#include "editor/visual_script_editor.h"
#endif

void initialize_visual_script_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		visual_script_language = memnew(VisualScriptLanguage);
		// script_language_gd->init();
		ScriptServer::register_language(visual_script_language);

		GDREGISTER_CLASS(VisualScript);
		GDREGISTER_ABSTRACT_CLASS(VisualScriptNode);
		GDREGISTER_CLASS(VisualScriptFunctionState);
		GDREGISTER_CLASS(VisualScriptFunction);
		GDREGISTER_ABSTRACT_CLASS(VisualScriptLists);
		GDREGISTER_CLASS(VisualScriptComposeArray);
		GDREGISTER_CLASS(VisualScriptOperator);
		GDREGISTER_CLASS(VisualScriptVariableSet);
		GDREGISTER_CLASS(VisualScriptVariableGet);
		GDREGISTER_CLASS(VisualScriptConstant);
		GDREGISTER_CLASS(VisualScriptIndexGet);
		GDREGISTER_CLASS(VisualScriptIndexSet);
		GDREGISTER_CLASS(VisualScriptGlobalConstant);
		GDREGISTER_CLASS(VisualScriptClassConstant);
		GDREGISTER_CLASS(VisualScriptMathConstant);
		GDREGISTER_CLASS(VisualScriptBasicTypeConstant);
		GDREGISTER_CLASS(VisualScriptEngineSingleton);
		GDREGISTER_CLASS(VisualScriptSceneNode);
		GDREGISTER_CLASS(VisualScriptSceneTree);
		GDREGISTER_CLASS(VisualScriptResourcePath);
		GDREGISTER_CLASS(VisualScriptSelf);
		GDREGISTER_CLASS(VisualScriptCustomNode);
		GDREGISTER_CLASS(VisualScriptSubCall);
		GDREGISTER_CLASS(VisualScriptConstructor);
		GDREGISTER_CLASS(VisualScriptLocalVar);
		GDREGISTER_CLASS(VisualScriptLocalVarSet);
		GDREGISTER_CLASS(VisualScriptInputAction);
		GDREGISTER_CLASS(VisualScriptDeconstruct);
		GDREGISTER_CLASS(VisualScriptPreload);
		GDREGISTER_CLASS(VisualScriptTypeCast);

		GDREGISTER_CLASS(VisualScriptFunctionCall);
		GDREGISTER_CLASS(VisualScriptPropertySet);
		GDREGISTER_CLASS(VisualScriptPropertyGet);
		GDREGISTER_CLASS(VisualScriptEmitSignal);

		GDREGISTER_CLASS(VisualScriptReturn);
		GDREGISTER_CLASS(VisualScriptCondition);
		GDREGISTER_CLASS(VisualScriptWhile);
		GDREGISTER_CLASS(VisualScriptIterator);
		GDREGISTER_CLASS(VisualScriptSequence);
		GDREGISTER_CLASS(VisualScriptSwitch);
		GDREGISTER_CLASS(VisualScriptSelect);

		GDREGISTER_CLASS(VisualScriptYield);
		GDREGISTER_CLASS(VisualScriptYieldSignal);

		GDREGISTER_CLASS(VisualScriptExpression);

		register_visual_script_nodes();
		register_visual_script_func_nodes();
		register_visual_script_flow_control_nodes();
		register_visual_script_yield_nodes();
		register_visual_script_expression_node();
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		ClassDB::set_current_api(ClassDB::API_EDITOR);
		ClassDB::set_current_api(ClassDB::API_CORE);
		VisualScriptEditor::register_editor();
	}
#endif
}

void uninitialize_visual_script_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		unregister_visual_script_nodes();

		ScriptServer::unregister_language(visual_script_language);

		if (visual_script_language) {
			memdelete(visual_script_language);
		}
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		VisualScriptEditor::free_clipboard();
	}
#endif
}
