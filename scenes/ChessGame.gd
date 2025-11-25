extends Node2D

# --- CONFIGURATION ---
const TILE_SIZE = 16   # Size of one square in pixels (adjust to match your board.png)
const BOARD_OFFSET = Vector2(8, 8) # Offset to center pieces in squares (half of TILE_SIZE)

# Piece Codes: 
# Color: 0 = White, 1 = Black
# Type: p=Pawn, r=Rook, n=Knight, b=Bishop, q=Queen, k=King
# We will store pieces as dictionaries: { "type": "p", "color": 0, "sprite": Sprite2D }

var board = [] # 8x8 Grid storing piece data or null
var selected_piece = null # The piece currently selected by the player
var turn = 0 # 0 for White, 1 for Black

# Preload Textures (Assuming filenames like p0.jpg, k1.jpg)
var textures = {
	0: { "p": preload("res://assets/p0.png"), "r": preload("res://assets/r0.png"), "n": preload("res://assets/n0.png"), 
		 "b": preload("res://assets/b0.png"), "q": preload("res://assets/q0.png"), "k": preload("res://assets/k0.png") },
	1: { "p": preload("res://assets/p1.png"), "r": preload("res://assets/r1.png"), "n": preload("res://assets/n1.png"), 
		 "b": preload("res://assets/b1.png"), "q": preload("res://assets/q1.png"), "k": preload("res://assets/k1.png") }
}

func _ready():
	# Initialize the 8x8 board with nulls
	for x in range(8):
		board.append([])
		for y in range(8):
			board[x].append(null)
			
	setup_board()

func setup_board():
	# Setup Black Pieces (Top, rows 0 and 1)
	spawn_piece(0, 0, "r", 1); spawn_piece(1, 0, "n", 1); spawn_piece(2, 0, "b", 1); spawn_piece(3, 0, "q", 1)
	spawn_piece(4, 0, "k", 1); spawn_piece(5, 0, "b", 1); spawn_piece(6, 0, "n", 1); spawn_piece(7, 0, "r", 1)
	for x in range(8): spawn_piece(x, 1, "p", 1)

	# Setup White Pieces (Bottom, rows 6 and 7)
	spawn_piece(0, 7, "r", 0); spawn_piece(1, 7, "n", 0); spawn_piece(2, 7, "b", 0); spawn_piece(3, 7, "q", 0)
	spawn_piece(4, 7, "k", 0); spawn_piece(5, 7, "b", 0); spawn_piece(6, 7, "n", 0); spawn_piece(7, 7, "r", 0)
	for x in range(8): spawn_piece(x, 6, "p", 0)

func spawn_piece(x, y, type, color):
	var piece = {
		"type": type,
		"color": color,
		"sprite": Sprite2D.new(),
		"has_moved": false # Useful for pawns moving 2 steps
	}
	
	# Configure Sprite
	piece.sprite.texture = textures[color][type]
	piece.sprite.position = grid_to_pixel(Vector2(x, y))
	# Optional: Scale down if your images are too big
	# piece.sprite.scale = Vector2(0.5, 0.5) 
	
	$Pieces.add_child(piece.sprite)
	board[x][y] = piece

func _input(event):
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		var clicked_pos = pixel_to_grid(event.position)
		
		# Check if click is inside board bounds
		if clicked_pos.x < 0 or clicked_pos.x > 7 or clicked_pos.y < 0 or clicked_pos.y > 7:
			return

		if selected_piece:
			# Try to move
			try_move_piece(selected_piece, clicked_pos)
			# Deselect after move attempt (successful or not)
			selected_piece.sprite.modulate = Color(1, 1, 1) # Reset color
			selected_piece = null
		else:
			# Select a piece
			var piece = board[clicked_pos.x][clicked_pos.y]
			if piece and piece.color == turn:
				selected_piece = piece
				selected_piece.sprite.modulate = Color(0.5, 1, 0.5) # Highlight Green

func try_move_piece(piece, target_pos):
	var start_pos = pixel_to_grid(piece.sprite.position)
	
	# 1. Check if move is valid for this piece type
	if is_valid_move(piece, start_pos, target_pos):
		
		# 2. Check for capturing
		var target_piece = board[target_pos.x][target_pos.y]
		if target_piece:
			if target_piece.color == piece.color:
				return # Cannot capture own piece
			else:
				# Capture! Remove sprite and data
				target_piece.sprite.queue_free()
				board[target_pos.x][target_pos.y] = null

		# 3. Move the piece
		board[start_pos.x][start_pos.y] = null
		board[target_pos.x][target_pos.y] = piece
		piece.sprite.position = grid_to_pixel(target_pos)
		piece.has_moved = true
		
		# 4. Switch Turn
		turn = 1 - turn # Toggles between 0 and 1
		print("Turn switched. Now: ", "White" if turn == 0 else "Black")

func is_valid_move(piece, start, end):
	var dx = end.x - start.x
	var dy = end.y - start.y
	var abs_dx = abs(dx)
	var abs_dy = abs(dy)
	
	# Cannot move to same square
	if start == end: return false
	
	match piece.type:
		"p": # PAWN
			var direction = -1 if piece.color == 0 else 1 # White moves up (-1), Black down (+1)
			var target_piece = board[end.x][end.y]
			
			# Move forward 1
			if dx == 0 and dy == direction and target_piece == null:
				return true
			# Move forward 2 (first move)
			if dx == 0 and dy == direction * 2 and piece.has_moved == false and target_piece == null:
				# Check path is clear
				if board[start.x][start.y + direction] == null:
					return true
			# Capture Diagonal
			if abs_dx == 1 and dy == direction and target_piece != null and target_piece.color != piece.color:
				return true
			return false

		"r": # ROOK (Straight lines)
			if dx != 0 and dy != 0: return false # Must be straight
			return is_path_clear(start, end)

		"b": # BISHOP (Diagonals)
			if abs_dx != abs_dy: return false # Must be diagonal
			return is_path_clear(start, end)

		"n": # KNIGHT (L-shape)
			return (abs_dx == 2 and abs_dy == 1) or (abs_dx == 1 and abs_dy == 2)

		"q": # QUEEN (Rook + Bishop)
			if dx == 0 or dy == 0 or abs_dx == abs_dy:
				return is_path_clear(start, end)
			return false

		"k": # KING (1 step any direction)
			return abs_dx <= 1 and abs_dy <= 1

	return false

func is_path_clear(start, end):
	var dx = sign(end.x - start.x)
	var dy = sign(end.y - start.y)
	var current = start + Vector2(dx, dy) # Start one step ahead
	
	while current != end:
		# If we hit a piece, path is blocked
		if current.x >= 0 and current.x < 8 and current.y >= 0 and current.y < 8: # Bounds check
			if board[current.x][current.y] != null:
				return false
		current += Vector2(dx, dy)
	return true

# --- HELPERS ---
func grid_to_pixel(grid_pos):
	return grid_pos * TILE_SIZE + BOARD_OFFSET

func pixel_to_grid(pixel_pos):
	return Vector2(floor(pixel_pos.x / TILE_SIZE), floor(pixel_pos.y / TILE_SIZE))
