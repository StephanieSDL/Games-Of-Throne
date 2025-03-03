#pragma once

#include <vector>

#include "tiny_ecs_registry.hpp"
#include "common.hpp"
#include "render_system.hpp"

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_mixer.h>
#include <functional>

class DecisionNode
{
public:
    std::function<void(float)> action;
    std::function<bool(float)> condition;
    DecisionNode *trueBranch;
    DecisionNode *falseBranch;
    DecisionNode(std::function<void(float)> act, std::function<bool(float)> cond) : action(act), condition(cond), trueBranch(nullptr), falseBranch(nullptr) {}
    ~DecisionNode()
    {
        delete trueBranch;
        delete falseBranch;
    }

    void execute(float elapsed_ms)
    {
        if (action)
        {
            action(elapsed_ms);
        }

        if (condition)
        {
            if (condition(elapsed_ms))
            {
                if (trueBranch)
                {
                    trueBranch->execute(elapsed_ms);
                }
            }
            else
            {
                if (falseBranch)
                {
                    falseBranch->execute(elapsed_ms);
                }
            }
        }
    }
};

class AISystem
{
public:
    AISystem();
    ~AISystem();
    void init(RenderSystem *renderer);
    void step(float elapsed_ms, std::vector<std::vector<int>> &levelMap);
    void boss_attack(Entity entity, int attack_id, float elapsed_ms);
    // struct Node
    // {
    // 	int x, y;
    // 	float gCost, hCost, fCost;
    // 	Node *parent; // to trace the path back once we find the target

    // 	// Comparator for priority queue
    // 	bool operator>(const Node &other) const
    // 	{
    // 		return fCost > other.fCost;
    // 	}
    // };

    std::vector<vec2> findPathAStar(vec2 start, vec2 goal);

private:
    RenderSystem *renderer;

    DecisionNode *chef_decision_tree;
    DecisionNode *knight_decision_tree;
    DecisionNode *prince_decision_tree;
    DecisionNode *king_decision_tree;
    DecisionNode *create_chef_decision_tree();
    DecisionNode *create_knight_decision_tree();
    DecisionNode *create_prince_decision_tree();
    DecisionNode *create_king_decision_tree();
    void perform_chef_attack(ChefAttack attack);
    void perform_knight_attack(KnightAttack attack);
    void perform_prince_attack(PrinceAttack attack);
    void process_prince_attack(float elapsed_ms);
    void perform_king_attack(KingAttack attack);
    void process_king_attack(float elapsed_ms);
    void play_knight_animation(std::vector<BoneKeyframe> &keyframes);
    void play_prince_animation(std::vector<BoneKeyframe> &keyframes);
    void play_king_animation(std::vector<BoneKeyframe> &keyframes);

    // bool isWalkable(int x, int y, const std::vector<std::vector<int>>& grid);
    // std::vector<Node> findPathBFS(int startX, int startY, int targetX, int targetY, const std::vector<std::vector<int>>& grid);
};

struct AStarNode
{
    int x, y;
    float gCost, hCost, fCost;
    AStarNode *parent;

    bool operator>(const AStarNode &other) const
    {
        return fCost > other.fCost;
    }
};