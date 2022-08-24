# VisualScript module for Godot

This repository contains the implementation of the "VisualScript" visual
scripting language which used to be included directly in
[Godot Engine](https://godotengine.org/) from Godot 3.0 until its removal in
Godot 4.0.

[See this blog post](https://godotengine.org/article/godot-4-will-discontinue-visual-scripting)
for details on why the VisualScript implementation was removed from Godot 4.0.

This repository has two aims:

- Initially, this is a straight copy of the `modules/visual_script/` folder in
  the Godot source tree, and can still be compiled as such to restore this
  functionality in Godot 4.0.
- Ideally, if there is community interest and participation, we would like this
  module (which needs to be compiled together with the engine in a custom build)
  converted to use GDExtension via [godot-cpp](https://github.com/godotengine/godot-cpp),
  allowing it to be distributed as a compiled library that can add this
  functionality back to a stock Godot build. As an extension, this feature could
  also evolved more freely, attracting new contributors with innovative ideas,
  and thus become a better visual scripting solution than what we had in the
  Godot 3.x era.

## Compiling

For now this is still a C++ module, so it needs to be compiled together with
Godot as a custom build.

[See the Godot documentation](https://docs.godotengine.org/en/latest/development/compiling/)
for instructions on how to compile the engine.

To add this module, it should be cloned or copied as the `modules/visual_script/`
folder in the Godot source tree.

For example:

```
git clone https://github.com/godotengine/godot
cd godot/modules
git clone https://github.com/godotengine/godot-visual-script visual_script
cd ..
scons
```

Note that a custom `visual_script` name is given for this repository's clone, so
that the final structure looks like `modules/visual_script/` and not
`modules/godot-visual-script/`.

## Contributing

If you're interested in working on porting this C++ module to GDExtension,
please join the `#scripting` (to discuss VisualScript) and `#gdextension` (to
discuss GDExtension and `godot-cpp`) on the [Godot Contributors Chat](https://chat.godotengine.org).
