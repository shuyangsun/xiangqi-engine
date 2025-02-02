#include <gtest/gtest.h>

#include "xiangqi/game.h"

using namespace xiangqi;

TEST(GameTest, InitialState) {
  Game game;
  EXPECT_EQ(game.PieceAt({9, 4}), R_GENERAL);
  EXPECT_EQ(game.PieceAt({0, 4}), B_GENERAL);
}
