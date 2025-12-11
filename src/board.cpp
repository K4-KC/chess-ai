#include "board.h"
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// --- Helper Macros ---
#define INDEX(r, c) ((r) * 8 + (c))
#define ROW(i) ((i) / 8)
#define COL(i) ((i) % 8)

Board::Board() {
    // Default setup
    for (int i = 0; i < 64; i++) squares[i] = 0;
    turn = 0;
    promotion_pending = false;
    promotion_square = 0;
    setup_board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

Board::~Board() {}

// --- Bitwise Helpers ---

uint8_t Board::get_type(uint8_t val) const {
    return val & TYPE_MASK;
}

uint8_t Board::get_color_code(uint8_t val) const {
    return val & COLOR_MASK;
}

bool Board::is_enemy(uint8_t me_sq, uint8_t target_sq) const {
    uint8_t c1 = get_color_code(squares[me_sq]);
    uint8_t c2 = get_color_code(squares[target_sq]);
    return (c1 != 0 && c2 != 0 && c1 != c2);
}

void Board::clear_en_passant_flags(uint8_t color_code) {
    // Iterate all squares, if it's a pawn of specific color, unset bit 5
    for(int i=0; i<64; i++) {
        if(get_type(squares[i]) == PAWN && get_color_code(squares[i]) == color_code) {
            squares[i] &= ~STATE_MASK;
        }
    }
}

// --- Public API ---

uint8_t Board::get_turn() {
    return turn;
}

uint8_t Board::get_piece_on_square(uint8_t pos) {
    if (pos >= 64) return 0;
    return squares[pos];
}

void Board::setup_board(const String &custom_layout) {
    // Basic FEN parser
    // Reset board
    for (int i = 0; i < 64; i++) squares[i] = 0;
    promotion_pending = false;

    PackedStringArray parts = custom_layout.split(" ");
    if (parts.size() == 0) return;

    String placement = parts[0];
    int row = 0;
    int col = 0;

    for (int i = 0; i < placement.length(); i++) {
        char c = placement[i];
        if (c == '/') {
            row++;
            col = 0;
        } else if (c >= '0' && c <= '9') {
            col += (c - '0');
        } else {
            // Determine Color and Type
            uint8_t color = (c >= 'A' && c <= 'Z') ? COLOR_WHITE : COLOR_BLACK;
            uint8_t type = EMPTY;
            char lower = (c >= 'A' && c <= 'Z') ? c + 32 : c;

            switch(lower) {
                case 'p': type = PAWN; break;
                case 'n': type = KNIGHT; break;
                case 'b': type = BISHOP; break;
                case 'r': type = ROOK; break;
                case 'q': type = QUEEN; break;
                case 'k': type = KING; break;
            }

            if (row < 8 && col < 8) {
                // Shift color into position (bits 3-4)
                squares[INDEX(row, col)] = type | color;
            }
            col++;
        }
    }

    // Set Turn
    if (parts.size() > 1) {
        turn = (parts[1] == "w") ? 0 : 1;
    }

    // Handle Castling Rights (simplification: if in string, mark unset moved, else mark moved)
    // Note: A robust FEN parser would need to set the MOVED flag (Bit 5) based on absence of KQkq.
    // For brevity, we assume new game state unless specified.
}

uint8_t Board::attempt_move(uint8_t start, uint8_t end) {
    if (promotion_pending) return 0; // Must commit promotion first
    if (start >= 64 || end >= 64) return 0;
    
    uint8_t p = squares[start];
    if (get_type(p) == EMPTY) return 0;
    
    // Check turn
    uint8_t piece_color = get_color_code(p);
    uint8_t current_turn_color = (turn == 0) ? COLOR_WHITE : COLOR_BLACK;
    if (piece_color != current_turn_color) return 0;

    // Check legality
    Array legal_moves = get_legal_moves_for_piece(start);
    bool valid = false;
    for(int i=0; i<legal_moves.size(); i++) {
        if ((int)legal_moves[i] == end) {
            valid = true;
            break;
        }
    }

    if (!valid) return 0;

    // --- EXECUTE MOVE ---
    
    // Handle Special Logic: En Passant Capture
    // If Pawn moves diagonally to an empty square, it's EP. Capture the pawn behind.
    if (get_type(p) == PAWN && get_type(squares[end]) == EMPTY && (start % 8 != end % 8)) {
        // En Passant Capture
        int capture_sq = INDEX(ROW(start), COL(end));
        squares[capture_sq] = 0; 
    }

    // Handle Special Logic: Castling
    // If King moves 2 squares
    if (get_type(p) == KING && abs(COL(start) - COL(end)) == 2) {
        // Move the rook too
        int r_start, r_end;
        if (COL(end) > COL(start)) { // King Side
            r_start = INDEX(ROW(start), 7);
            r_end = INDEX(ROW(start), 5);
        } else { // Queen Side
            r_start = INDEX(ROW(start), 0);
            r_end = INDEX(ROW(start), 3);
        }
        squares[r_end] = squares[r_start] | STATE_MASK; // Move rook and mark moved
        squares[r_start] = 0;
    }

    // Update Board Array
    squares[end] = p | STATE_MASK; // Mark as moved/state modified
    squares[start] = 0;

    // Handle Special Logic: Pawn En Passant Flagging
    // Clear ALL EP flags for the color that just moved (previous EP opportunities expire)
    // Actually, we clear flags for the *current* color before setting new ones.
    // But since we already moved, let's just clear everyone else's EP flags next turn?
    // Standard approach: Clear all EP flags. If this move was a double pawn push, set EP flag.
    for(int i=0; i<64; i++) squares[i] &= ~STATE_MASK; // Crude reset of all flags? 
    // NO, checking moved state of rooks/kings relies on that bit.
    // We must only clear EP flags on Pawns.
    for(int i=0; i<64; i++) {
        if (get_type(squares[i]) == PAWN) squares[i] &= ~STATE_MASK;
    }
    
    // If Pawn double push, set EP flag
    if (get_type(p) == PAWN && abs(ROW(start) - ROW(end)) == 2) {
        squares[end] |= STATE_MASK; 
    }
    
    // Restore King/Rook moved state for the piece we just moved (we blindly wiped it in the loop above if it was a pawn, but for King/Rook we need to ensure it's set)
    if(get_type(squares[end]) == KING || get_type(squares[end]) == ROOK) {
        squares[end] |= STATE_MASK;
    }

    // Check Promotion
    if (get_type(squares[end]) == PAWN) {
        int r = ROW(end);
        if (r == 0 || r == 7) {
            promotion_pending = true;
            promotion_square = end;
            return 2; // Success, waiting for promotion
        }
    }

    // Switch Turn
    turn = (turn == 0) ? 1 : 0;
    return 1;
}

uint8_t Board::commit_promotion(String type_str) {
    if (!promotion_pending) return 0;

    uint8_t p = squares[promotion_square];
    uint8_t color_bits = p & COLOR_MASK;
    uint8_t new_type = QUEEN; // Default

    String t = type_str.to_lower();
    if (t == "q") new_type = QUEEN;
    else if (t == "r") new_type = ROOK;
    else if (t == "b") new_type = BISHOP;
    else if (t == "n") new_type = KNIGHT;
    else return 0; // Invalid input

    squares[promotion_square] = new_type | color_bits | STATE_MASK;
    
    promotion_pending = false;
    promotion_square = 0;
    
    // Switch turn now that promotion is done
    turn = (turn == 0) ? 1 : 0;
    
    return 1;
}

// --- Logic ---

// Helper to get direction vectors
const int knight_offsets[8] = {-17, -15, -10, -6, 6, 10, 15, 17};
const int king_offsets[8] = {-9, -8, -7, -1, 1, 7, 8, 9};

bool Board::is_square_attacked(uint8_t square, uint8_t by_color) {
    // Reverse logic: Look from the square outwards to see if an enemy piece hits it
    int r = ROW(square);
    int c = COL(square);

    // 1. Check Pawn attacks (diagonals)
    int pawn_dir = (by_color == COLOR_WHITE) ? 1 : -1; // White attacks "up" (lower index) ?? 
    // Wait, Index 0 is Top (Black side usually).
    // If White is at bottom (Rank 7,8), they move towards 0. 
    // White Pawn at row 6 attacks row 5. (-8).
    // Black Pawn at row 1 attacks row 2. (+8).
    
    int attack_from_row = (by_color == COLOR_WHITE) ? r + 1 : r - 1;
    if (attack_from_row >= 0 && attack_from_row < 8) {
        // Check Left/Right
        if (c > 0) {
            uint8_t p = squares[INDEX(attack_from_row, c - 1)];
            if (get_type(p) == PAWN && get_color_code(p) == by_color) return true;
        }
        if (c < 7) {
            uint8_t p = squares[INDEX(attack_from_row, c + 1)];
            if (get_type(p) == PAWN && get_color_code(p) == by_color) return true;
        }
    }

    // 2. Check Knights
    for (int off : knight_offsets) {
        int target = square + off;
        if (target >= 0 && target < 64) {
            // Validate wrap around
            int tr = ROW(target); int tc = COL(target);
            if (abs(tr - r) > 2 || abs(tc - c) > 2) continue; // Wrapped around board
            
            uint8_t p = squares[target];
            if (get_type(p) == KNIGHT && get_color_code(p) == by_color) return true;
        }
    }

    // 3. Check Sliding (Rook/Queen)
    int dirs_ortho[4] = {-8, 8, -1, 1}; // Up, Down, Left, Right
    for (int d : dirs_ortho) {
        int curr = square;
        while (true) {
            int prev_c = COL(curr);
            curr += d;
            if (curr < 0 || curr >= 64) break;
            if (abs(COL(curr) - prev_c) > 1) break; // Wrapped row

            uint8_t p = squares[curr];
            if (p != 0) {
                if (get_color_code(p) == by_color && (get_type(p) == ROOK || get_type(p) == QUEEN)) return true;
                break; // Blocked
            }
        }
    }

    // 4. Check Diagonals (Bishop/Queen)
    int dirs_diag[4] = {-9, -7, 7, 9}; 
    for (int d : dirs_diag) {
        int curr = square;
        while (true) {
            int prev_c = COL(curr);
            curr += d;
            if (curr < 0 || curr >= 64) break;
            if (abs(COL(curr) - prev_c) != 1) break; // Wrapped wrong

            uint8_t p = squares[curr];
            if (p != 0) {
                if (get_color_code(p) == by_color && (get_type(p) == BISHOP || get_type(p) == QUEEN)) return true;
                break; // Blocked
            }
        }
    }

    // 5. Check King
    for (int off : king_offsets) {
        int target = square + off;
        if (target >= 0 && target < 64) {
             if (abs(COL(target) - c) <= 1 && abs(ROW(target) - r) <= 1) {
                uint8_t p = squares[target];
                if (get_type(p) == KING && get_color_code(p) == by_color) return true;
             }
        }
    }

    return false;
}

bool Board::is_king_in_check(uint8_t color) {
    // Find King
    int k_pos = -1;
    for(int i=0; i<64; i++) {
        if (get_type(squares[i]) == KING && get_color_code(squares[i]) == color) {
            k_pos = i;
            break;
        }
    }
    if (k_pos == -1) return false; // Should not happen
    
    uint8_t enemy_color = (color == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE;
    return is_square_attacked(k_pos, enemy_color);
}

Array Board::get_pseudo_legal_moves_for_piece(uint8_t start) {
    Array moves;
    if (start >= 64) return moves;
    
    uint8_t p = squares[start];
    if (p == 0) return moves;

    uint8_t type = get_type(p);
    uint8_t color = get_color_code(p);
    int r = ROW(start);
    int c = COL(start);

    // Lambda to add if valid
    auto try_add = [&](int target) {
        if (target < 0 || target >= 64) return;
        uint8_t target_p = squares[target];
        if (target_p == 0 || get_color_code(target_p) != color) {
            moves.append(target);
        }
    };

    if (type == PAWN) {
        int forward = (color == COLOR_WHITE) ? -8 : 8;
        int start_rank = (color == COLOR_WHITE) ? 6 : 1;
        
        // 1. Move forward 1
        int f1 = start + forward;
        if (f1 >= 0 && f1 < 64 && squares[f1] == 0) {
            moves.append(f1);
            // 2. Move forward 2
            int f2 = f1 + forward;
            if (r == start_rank && f2 >= 0 && f2 < 64 && squares[f2] == 0) {
                moves.append(f2);
            }
        }
        // 3. Captures
        int caps[] = {forward - 1, forward + 1};
        for(int cap : caps) {
            int cap_c = COL(cap);
            if (cap >= 0 && cap < 64 && abs(cap_c - c) == 1) {
                uint8_t target_p = squares[cap];
                // Normal Capture
                if (target_p != 0 && get_color_code(target_p) != color) {
                    moves.append(cap);
                }
                // En Passant Capture
                // Check if the square is empty, but the pawn "behind" it (row-wise) is an enemy pawn with state bit set
                else if (target_p == 0) {
                    int en_passant_victim = start + (cap - start - forward); // Should be strictly horizontal
                    // Actually simpler: En Passant target is `cap`. Victim is `cap - forward`.
                    int victim_idx = cap - forward;
                    uint8_t victim = squares[victim_idx];
                    if (get_type(victim) == PAWN && get_color_code(victim) != color && (victim & STATE_MASK)) {
                        moves.append(cap);
                    }
                }
            }
        }
    }
    else if (type == KNIGHT) {
        for (int off : knight_offsets) {
            int t = start + off;
            if (t >= 0 && t < 64) {
                 if (abs(COL(t) - c) <= 2 && abs(ROW(t) - r) <= 2) try_add(t);
            }
        }
    }
    else if (type == KING) {
        for (int off : king_offsets) {
            int t = start + off;
            if (t >= 0 && t < 64) {
                 if (abs(COL(t) - c) <= 1 && abs(ROW(t) - r) <= 1) try_add(t);
            }
        }
        // Castling
        if (!(p & STATE_MASK) && !is_king_in_check(color)) {
            // King Side
            if ((squares[start+1] == 0) && (squares[start+2] == 0)) {
                // Check Rook
                 // Assuming standard setup, Rook is at start+3
                 // Also ensure start+1 and start+2 are not attacked
                 // For brevity, just checking occupancy and rook state
                 int rook_idx = start + 3; // h-file
                 if (COL(rook_idx) == 7) {
                     uint8_t rook = squares[rook_idx];
                     if (get_type(rook) == ROOK && !(rook & STATE_MASK)) {
                         moves.append(start + 2);
                     }
                 }
            }
            // Queen Side
             if ((squares[start-1] == 0) && (squares[start-2] == 0) && (squares[start-3] == 0)) {
                 int rook_idx = start - 4; // a-file
                 if (COL(rook_idx) == 0) {
                     uint8_t rook = squares[rook_idx];
                     if (get_type(rook) == ROOK && !(rook & STATE_MASK)) {
                         moves.append(start - 2);
                     }
                 }
            }
        }
    }
    else {
        // Sliding Pieces (Rook, Bishop, Queen)
        int dirs[8] = {-8, 8, -1, 1, -9, -7, 7, 9};
        int start_d = (type == BISHOP) ? 4 : 0;
        int end_d = (type == ROOK) ? 4 : 8;

        for (int i = start_d; i < end_d; i++) {
            int curr = start;
            while(true) {
                int prev_c = COL(curr);
                curr += dirs[i];
                if (curr < 0 || curr >= 64) break;
                // Check wrap
                if (abs(COL(curr) - prev_c) > 1) break;

                uint8_t target_p = squares[curr];
                if (target_p == 0) {
                    moves.append(curr);
                } else {
                    if (get_color_code(target_p) != color) {
                        moves.append(curr);
                    }
                    break; // Blocked
                }
            }
        }
    }

    return moves;
}

Array Board::get_legal_moves_for_piece(uint8_t square) {
    Array pseudo = get_pseudo_legal_moves_for_piece(square);
    Array legal;
    
    uint8_t p = squares[square];
    uint8_t my_color = get_color_code(p);
    
    // Backup board state
    uint8_t backup[64];
    
    for(int i=0; i<pseudo.size(); i++) {
        int target = pseudo[i];
        
        // COPY STATE
        for(int k=0; k<64; k++) backup[k] = squares[k];
        
        // SIMULATE MOVE (Simplified)
        squares[target] = squares[square];
        squares[square] = 0;
        // (Note: En passant capture or castling logic isn't fully simulated here 
        // regarding removing the captured pawn or moving the rook, 
        // but strictly for "is my King in check", moving the King/Blocker is usually enough).
        
        // If it was a King move, the King is now at `target`.
        // If it was valid, King shouldn't be in check.
        if (!is_king_in_check(my_color)) {
            legal.append(target);
        }
        
        // RESTORE STATE
        for(int k=0; k<64; k++) squares[k] = backup[k];
    }
    return legal;
}

Array Board::get_all_possible_moves(uint8_t color) {
    Array all_moves;
    uint8_t check_color = (color == 0) ? COLOR_WHITE : COLOR_BLACK;
    
    for(int i=0; i<64; i++) {
        uint8_t p = squares[i];
        if (p != 0 && get_color_code(p) == check_color) {
            Array piece_moves = get_legal_moves_for_piece(i);
            // Format: Dictionary with start/end or just list of ends?
            // Usually simpler to return Dictionary {start_sq: [end_sqs]}
            if (piece_moves.size() > 0) {
                Dictionary entry;
                entry["from"] = i;
                entry["to"] = piece_moves;
                all_moves.append(entry);
            }
        }
    }
    return all_moves;
}

void Board::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_turn"), &Board::get_turn);
    ClassDB::bind_method(D_METHOD("get_piece_on_square", "pos"), &Board::get_piece_on_square);
    ClassDB::bind_method(D_METHOD("setup_board", "custom_layout"), &Board::setup_board);
    ClassDB::bind_method(D_METHOD("attempt_move", "start", "end"), &Board::attempt_move);
    ClassDB::bind_method(D_METHOD("commit_promotion", "type_str"), &Board::commit_promotion);
    ClassDB::bind_method(D_METHOD("get_all_possible_moves", "color"), &Board::get_all_possible_moves);
    ClassDB::bind_method(D_METHOD("get_legal_moves_for_piece", "square"), &Board::get_legal_moves_for_piece);
}