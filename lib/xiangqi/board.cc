#include "xiangqi/board.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "xiangqi/board_c.h"
#include "xiangqi/internal/moves.h"
#include "xiangqi/types.h"

namespace xq {

namespace {

bool IsPathClear(const Board& board, const Position from, const Position to) {
  const uint8_t start = std::min(from, to);
  const uint8_t end = std::max(from, to);
  if (end - start < K_TOTAL_COL) {  // same row
    for (uint8_t pos = start + 1; pos < end; pos++) {
      if (board[pos] != PIECE_EMPTY) {
        return false;
      }
    }
    return true;
  } else if (Col(from) == Col(to)) {
    for (uint8_t pos = start + K_TOTAL_COL; pos < end; pos += K_TOTAL_COL) {
      if (board[pos] != PIECE_EMPTY) {
        return false;
      }
    }
    return true;
  }
  return false;
}

bool ThreatensBySoldier(const Piece soldier, const Position pos,
                        const Position target) {
  const uint8_t pos_col = Col(pos);
  return ((soldier == R_SOLDIER) &&
          ((pos == target + K_TOTAL_COL) ||                    // up
           (pos < K_RED_RIVER_START &&                         // crossed river
            ((pos_col < K_TOTAL_COL - 1 && pos + 1 == target)  // right
             || (pos_col > 0 && target + 1 == pos)             // left
             )))) ||
         ((soldier == B_SOLDIER) &&
          ((pos + K_TOTAL_COL == target) ||                    // down
           (pos >= K_RED_RIVER_START &&                        // crossed river
            ((pos_col < K_TOTAL_COL - 1 && pos + 1 == target)  // right
             || (pos_col > 0 && target + 1 == pos)             // left
             ))));
}

bool ThreatensByHorse(const Board& board, const Position pos,
                      const Position target) {
  const uint8_t pos_row = Row(pos);
  const uint8_t pos_col = Col(pos);
  if ((target > pos + K_TOTAL_COL)    // target at least one row below
      && (pos_row < K_TOTAL_ROW - 2)  // can move down for 2 rows
      && board[pos + K_TOTAL_COL] == PIECE_EMPTY  // not blocked
  ) {
    const uint8_t down_2_row = pos + K_TOTAL_COL * 2;
    if ((pos_col > 0 && down_2_row - 1 == target)                   // left 1
        || (pos_col < K_TOTAL_COL - 1 && down_2_row + 1 == target)  // right 1
    ) {
      return true;
    }
  } else if ((target + K_TOTAL_COL < pos)  // target at least one row above
             && (pos_row > 1)              // can move up for 2 rows
             && (board[pos - K_TOTAL_COL] == PIECE_EMPTY)  // not blocked
  ) {
    const uint8_t up_2_row = pos - K_TOTAL_COL * 2;
    if ((pos_col > 0 && up_2_row == target + 1)                   // left 1
        || (pos_col < K_TOTAL_COL - 1 && up_2_row + 1 == target)  // right 1
    ) {
      return true;
    }
  }
  return (pos_col < K_TOTAL_COL - 2         // can move right for 2 columns
          && board[pos + 1] == PIECE_EMPTY  // not blocked
          && ((pos_row < K_TOTAL_ROW - 1 &&
               pos + K_TOTAL_COL + 2 == target)                    // down 1
              || (pos_row > 0 && target + K_TOTAL_COL - 2 == pos)  // up 1
              )) ||
         (pos_col > 1                       // can move left for 2 columns
          && board[pos - 1] == PIECE_EMPTY  // not blocked
          && ((pos_row < K_TOTAL_ROW - 1 &&
               pos + K_TOTAL_COL - 2 == target)                    // down 1
              || (pos_row > 0 && target + K_TOTAL_COL + 2 == pos)  // up 1
              ));
}

bool ThreatensByCannon(const Board& board, const Position pos,
                       const Position target) {
  if (Row(pos) == Row(target)) {
    bool found_in_between = false;
    for (uint8_t p = std::min(pos, target) + 1; p < std::max(pos, target);
         p++) {
      if (board[p] != PIECE_EMPTY) {
        if (found_in_between) {
          return false;
        } else {
          found_in_between = true;
        }
      }
    }
    return found_in_between;
  } else if (Col(pos) == Col(target)) {
    bool found_in_between = false;
    for (uint8_t p = std::min(pos, target) + K_TOTAL_COL;
         p < std::max(pos, target); p += K_TOTAL_COL) {
      if (board[p] != PIECE_EMPTY) {
        if (found_in_between) {
          return false;
        } else {
          found_in_between = true;
        }
      }
    }
    return found_in_between;
  }
  return false;
}

char PieceToCh(const Piece piece, const uint8_t row, const uint8_t col) {
  switch (piece) {
    case PIECE_EMPTY:
      if (row == 4 || row == 5) {
        return '-';
      } else if (col >= 3 && col <= 5 &&
                 ((row >= 0 && row <= 2) || (row >= 7 && row <= 9))) {
        return '*';
      } else {
        return '.';
      }
    case R_GENERAL:
      return 'G';
    case R_ADVISOR:
      return 'A';
    case R_ELEPHANT:
      return 'E';
    case R_HORSE:
      return 'H';
    case R_CHARIOT:
      return 'R';
    case R_CANNON:
      return 'C';
    case R_SOLDIER:
      return 'S';
    case B_GENERAL:
      return 'g';
    case B_ADVISOR:
      return 'a';
    case B_ELEPHANT:
      return 'e';
    case B_HORSE:
      return 'h';
    case B_CHARIOT:
      return 'r';
    case B_CANNON:
      return 'c';
    case B_SOLDIER:
      return 's';
    default:
      return '?';
  }
}

Piece ChToPiece(const char ch) {
  switch (ch) {
    case '.':
      return PIECE_EMPTY;
    case 'G':
      return R_GENERAL;
    case 'A':
      return R_ADVISOR;
    case 'E':
      return R_ELEPHANT;
    case 'H':
      return R_HORSE;
    case 'R':
      return R_CHARIOT;
    case 'C':
      return R_CANNON;
    case 'S':
      return R_SOLDIER;
    case 'g':
      return B_GENERAL;
    case 'a':
      return B_ADVISOR;
    case 'e':
      return B_ELEPHANT;
    case 'h':
      return B_HORSE;
    case 'r':
      return B_CHARIOT;
    case 'c':
      return B_CANNON;
    case 's':
      return B_SOLDIER;
    default:
      return PIECE_EMPTY;
  }
}

}  // namespace

Board BoardFromString(const std::string_view str) {
  Board result;
  result.fill(PIECE_EMPTY);
  size_t idx = 23;
  for (uint8_t row = 0; row < K_TOTAL_ROW; row++) {
    for (uint8_t col = 0; col < K_TOTAL_COL; col++) {
      result[Pos(row, col)] = ChToPiece(str[idx]);
      idx += 2;
    }
    idx += 3;
  }
  return result;
}

std::string BoardToString(const Board& board) {
  std::string result;
  result.reserve(242);
  result.append("  A B C D E F G H I \n");
  for (uint8_t row = 0; row < K_TOTAL_ROW; row++) {
    result.append(std::to_string(row));
    result.append(" ");
    for (uint8_t col = 0; col < K_TOTAL_COL; col++) {
      result += PieceToCh(board[Pos(row, col)], row, col);
      result.append(" ");
    }
    result.append("\n");
  }
  return result;
}

bool BoardEq(const Board& a, const Board& b) {
  for (Position pos = 0; pos < K_BOARD_SIZE; pos++) {
    if (a[pos] != b[pos]) {
      return false;
    }
  }
  return true;
}

bool operator==(const Board& lhs, const Board& rhs) {
  return BoardEq(lhs, rhs);
}

bool IsBeingCheckmate(const Board& board, Player player) {
  const Piece general = (player == PLAYER_RED) ? R_GENERAL : B_GENERAL;
  const Position general_pos = FindGeneral(board, player);
  if (general_pos == K_NO_POSITION) {
    return true;
  }

  // Now scan the board for enemy pieces that might be threatening our
  // general.
  for (uint8_t pos = 0; pos < K_BOARD_SIZE; pos++) {
    const Piece piece = board[pos];
    if (piece == PIECE_EMPTY) {
      continue;
    }
    const bool piece_is_red = IsRed(piece);
    const bool player_is_red = player == PLAYER_RED;
    if (piece_is_red == player_is_red) {
      continue;  // Skip own piece
    }

    switch (std::abs(static_cast<std::underlying_type_t<Piece>>(piece))) {
      case static_cast<std::underlying_type_t<Piece>>(R_GENERAL):
      case static_cast<std::underlying_type_t<Piece>>(R_CHARIOT):
        if (IsPathClear(board, pos, general_pos)) {
          return true;
        }
        break;
      case static_cast<std::underlying_type_t<Piece>>(R_SOLDIER):
        if (ThreatensBySoldier(piece, pos, general_pos)) {
          return true;
        }
        break;
      case static_cast<std::underlying_type_t<Piece>>(R_HORSE):
        if (ThreatensByHorse(board, pos, general_pos)) {
          return true;
        }
        break;
      case static_cast<std::underlying_type_t<Piece>>(R_CANNON):
        if (ThreatensByCannon(board, pos, general_pos)) {
          return true;
        }
        break;
      default:
        continue;
    }
  }
  return false;
}

Winner GetWinner(const Board& board) {
  const Position black_general_pos = FindGeneral(board, PLAYER_BLACK);
  if (black_general_pos == K_NO_POSITION) {
    return WINNER_RED;
  }
  const Position red_general_pos = FindGeneral(board, PLAYER_RED);
  if (red_general_pos == K_NO_POSITION) {
    return WINNER_BLACK;
  }
  return WINNER_NONE;
}

bool DidPlayerLose(const Board& board, Player player) {
  using namespace xq::internal::util;

  const Winner opponent = player == PLAYER_RED ? WINNER_BLACK : WINNER_RED;
  const Piece opponent_general = player == PLAYER_RED ? B_GENERAL : R_GENERAL;
  if (GetWinner(board) == opponent) {
    return true;
  }

  for (Position pos = 0; pos < K_BOARD_SIZE; pos++) {
    const Piece piece = board[pos];
    if (piece == PIECE_EMPTY || IsRed(piece) != (player == PLAYER_RED)) {
      continue;
    }
    switch (piece) {
      case R_GENERAL:
        for (const Position to : PossibleMovesGeneral(
                 board, pos, FindGeneral(board, PLAYER_BLACK))) {
          if (to == K_NO_POSITION) {
            break;
          }
          Board next = board;
          const Piece capture = Move(next, NewMovement(pos, to));
          if (capture == opponent_general || !IsBeingCheckmate(next, player)) {
            return false;
          }
        }
        break;
      case B_GENERAL:
        for (const Position to :
             PossibleMovesGeneral(board, pos, FindGeneral(board, PLAYER_RED))) {
          if (to == K_NO_POSITION) {
            break;
          }
          Board next = board;
          const Piece capture = Move(next, NewMovement(pos, to));
          if (capture == opponent_general || !IsBeingCheckmate(next, player)) {
            return false;
          }
        }
        break;
      case R_ADVISOR:
      case B_ADVISOR:
        for (const Position to : PossibleMovesAdvisor(board, pos)) {
          if (to == K_NO_POSITION) {
            break;
          }
          Board next = board;
          const Piece capture = Move(next, NewMovement(pos, to));
          if (capture == opponent_general || !IsBeingCheckmate(next, player)) {
            return false;
          }
        }
        break;
      case R_ELEPHANT:
      case B_ELEPHANT:
        for (const Position to : PossibleMovesElephant(board, pos)) {
          if (to == K_NO_POSITION) {
            break;
          }
          Board next = board;
          const Piece capture = Move(next, NewMovement(pos, to));
          if (capture == opponent_general || !IsBeingCheckmate(next, player)) {
            return false;
          }
        }
        break;
      case R_HORSE:
      case B_HORSE:
        for (const Position to : PossibleMovesHorse(board, pos)) {
          if (to == K_NO_POSITION) {
            break;
          }
          Board next = board;
          const Piece capture = Move(next, NewMovement(pos, to));
          if (capture == opponent_general || !IsBeingCheckmate(next, player)) {
            return false;
          }
        }
        break;
      case R_CHARIOT:
      case B_CHARIOT:
        for (const Position to : PossibleMovesChariot(board, pos)) {
          if (to == K_NO_POSITION) {
            break;
          }
          Board next = board;
          const Piece capture = Move(next, NewMovement(pos, to));
          if (capture == opponent_general || !IsBeingCheckmate(next, player)) {
            return false;
          }
        }
        break;
      case R_CANNON:
      case B_CANNON:
        for (const Position to : PossibleMovesCannon(board, pos)) {
          if (to == K_NO_POSITION) {
            break;
          }
          Board next = board;
          const Piece capture = Move(next, NewMovement(pos, to));
          if (capture == opponent_general || !IsBeingCheckmate(next, player)) {
            return false;
          }
        }
        break;
      case R_SOLDIER:
      case B_SOLDIER:
        for (const Position to : PossibleMovesSoldier(board, pos)) {
          if (to == K_NO_POSITION) {
            break;
          }
          Board next = board;
          const Piece capture = Move(next, NewMovement(pos, to));
          if (capture == opponent_general || !IsBeingCheckmate(next, player)) {
            return false;
          }
        }
        break;
      default:
        continue;
    }
  }
  return true;
}

Board FlipBoard(const Board& board) {
  Board result = board;
  for (uint8_t pos = 0; pos < K_BOARD_SIZE / 2; pos++) {
    const uint8_t pos_mirror = K_BOARD_SIZE - 1 - pos;
    const Piece left_flipped = static_cast<Piece>(
        -static_cast<std::underlying_type_t<Piece>>(board[pos]));
    result[pos] = static_cast<Piece>(
        -static_cast<std::underlying_type_t<Piece>>(board[pos_mirror]));
    result[pos_mirror] = left_flipped;
  }
  return result;
}

Board MirrorBoardHorizontal(const Board& board) {
  Board result = board;
  for (uint8_t row = 0; row < K_TOTAL_ROW; row++) {
    const Position row_start = row * K_TOTAL_COL;
    for (uint8_t col = 0; col < K_TOTAL_COL / 2; col++) {
      const Position left_pos = row_start + col;
      const Position right_pos = row_start + K_TOTAL_COL - 1 - col;
      const Piece left_piece = board[left_pos];
      result[left_pos] = board[right_pos];
      result[right_pos] = left_piece;
    }
  }
  return result;
}

Board MirrorBoardVertical(const Board& board) {
  Board result = board;
  result.fill(PIECE_EMPTY);
  for (uint8_t row = 0; row < K_TOTAL_ROW / 2; row++) {
    const size_t top_start = row * K_TOTAL_COL;
    const size_t bottom_start = (K_TOTAL_ROW - 1 - row) * K_TOTAL_COL;
    std::memcpy(result.data() + top_start, board.data() + bottom_start,
                K_TOTAL_COL);
    std::memcpy(result.data() + bottom_start, board.data() + top_start,
                K_TOTAL_COL);
    for (uint8_t col = 0; col < K_TOTAL_COL; col++) {
      result[top_start + col] = Piece(
          -static_cast<std::underlying_type_t<Piece>>(result[top_start + col]));
      result[bottom_start + col] =
          Piece(-static_cast<std::underlying_type_t<Piece>>(
              result[bottom_start + col]));
    }
  }
  return result;
}

BoardState EncodeBoardState(const Board& board) {
  uint64_t res1 = 0, res2 = 0, res3 = 0, res4 = 0;

  // Initialize all pieces as missing.
  uint8_t r_general_pos = 0xFF, b_general_pos = 0xFF;
  uint16_t r_advisor_poses = 0xFFFF, r_elephant_poses = 0xFFFF,
           r_horse_poses = 0xFFFF, r_chariot_poses = 0xFFFF,
           r_cannon_poses = 0xFFFF, b_advisor_poses = 0xFFFF,
           b_elephant_poses = 0xFFFF, b_horse_poses = 0xFFFF,
           b_chariot_poses = 0xFFFF, b_cannon_poses = 0xFFFF;

  std::array<uint8_t, 5> r_soldier_poses{};
  r_soldier_poses.fill(0xFF);
  std::array<uint8_t, 5> b_soldier_poses{};
  b_soldier_poses.fill(0xFF);
  uint8_t r_soldier_idx = 0, b_soldier_idx = 0;

  for (Position pos = 0; pos < K_BOARD_SIZE; pos++) {
    switch (board[pos]) {
      case PIECE_EMPTY: {
        continue;
      }
      case R_GENERAL: {
        r_general_pos = pos;
        break;
      }
      case B_GENERAL: {
        b_general_pos = pos;
        break;
      }
      case R_ADVISOR: {
        const uint16_t left_byte = r_advisor_poses & 0xFF00;
        const uint16_t cur_min = left_byte >> 8;
        r_advisor_poses = (pos < cur_min)
                              ? (static_cast<uint16_t>(pos << 8) | cur_min)
                              : (left_byte | pos);
        break;
      }
      case B_ADVISOR: {
        const uint16_t left_byte = b_advisor_poses & 0xFF00;
        const uint16_t cur_min = left_byte >> 8;
        b_advisor_poses = (pos < cur_min)
                              ? (static_cast<uint16_t>(pos << 8) | cur_min)
                              : (left_byte | pos);
        break;
      }
      case R_ELEPHANT: {
        const uint16_t left_byte = r_elephant_poses & 0xFF00;
        const uint16_t cur_min = left_byte >> 8;
        r_elephant_poses = (pos < cur_min)
                               ? (static_cast<uint16_t>(pos << 8) | cur_min)
                               : (left_byte | pos);
        break;
      }
      case B_ELEPHANT: {
        const uint16_t left_byte = b_elephant_poses & 0xFF00;
        const uint16_t cur_min = left_byte >> 8;
        b_elephant_poses = (pos < cur_min)
                               ? (static_cast<uint16_t>(pos << 8) | cur_min)
                               : (left_byte | pos);
        break;
      }
      case R_HORSE: {
        const uint16_t left_byte = r_horse_poses & 0xFF00;
        const uint16_t cur_min = left_byte >> 8;
        r_horse_poses = (pos < cur_min)
                            ? (static_cast<uint16_t>(pos << 8) | cur_min)
                            : (left_byte | pos);
        break;
      }
      case B_HORSE: {
        const uint16_t left_byte = b_horse_poses & 0xFF00;
        const uint16_t cur_min = left_byte >> 8;
        b_horse_poses = (pos < cur_min)
                            ? (static_cast<uint16_t>(pos << 8) | cur_min)
                            : (left_byte | pos);
        break;
      }
      case R_CHARIOT: {
        const uint16_t left_byte = r_chariot_poses & 0xFF00;
        const uint16_t cur_min = left_byte >> 8;
        r_chariot_poses = (pos < cur_min)
                              ? (static_cast<uint16_t>(pos << 8) | cur_min)
                              : (left_byte | pos);
        break;
      }
      case B_CHARIOT: {
        const uint16_t left_byte = b_chariot_poses & 0xFF00;
        const uint16_t cur_min = left_byte >> 8;
        b_chariot_poses = (pos < cur_min)
                              ? (static_cast<uint16_t>(pos << 8) | cur_min)
                              : (left_byte | pos);
        break;
      }
      case R_CANNON: {
        const uint16_t left_byte = r_cannon_poses & 0xFF00;
        const uint16_t cur_min = left_byte >> 8;
        r_cannon_poses = (pos < cur_min)
                             ? (static_cast<uint16_t>(pos << 8) | cur_min)
                             : (left_byte | pos);
        break;
      }
      case B_CANNON: {
        const uint16_t left_byte = b_cannon_poses & 0xFF00;
        const uint16_t cur_min = left_byte >> 8;
        b_cannon_poses = (pos < cur_min)
                             ? (static_cast<uint16_t>(pos << 8) | cur_min)
                             : (left_byte | pos);
        break;
      }
      case R_SOLDIER:
        r_soldier_poses[r_soldier_idx++] = pos;
        break;
      case B_SOLDIER:
        b_soldier_poses[b_soldier_idx++] = pos;
        break;
    }
  }

  std::sort(r_soldier_poses.begin(), r_soldier_poses.end());
  std::sort(b_soldier_poses.begin(), b_soldier_poses.end());

  res1 |= static_cast<uint64_t>(r_general_pos) << (7 * 8);
  res3 |= static_cast<uint64_t>(b_general_pos) << (7 * 8);
  res1 |= static_cast<uint64_t>(r_advisor_poses) << (5 * 8);
  res3 |= static_cast<uint64_t>(b_advisor_poses) << (5 * 8);
  res1 |= static_cast<uint64_t>(r_elephant_poses) << (3 * 8);
  res3 |= static_cast<uint64_t>(b_elephant_poses) << (3 * 8);
  res1 |= static_cast<uint64_t>(r_horse_poses) << 8;
  res3 |= static_cast<uint64_t>(b_horse_poses) << 8;
  res1 |= static_cast<uint64_t>(r_chariot_poses >> 8);
  res3 |= static_cast<uint64_t>(b_chariot_poses >> 8);

  res2 |= static_cast<uint64_t>(r_chariot_poses & 0x00FF) << (7 * 8);
  res4 |= static_cast<uint64_t>(b_chariot_poses & 0x00FF) << (7 * 8);

  res2 |= static_cast<uint64_t>(r_cannon_poses) << (5 * 8);
  res4 |= static_cast<uint64_t>(b_cannon_poses) << (5 * 8);

  res2 |= (static_cast<uint64_t>(r_soldier_poses[0]) << (4 * 8)) |
          (static_cast<uint64_t>(r_soldier_poses[1]) << (3 * 8)) |
          (static_cast<uint64_t>(r_soldier_poses[2]) << (2 * 8)) |
          (static_cast<uint64_t>(r_soldier_poses[3]) << 8) |
          static_cast<uint64_t>(r_soldier_poses[4]);
  res4 |= (static_cast<uint64_t>(b_soldier_poses[0]) << (4 * 8)) |
          (static_cast<uint64_t>(b_soldier_poses[1]) << (3 * 8)) |
          (static_cast<uint64_t>(b_soldier_poses[2]) << (2 * 8)) |
          (static_cast<uint64_t>(b_soldier_poses[3]) << 8) |
          static_cast<uint64_t>(b_soldier_poses[4]);

  return {res1, res2, res3, res4};
}

Board DecodeBoardState(const BoardState& state) {
  // Initialize an empty board.
  Board board;
  board.fill(PIECE_EMPTY);

  // For red pieces, the encoded state is split between state[0] (res1) and
  // state[1] (res2).
  uint64_t res1 = state[0];
  uint64_t res2 = state[1];

  // --- Red Pieces ---

  // Red General: bits 56-63 of res1.
  uint8_t r_general = (res1 >> 56) & 0xFF;
  if (r_general != 0xFF) {
    board[r_general] = R_GENERAL;
  }

  // Red Advisors: bits 48-55 and 40-47 of res1.
  uint8_t r_adv1 = (res1 >> 48) & 0xFF;
  if (r_adv1 != 0xFF) {
    board[r_adv1] = R_ADVISOR;
  }
  uint8_t r_adv2 = (res1 >> 40) & 0xFF;
  if (r_adv2 != 0xFF) {
    board[r_adv2] = R_ADVISOR;
  }

  // Red Elephants: bits 32-39 and 24-31 of res1.
  uint8_t r_ele1 = (res1 >> 32) & 0xFF;
  if (r_ele1 != 0xFF) {
    board[r_ele1] = R_ELEPHANT;
  }
  uint8_t r_ele2 = (res1 >> 24) & 0xFF;
  if (r_ele2 != 0xFF) {
    board[r_ele2] = R_ELEPHANT;
  }

  // Red Horses: bits 16-23 and 8-15 of res1.
  uint8_t r_horse1 = (res1 >> 16) & 0xFF;
  if (r_horse1 != 0xFF) {
    board[r_horse1] = R_HORSE;
  }
  uint8_t r_horse2 = (res1 >> 8) & 0xFF;
  if (r_horse2 != 0xFF) {
    board[r_horse2] = R_HORSE;
  }

  // Red Chariots:
  //   - One chariot: bits 0-7 of res1.
  uint8_t r_chariot1 = res1 & 0xFF;
  if (r_chariot1 != 0xFF) {
    board[r_chariot1] = R_CHARIOT;
  }
  //   - The other chariot: bits 56-63 of res2.
  uint8_t r_chariot2 = (res2 >> 56) & 0xFF;
  if (r_chariot2 != 0xFF) {
    board[r_chariot2] = R_CHARIOT;
  }

  // Red Cannons: bits 48-55 and 40-47 of res2.
  uint8_t r_cannon1 = (res2 >> 48) & 0xFF;
  if (r_cannon1 != 0xFF) {
    board[r_cannon1] = R_CANNON;
  }
  uint8_t r_cannon2 = (res2 >> 40) & 0xFF;
  if (r_cannon2 != 0xFF) {
    board[r_cannon2] = R_CANNON;
  }

  // Red Soldiers: bits 32-39, 24-31, 16-23, 8-15, and 0-7 of res2.
  uint8_t r_soldier[5];
  r_soldier[0] = (res2 >> 32) & 0xFF;
  r_soldier[1] = (res2 >> 24) & 0xFF;
  r_soldier[2] = (res2 >> 16) & 0xFF;
  r_soldier[3] = (res2 >> 8) & 0xFF;
  r_soldier[4] = res2 & 0xFF;
  for (int i = 0; i < 5; ++i) {
    if (r_soldier[i] != 0xFF) {
      const Position pos = r_soldier[i];
      board[pos] = R_SOLDIER;
    }
  }

  // For black pieces, the encoded state is in state[2] (res3) and state[3]
  // (res4).
  uint64_t res3 = state[2];
  uint64_t res4 = state[3];

  // --- Black Pieces ---

  // Black General: bits 56-63 of res3.
  uint8_t b_general = (res3 >> 56) & 0xFF;
  if (b_general != 0xFF) {
    board[b_general] = B_GENERAL;
  }

  // Black Advisors: bits 48-55 and 40-47 of res3.
  uint8_t b_adv1 = (res3 >> 48) & 0xFF;
  if (b_adv1 != 0xFF) {
    board[b_adv1] = B_ADVISOR;
  }
  uint8_t b_adv2 = (res3 >> 40) & 0xFF;
  if (b_adv2 != 0xFF) {
    board[b_adv2] = B_ADVISOR;
  }

  // Black Elephants: bits 32-39 and 24-31 of res3.
  uint8_t b_ele1 = (res3 >> 32) & 0xFF;
  if (b_ele1 != 0xFF) {
    board[b_ele1] = B_ELEPHANT;
  }
  uint8_t b_ele2 = (res3 >> 24) & 0xFF;
  if (b_ele2 != 0xFF) {
    board[b_ele2] = B_ELEPHANT;
  }

  // Black Horses: bits 16-23 and 8-15 of res3.
  uint8_t b_horse1 = (res3 >> 16) & 0xFF;
  if (b_horse1 != 0xFF) {
    board[b_horse1] = B_HORSE;
  }
  uint8_t b_horse2 = (res3 >> 8) & 0xFF;
  if (b_horse2 != 0xFF) {
    board[b_horse2] = B_HORSE;
  }

  // Black Chariots:
  //   - One from res3: bits 0-7.
  uint8_t b_chariot1 = res3 & 0xFF;
  if (b_chariot1 != 0xFF) {
    board[b_chariot1] = B_CHARIOT;
  }
  //   - The other from res4: bits 56-63.
  uint8_t b_chariot2 = (res4 >> 56) & 0xFF;
  if (b_chariot2 != 0xFF) {
    board[b_chariot2] = B_CHARIOT;
  }

  // Black Cannons: bits 48-55 and 40-47 of res4.
  uint8_t b_cannon1 = (res4 >> 48) & 0xFF;
  if (b_cannon1 != 0xFF) {
    board[b_cannon1] = B_CANNON;
  }
  uint8_t b_cannon2 = (res4 >> 40) & 0xFF;
  if (b_cannon2 != 0xFF) {
    board[b_cannon2] = B_CANNON;
  }

  // Black Soldiers: bits 32-39, 24-31, 16-23, 8-15, and 0-7 of res4.
  uint8_t b_soldier[5];
  b_soldier[0] = (res4 >> 32) & 0xFF;
  b_soldier[1] = (res4 >> 24) & 0xFF;
  b_soldier[2] = (res4 >> 16) & 0xFF;
  b_soldier[3] = (res4 >> 8) & 0xFF;
  b_soldier[4] = res4 & 0xFF;
  for (int i = 0; i < 5; ++i) {
    if (b_soldier[i] != 0xFF) {
      const Position pos = b_soldier[i];
      board[pos] = B_SOLDIER;
    }
  }

  return board;
}

MovesPerPiece PossibleMoves(const Board& board, const Position pos,
                            const bool avoid_checkmate) {
  using namespace xq::internal::util;

  const Piece piece = board[pos];
  MovesPerPiece result;
  result.fill(K_NO_POSITION);
  switch (piece) {
    case PIECE_EMPTY:
      return result;
    case R_GENERAL:
      memcpy(result.data(),
             PossibleMovesGeneral(board, pos, FindGeneral(board, PLAYER_BLACK))
                 .data(),
             5);
      break;
    case B_GENERAL:
      memcpy(result.data(),
             PossibleMovesGeneral(board, pos, FindGeneral(board, PLAYER_RED))
                 .data(),
             5);
      break;
    case R_ADVISOR:
    case B_ADVISOR:
      memcpy(result.data(), PossibleMovesAdvisor(board, pos).data(), 4);
      break;
    case R_ELEPHANT:
    case B_ELEPHANT:
      memcpy(result.data(), PossibleMovesElephant(board, pos).data(), 4);
      break;
    case R_HORSE:
    case B_HORSE:
      memcpy(result.data(), PossibleMovesHorse(board, pos).data(), 8);
      break;
    case R_CHARIOT:
    case B_CHARIOT:
      memcpy(result.data(), PossibleMovesChariot(board, pos).data(), 17);
      break;
    case R_CANNON:
    case B_CANNON:
      memcpy(result.data(), PossibleMovesCannon(board, pos).data(), 17);
      break;
    case R_SOLDIER:
    case B_SOLDIER:
      memcpy(result.data(), PossibleMovesSoldier(board, pos).data(), 3);
      break;
    default:
      return result;
  }
  if (avoid_checkmate) {
    const Player player = IsRed(piece) ? PLAYER_RED : PLAYER_BLACK;
    for (size_t i = 0; i < result.size() && result[i] != K_NO_POSITION; i++) {
      Board next = board;
      const Piece captured = Move(next, NewMovement(pos, result[i]));
      if (captured == R_GENERAL || captured == B_GENERAL) {
        continue;
      }
      if (IsBeingCheckmate(next, player)) {
        result[i] = K_NO_POSITION;
      }
    }
    std::sort(result.begin(), result.end());
  }
  return result;
}

Piece Move(Board& board, const Movement movement) {
  const Position from = Orig(movement);
  const Position to = Dest(movement);
  if (from == to) {
    return PIECE_EMPTY;
  }
  const Piece piece = board[from];
  if (piece == PIECE_EMPTY) {
    return PIECE_EMPTY;
  }
  const Piece captured = board[to];
  board[to] = piece;
  board[from] = PIECE_EMPTY;
  return captured;
}

// Returns a vector of all possible moves for player.
std::vector<uint16_t> AllPossibleNextMoves(const Board& board,
                                           const Player player,
                                           const bool avoid_checkmate) {
  using namespace xq::internal::util;

  std::vector<uint16_t> result;
  for (Position pos = 0; pos < K_BOARD_SIZE; pos++) {
    const Piece piece = board[pos];
    if (piece == PIECE_EMPTY || IsRed(piece) != (player == PLAYER_RED)) {
      continue;
    }
    const auto from_16bit = static_cast<uint16_t>(pos << 8);
    switch (piece) {
      case R_GENERAL:
        for (const Position to : PossibleMovesGeneral(
                 board, pos, FindGeneral(board, PLAYER_BLACK))) {
          if (to == K_NO_POSITION) {
            break;
          }
          result.emplace_back(from_16bit | static_cast<uint16_t>(to));
        }
        break;
      case B_GENERAL:
        for (const Position to :
             PossibleMovesGeneral(board, pos, FindGeneral(board, PLAYER_RED))) {
          if (to == K_NO_POSITION) {
            break;
          }
          result.emplace_back(from_16bit | static_cast<uint16_t>(to));
        }
        break;
      case R_ADVISOR:
      case B_ADVISOR:
        for (const Position to : PossibleMovesAdvisor(board, pos)) {
          if (to == K_NO_POSITION) {
            break;
          }
          result.emplace_back(from_16bit | static_cast<uint16_t>(to));
        }
        break;
      case R_ELEPHANT:
      case B_ELEPHANT:
        for (const Position to : PossibleMovesElephant(board, pos)) {
          if (to == K_NO_POSITION) {
            break;
          }
          result.emplace_back(from_16bit | static_cast<uint16_t>(to));
        }
        break;
      case R_HORSE:
      case B_HORSE:
        for (const Position to : PossibleMovesHorse(board, pos)) {
          if (to == K_NO_POSITION) {
            break;
          }
          result.emplace_back(from_16bit | static_cast<uint16_t>(to));
        }
        break;
      case R_CHARIOT:
      case B_CHARIOT:
        for (const Position to : PossibleMovesChariot(board, pos)) {
          if (to == K_NO_POSITION) {
            break;
          }
          result.emplace_back(from_16bit | static_cast<uint16_t>(to));
        }
        break;
      case R_CANNON:
      case B_CANNON:
        for (const Position to : PossibleMovesCannon(board, pos)) {
          if (to == K_NO_POSITION) {
            break;
          }
          result.emplace_back(from_16bit | static_cast<uint16_t>(to));
        }
        break;
      case R_SOLDIER:
      case B_SOLDIER:
        for (const Position to : PossibleMovesSoldier(board, pos)) {
          if (to == K_NO_POSITION) {
            break;
          }
          result.emplace_back(from_16bit | static_cast<uint16_t>(to));
        }
        break;
      default:
        continue;
    }
  }
  if (avoid_checkmate) {
    std::vector<Movement> final_result;
    for (const Movement move : result) {
      const Player player =
          IsRed(board[Orig(move)]) ? PLAYER_RED : PLAYER_BLACK;
      Board next = board;
      const Piece captured = Move(next, move);
      if (captured == R_GENERAL || captured == B_GENERAL ||
          !IsBeingCheckmate(next, player)) {
        final_result.emplace_back(move);
      }
    }
  }
  return result;
}

std::vector<Board> AllPossibleNextBoards(const Board& board,
                                         const Player player,
                                         const bool avoid_checkmate) {
  const std::vector<uint16_t> possible_moves =
      AllPossibleNextMoves(board, player, avoid_checkmate);
  std::vector<Board> result;
  result.reserve(possible_moves.size());
  for (const uint16_t move : possible_moves) {
    const Position from = static_cast<Position>((move & 0xFF00) >> 8);
    const Position to = static_cast<Position>(move & 0x00FF);
    Board next = board;
    Move(next, move);
    result.emplace_back(std::move(next));
  }
  return result;
}

Position FindGeneral(const Board& board, const Player player) {
  return FindGeneral_C(board.data(), player);
}

}  // namespace xq
