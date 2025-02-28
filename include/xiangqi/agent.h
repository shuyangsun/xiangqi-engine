#ifndef XIANGQI_GAME_ENGINE_INCLUDE_XIANGQI_AGENT_H_
#define XIANGQI_GAME_ENGINE_INCLUDE_XIANGQI_AGENT_H_

#include <memory>

#include "xiangqi/types.h"

namespace xq {

class IAgent {
 public:
  virtual ~IAgent() = default;

  virtual uint16_t MakeMove(const Board& board, Player player) const = 0;
};

class AgentFactory {
 public:
  AgentFactory() = delete;
  ~AgentFactory() = delete;

  static std::unique_ptr<IAgent> Random();
  static std::unique_ptr<IAgent> MCTS(size_t num_simulations = 10000,
                                      size_t depth = 20,
                                      float exploration_constant = 5.0);
};

}  // namespace xq

#endif  // XIANGQI_GAME_ENGINE_INCLUDE_XIANGQI_AGENT_H_
