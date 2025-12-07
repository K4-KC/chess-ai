extends "res://scenes/chess_game.gd"

const AI_COLOR = 1
var agent_module = null

func _ready():
	super._ready()
	
	# Check if the class is registered
	if not ClassDB.class_exists("ChessAgent"):
		printerr("CRITICAL ERROR: 'ChessAgent' class not found in ClassDB.")
		printerr("Ensure your GDExtension is compiled and the .gdextension file is correct.")
		return

	# Try to instantiate
	agent_module = ClassDB.instantiate("ChessAgent")
	
	if agent_module:
		add_child(agent_module)
		print("C++ ChessAgent successfully loaded and added to scene.")
	else:
		printerr("CRITICAL ERROR: Failed to instantiate 'ChessAgent'.")


func finalize_turn():
	super.finalize_turn()
	if turn == AI_COLOR:
		call_deferred("perform_ai_turn")

func perform_ai_turn():
	# 1. Get Valid Moves (using existing GDScript logic)
	var raw_moves = get_all_valid_moves(AI_COLOR)
	if raw_moves.is_empty(): return
	
	# 2. Format Moves for C++
	# We only need start/end vectors for the C++ agent to identify the move
	var simple_moves = []
	for m in raw_moves:
		simple_moves.append({ "start": m.start, "end": m.end })
	
	# 3. Format Board for C++
	# Flatten 2D board to 1D integer array
	var simple_board = []
	for y in range(8):
		for x in range(8):
			var p = board[x][y]
			var val = 0
			if p != null:
				# Encoding: White 1-6, Black 7-12
				# Types: p=0, n=1, b=2, r=3, q=4, k=5 (index in array)
				var types = ["p", "n", "b", "r", "q", "k"]
				var type_idx = types.find(p.type)
				val = (type_idx + 1) + (p.color * 6)
			simple_board.append(val)
	
	# 4. Call C++
	var best_move_dict = agent_module.select_best_move(simple_board, simple_moves)
	
	# 5. Execute
	if best_move_dict.has("start"):
		# Find the original full move object to execute (to handle piece references safely)
		for m in raw_moves:
			if m.start == best_move_dict.start and m.end == best_move_dict.end:
				execute_move(m.piece, m.start, m.end)
				break
