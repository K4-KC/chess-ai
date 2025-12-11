#ifndef BOARD_RULES_H
#define BOARD_RULES_H

// Godot C++ bindings for Node2D, core registration, and core types.
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>

using namespace godot;

// Constants for Bitwise operations
const uint8_t TYPE_MASK = 0x07;
const uint8_t COLOR_MASK = 0x18;
const uint8_t STATE_MASK = 0x20; // Used for "Has Moved" or "En Passant Vulnerable"

enum PieceType {
    EMPTY = 0,
    PAWN = 1,
    KNIGHT = 2,
    BISHOP = 3,
    ROOK = 4,
    QUEEN = 5,
    KING = 6
};

enum PieceColor {
    COLOR_NONE = 0,
    COLOR_WHITE = 8,
    COLOR_BLACK = 16
};

class Board : public Node2D {
    GDCLASS(Board, Node2D)

private:
    // 1D Array of 64 bytes. 
    // Index 0 = a8 (Top Left), Index 63 = h1 (Bottom Right)
    uint8_t squares[64];

    uint8_t turn; // 0 for White, 1 for Black
    
    // State to handle pending promotions
    bool promotion_pending;
    uint8_t promotion_square; // Where the pawn is waiting

    // Helpers
    uint8_t get_type(uint8_t val) const;
    uint8_t get_color_code(uint8_t val) const; // Returns 1 (White) or 2 (Black)
    bool is_enemy(uint8_t me_sq, uint8_t target_sq) const;
    void clear_en_passant_flags(uint8_t color_to_clear);

    // Internal Logic functions
    bool is_square_attacked(uint8_t square, uint8_t by_color); 
    bool is_king_in_check(uint8_t color); 
    
    // Generates moves regardless of checks
    Array get_pseudo_legal_moves_for_piece(uint8_t start_pos);

protected:
    static void _bind_methods();

public:
    Board();
    ~Board();

    uint8_t get_turn(); // Returns 0 for White and 1 for Black
    uint8_t get_piece_on_square(uint8_t pos);

    void setup_board(const String &custom_layout); 

    // 0 = fail, 1 = success, 2 = success with promotion pending.
    uint8_t attempt_move(uint8_t start, uint8_t end); 
    
    // Finalize promotion with type_str ('q','r','b','n'). 0 = fail, 1 = success.
    uint8_t commit_promotion(String type_str); 
    
    Array get_all_possible_moves(uint8_t color);
    Array get_legal_moves_for_piece(uint8_t square);
};

#endif