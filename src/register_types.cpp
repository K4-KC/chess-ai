#include "register_types.h"

// Include your custom class headers here
#include "NeuralNet.h"
#include "ChessAgent.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_chess_ai_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	// Register your classes here
	ClassDB::register_class<NeuralNet>();
	ClassDB::register_class<ChessAgent>();
}

void uninitialize_chess_ai_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	// Cleanup if necessary (usually handled automatically by Godot's memory management)
}

extern "C" {
	// Initialization.
	GDExtensionBool GDE_EXPORT chess_ai_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
		godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

		init_obj.register_initializer(initialize_chess_ai_module);
		init_obj.register_terminator(uninitialize_chess_ai_module);
		init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

		return init_obj.init();
	}
}
