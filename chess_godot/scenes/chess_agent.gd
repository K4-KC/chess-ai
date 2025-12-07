extends "res://scenes/ChessGame.gd"

# --- CONFIGURATION ---
const AI_COLOR = 1 # 0 = White, 1 = Black
const INPUT_SIZE = 64 * 6 * 2 # 768 Inputs
const HIDDEN_LAYER_SIZE = 128 # Arbitrary hidden layer
const OUTPUT_SIZE = 1 # Scalar score

var neural_net = null

func _ready():
	# 1. Initialize the base game
	super._ready()
	
	# 2. Initialize the Neural Network
	# We assume the NeuralNet class is available globally via the C++ module
	if ClassDB.class_exists("NeuralNet"):
		neural_net = ClassDB.instantiate("NeuralNet")
		add_child(neural_net)
		
		# Topology: [Input, Hidden, Output]
		neural_net.set_layer_sizes([INPUT_SIZE, HIDDEN_LAYER_SIZE, OUTPUT_SIZE])
		print("ChessAgent: Neural Network Initialized.")
	else:
		printerr("ChessAgent Error: NeuralNet C++ class not found.")

# --- TURN HANDLING ---

# Override the parent finalize_turn to trigger AI
func finalize_turn():
	super.finalize_turn()
	
	# If it's the AI's turn, schedule the move
	if turn == AI_COLOR:
		# Use call_deferred to allow the engine to update the previous move's visual first
		call_deferred("perform_ai_turn")

func perform_ai_turn():
	print("ChessAgent: Thinking...")
	
	var best_move = null
	var best_score = -1.0 # Sigmoid outputs 0.0 to 1.0, so -1 is safe lowest
	
	# 1. Get all valid moves for the AI
	var moves = get_all_valid_moves(AI_COLOR)
	
	if moves.is_empty():
		print("ChessAgent: No legal moves found.")
		return
		
	# 2. Iterate and Evaluate
	for move in moves:
		var score = evaluate_move(move)
		
		if score > best_score:
			best_score = score
			best_move = move
	
	# 3. Execute Best Move
	if best_move:
		print("ChessAgent: Moving ", best_move.piece.type, " to ", best_move.end, " with score: ", best_score)
		# We must use the parent's execute_move to handle captures/sprites
		execute_move(best_move.piece, best_move.start, best_move.end)
		
		# Handle promotion automatically for AI (Always Queen for now)
		if is_promoting:
			_on_promotion_selected("q")

# --- EVALUATION LOGIC ---

func evaluate_move(move):
	# A. Simulate the move on the data structure (no visuals)
	var start = move.start
	var end = move.end
	var piece = move.piece
	
	var captured_piece = board[end.x][end.y]
	
	# Apply move
	board[start.x][start.y] = null
	board[end.x][end.y] = piece
	
	# B. Encode board state
	var input_vector = encode_board_state()
	
	# C. Run Neural Net
	neural_net.set_inputs(input_vector)
	neural_net.compute()
	var output = neural_net.get_outputs()
	var score = output[0] # Single output neuron
	
	# D. Revert move
	board[end.x][end.y] = captured_piece
	board[start.x][start.y] = piece
	
	return score

# --- INPUT ENCODING ---

func encode_board_state():
	var inputs = []
	# Structure: 64 squares. 
	# Per Square: 12 channels [P0, N0, B0, R0, Q0, K0, P1, N1, B1, R1, Q1, K1]
	# Total: 768 floats
	
	var piece_types = ["p", "n", "b", "r", "q", "k"]
	
	for y in range(8):
		for x in range(8):
			var piece = board[x][y]
			
			# Create 12 zeros for this square
			var square_vector = []
			square_vector.resize(12)
			square_vector.fill(0.0)
			
			if piece != null:
				# Find index: (Color * 6) + TypeIndex
				var type_idx = piece_types.find(piece.type)
				var channel_idx = (piece.color * 6) + type_idx
				if type_idx != -1:
					square_vector[channel_idx] = 1.0
			
			inputs.append_array(square_vector)
			
	return inputs

# --- HELPER: MOVE GENERATOR ---

func get_all_valid_moves(color):
	var valid_moves = []
	
	for x in range(8):
		for y in range(8):
			var piece = board[x][y]
			if piece and piece.color == color:
				
				# Check all possible target squares (inefficient but thorough)
				for tx in range(8):
					for ty in range(8):
						var start = Vector2(x, y)
						var target = Vector2(tx, ty)
						
						# 1. Geometry Check
						if is_valid_geometry(piece, start, target):
							# 2. Safety Check (King Safety)
							if not does_move_cause_self_check(piece, start, target):
								valid_moves.append({
									"piece": piece,
									"start": start,
									"end": target
								})
	return valid_moves
