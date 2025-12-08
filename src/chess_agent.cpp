#include "chess_agent.h"

// Godot includes for binding, engine checks, and logging.
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// Bind methods exposed to GDScript.
void ChessAgent::_bind_methods() {
	ClassDB::bind_method(
		D_METHOD("select_best_move", "board_state", "possible_moves"),
		&ChessAgent::select_best_move
	);
}

// Constructor: just initialize pointer; actual net is created in _ready.
ChessAgent::ChessAgent() {
	neural_net = nullptr;
}

ChessAgent::~ChessAgent() {
	// neural_net is a child node created with memnew; Godot will free it
	// when the node tree is cleared, so no manual delete here.
}

// Set up the neural network when the game runs (skip in editor).
void ChessAgent::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	// Instantiate and configure the Neural Network.
	neural_net = memnew(NeuralNet);
	add_child(neural_net);

	Array layers;
	layers.push_back(INPUT_NODES);
	layers.push_back(HIDDEN_NODES);
	layers.push_back(OUTPUT_NODES);

	neural_net->set_layer_sizes(layers);

	UtilityFunctions::print("C++ ChessAgent initialized with NeuralNet.");
}

// Evaluate each move with the neural net and return the highest-scoring move.
Dictionary ChessAgent::select_best_move(const Array &board_state, const Array &possible_moves) {
	if (possible_moves.size() == 0) {
		return Dictionary();
	}

	// Convert Godot Array board_state (64 ints) to a C++ vector for quick copying.
	std::vector<int> current_board;
	current_board.resize(64);
	for (int i = 0; i < 64; i++) {
		current_board[i] = (int)board_state[i];
	}

	// Default to first move as fallback.
	Dictionary best_move = possible_moves[0]; // Fallback to 0th move
	double best_score = 0.0;                  // Lower than any sigmoid output (0.0 to 1.0)

	// Iterate through all candidate moves.
	for (int i = 0; i < possible_moves.size(); i++) {
		Dictionary move = possible_moves[i];

		Vector2 start = move["start"];
		Vector2 end = move["end"];

		int start_idx = (int)start.y * 8 + (int)start.x;
		int end_idx = (int)end.y * 8 + (int)end.x;

		// Skip obviously invalid indices.
		if (start_idx < 0 || start_idx >= 64 || end_idx < 0 || end_idx >= 64) {
			continue;
		}

		// SIMULATION STEP
		// Work on a copy of the board, apply the move, then evaluate.
		std::vector<int> temp_board = current_board;

		// 2. Execute move (Overwrite destination with source, clear source).
		// NOTE: Special moves (castling, en passant, promotion) are not handled here.
		int piece = temp_board[start_idx];
		temp_board[end_idx] = piece;
		temp_board[start_idx] = 0;

		// 3. Encode flat board into neural net input vector.
		Array inputs = encode_board_to_inputs(temp_board);

		// 4. Run Neural Net to get a scalar evaluation.
		neural_net->set_inputs(inputs);
		neural_net->compute();
		Array outputs = neural_net->get_outputs();
		double score = (double)outputs[0];

		// 5. Track best-scoring move.
		if (score > best_score) {
			best_score = score;
			best_move = move;
		}
	}

	UtilityFunctions::print("ChessAgent selected move with score: ", best_score);
	return best_move;
}

// Encode a 64-square board into a 768-length one-hot input vector for the NN.
Array ChessAgent::encode_board_to_inputs(const std::vector<int> &board_state) {
	Array inputs;

	// We need 768 inputs (64 squares * 12 piece types).
	// Map: 1=WP, 2=WN, 3=WB, 4=WR, 5=WQ, 6=WK
	//      7=BP, 8=BN, 9=BB,10=BR,11=BQ,12=BK
	// For each square, 12 channels are created; exactly one or zero is 1.0.

	for (int i = 0; i < 64; i++) {
		int piece_val = board_state[i];

		// There are 12 channels per square.
		// Channel 0-5: White P, N, B, R, Q, K
		// Channel 6-11: Black P, N, B, R, Q, K
		for (int channel = 0; channel < 12; channel++) {
			double val = 0.0;

			if (piece_val != 0) {
				// Map piece_val to channel index:
				// piece_val 1 -> channel 0, piece_val 7 -> channel 6, etc.
				int target_channel = piece_val - 1;
				if (channel == target_channel) {
					val = 1.0;
				}
			}

			inputs.push_back(val);
		}
	}

	return inputs;
}
