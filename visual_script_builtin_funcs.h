/*************************************************************************/
/*  visual_script_builtin_funcs.h                                        */
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

#ifndef VISUAL_SCRIPT_BUILTIN_FUNCS_H
#define VISUAL_SCRIPT_BUILTIN_FUNCS_H

#include "visual_script.h"

class VisualScriptBuiltinFunc : public VisualScriptNode {
  GDCLASS(VisualScriptBuiltinFunc, VisualScriptNode);

public:
  enum BuiltinFunc {
    MATH_SIN,
    MATH_COS,
    MATH_TAN,
    MATH_SINH,
    MATH_COSH,
    MATH_TANH,
    MATH_ASIN,
    MATH_ACOS,
    MATH_ATAN,
    MATH_ATAN2,
    MATH_SQRT,
    MATH_FMOD,
    MATH_FPOSMOD,
    MATH_FLOOR,
    MATH_CEIL,
    MATH_ROUND,
    MATH_ABS,
    MATH_SIGN,
    MATH_POW,
    MATH_LOG,
    MATH_EXP,
    MATH_ISNAN,
    MATH_ISINF,
    MATH_EASE,
    MATH_STEP_DECIMALS,
    MATH_SNAPPED,
    MATH_LERP,
    MATH_CUBIC_INTERPOLATE,
    MATH_INVERSE_LERP,
    MATH_REMAP,
    MATH_MOVE_TOWARD,
    MATH_RANDOMIZE,
    MATH_RANDI,
    MATH_RANDF,
    MATH_RANDI_RANGE,
    MATH_RANDF_RANGE,
    MATH_RANDFN,
    MATH_SEED,
    MATH_RANDSEED,
    MATH_DEG_TO_RAD,
    MATH_RAD_TO_DEG,
    MATH_LINEAR_TO_DB,
    MATH_DB_TO_LINEAR,
    MATH_WRAP,
    MATH_WRAPF,
    MATH_PINGPONG,
    LOGIC_MAX,
    LOGIC_MIN,
    LOGIC_CLAMP,
    LOGIC_NEAREST_PO2,
    OBJ_WEAKREF,
    TYPE_CONVERT,
    TYPE_OF,
    TYPE_EXISTS,
    TEXT_CHAR,
    TEXT_STR,
    TEXT_PRINT,
    TEXT_PRINTERR,
    TEXT_PRINTRAW,
    TEXT_PRINT_VERBOSE,
    VAR_TO_STR,
    STR_TO_VAR,
    VAR_TO_BYTES,
    BYTES_TO_VAR,
    MATH_SMOOTHSTEP,
    MATH_POSMOD,
    MATH_LERP_ANGLE,
    TEXT_ORD,
    FUNC_MAX
  };

  static int get_func_argument_count(BuiltinFunc p_func);
  static String get_func_name(BuiltinFunc p_func);
  static void exec_func(BuiltinFunc p_func, const Variant **p_inputs,
                        Variant *r_return, Callable::CallError &r_error,
                        String &r_error_str);
  static BuiltinFunc find_function(const String &p_string);

private:
  static const char *func_name[FUNC_MAX];
  BuiltinFunc func;

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
  // virtual String get_text() const;
  virtual String get_category() const override { return "functions"; }

  void set_func(BuiltinFunc p_which);
  BuiltinFunc get_func();

  virtual VisualScriptNodeInstance *
  instantiate(VisualScriptInstance *p_instance) override;

  VisualScriptBuiltinFunc(VisualScriptBuiltinFunc::BuiltinFunc func);
  VisualScriptBuiltinFunc();
};

VARIANT_ENUM_CAST(VisualScriptBuiltinFunc::BuiltinFunc)

void register_visual_script_builtin_func_node();

#endif // VISUAL_SCRIPT_BUILTIN_FUNCS_H
