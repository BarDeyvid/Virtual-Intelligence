// ActionExecutor.hpp
#pragma once

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include "minecraft/MinecraftBridge.hpp"
#include "EndocrineSystem.hpp"

namespace alyssa_minecraft {

/// Maps a short label (e.g. "B1", "E2") assigned in the prompt to the real
/// integer world coordinates it refers to. Built by
/// MinecraftSession::build_prompt() and threaded through to execute() so a
/// small model only ever has to copy a label token instead of generating
/// coordinates by arithmetic — see gameplay_action.gbnf for why.
using LabelMap = std::unordered_map<std::string, std::array<int, 3>>;

/// Maps an entity label ("E1"...) to the entity's name ("player", "rabbit",
/// "zombie"...). "atacar" targets by name (actions.js matches bot.entities
/// by e.name), not by coordinate, so it needs this instead of/alongside
/// LabelMap — a resolved coordinate wouldn't mean anything to bot.attack().
using EntityLabelMap = std::unordered_map<std::string, std::string>;

/**
 * @class ActionExecutor
 * @brief Turns a gameplayModel signal into a real Minecraft action and feeds
 *        the outcome back into the EndocrineSystem.
 *
 * The signal handed to execute() is expected to already match the format the
 * gameplay_action.gbnf grammar guarantees (see AlyssaCore::generate_raw and
 * ExpertBase::parse_expert_signal's "gameplayModel" branch):
 *   [AÇÃO] <verbo> <args...> [CONFIANÇA] <0-1> [CONTEXTO] <texto>
 *
 * Reachability/legality of the action itself is validated authoritatively by
 * the sidecar (minecraft-bridge/actions.js) against the live world state —
 * duplicating that check here would just let the two implementations drift.
 * This class only guards against a signal that fails to parse at all.
 */
class ActionExecutor {
public:
    ActionExecutor(MinecraftBridge& bridge, alyssa_endocrine::EndocrineSystem& endocrine);

    /// Parses, resolves any label ("mover"/"minerar"/"colocar" target) to
    /// real coordinates via `labels` (or, for "atacar", to an entity name via
    /// `entity_names` — falling back to the raw argument if it's not a known
    /// label, so a model that writes the name directly still works),
    /// executes via the bridge, and applies endocrine feedback. Returns the
    /// sidecar's {"ok":bool,"message":string} result (or a synthesized
    /// failure if the signal didn't parse, or referenced an unknown
    /// mover/minerar/colocar label).
    nlohmann::json execute(const std::string& gameplay_signal, const LabelMap& labels,
                            const EntityLabelMap& entity_names);

    /// Drains sidecar events (damage/death/chat) accumulated since the last
    /// call and applies the matching endocrine response to each. Call this
    /// once per game loop tick even on turns with no gameplayModel action.
    /// Returns the drained events so the caller can react further — e.g.
    /// MinecraftSession forwards "chat" events into Alyssa's conversational
    /// brain, which this class deliberately has no access to.
    std::vector<nlohmann::json> process_pending_events();

private:
    MinecraftBridge& bridge;
    alyssa_endocrine::EndocrineSystem& endocrine;
};

} // namespace alyssa_minecraft
