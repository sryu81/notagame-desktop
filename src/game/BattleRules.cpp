#include "BattleData.h"
#include "Rules.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "Path.h"
#include "Unit.h"

namespace battle {

namespace {

struct SavedStatus { int32_t type; int32_t duration; int32_t magnitude; };
struct SavedUnit {
    int32_t faction;
    int32_t hp;
    float   cellX, cellY;
    int32_t statusCount;
    SavedStatus statuses[4];
};
struct SaveData {
    uint32_t magic;
    uint32_t version;
    uint32_t unitCount;
    int32_t  turnIndex;
    float    turnElapsed;
    SavedUnit units[16];
};
constexpr uint32_t kSaveMagic     = 0x4E41474D;
constexpr uint32_t kSaveVersion   = 2;
constexpr int      kMaxSavedUnits = 16;

} // namespace

void setPathOverlay(const std::vector<math::Vec2>& path) {
    if (!d().pathOverlay) return;
    d().pathOverlay->clearChildren();
    if (path.empty() || !d().grid || !d().assets) return;
    const Mesh*  planeMesh  = d().assets->mesh("plane");
    const Model* planeModel = d().assets->model("plane");
    if (!planeMesh || !planeModel) return;
    for (const auto& cell : path) {
        if (!d().grid->inBounds(static_cast<int>(cell.x), static_cast<int>(cell.y))) continue;
        SceneNode* node = d().pathOverlay->addChild();
        const math::Vec3 wp = d().grid->cellToWorld(static_cast<int>(cell.x),
                                                     static_cast<int>(cell.y));
        node->setLocal(math::Mat4::translation(math::Vec3(wp.x, wp.y + 0.01f, wp.z))
                       * math::Mat4::scale(math::Vec3(d().grid->cellSize() * 0.96f, 1.0f,
                                                      d().grid->cellSize() * 0.96f)));
        node->addDrawable({planeMesh, planeModel, math::Vec3(0.20f, 0.90f, 0.30f), d().unlitMat});
    }
}

void refreshSelectionOverlay() {
    if (!d().selectionIndicator || !d().grid) return;
    game::Unit* sel = nullptr;
    for (size_t i = 0; i < d().registry->size(); ++i) {
        auto* c = d().registry->unitAt(i);
        if (c && c->isSelected() && !c->isDead()) { sel = c; break; }
    }
    if (!sel) {
        d().selectionIndicator->setLocal(math::Mat4::scale(math::Vec3(0.0f, 1.0f, 0.0f)));
        return;
    }
    const math::Vec3 pos = d().grid->cellToWorld(
        static_cast<int>(sel->cell().x), static_cast<int>(sel->cell().y));
    d().selectionIndicator->setLocal(
        math::Mat4::translation(math::Vec3(pos.x, pos.y + 0.003f, pos.z))
        * math::Mat4::scale(math::Vec3(d().grid->cellSize() * 0.93f, 1.0f,
                                       d().grid->cellSize() * 0.93f)));
}

void enqueueMove(game::Unit* u, std::vector<math::Vec2> path) {
    if (!d().actionManager || !u || !d().grid) return;
    d().movingUnit = u;
    d().actionManager->enqueue(std::make_unique<action::MoveAction>(u, d().grid.get(), std::move(path)));
}

void enqueueAttack(game::Unit* attacker, game::Unit* target) {
    if (!d().actionManager || !attacker || !target || !d().grid) return;
    const int damage = rules::damage(attacker->stats(), target->stats(),
                                     rules::contextFor(*d().grid, *attacker, *target));
    auto* node = attacker->node();
    auto* targetNode = target->node();
    bool isSkinned = (node && node->skeleton() != nullptr);
    bool targetSkinned = (targetNode && targetNode->skeleton() != nullptr);
    const anim::Clip* atkClip = isSkinned ? nullptr : &d().attackClip;
    const anim::SkeletalClip* skelAtkClip = isSkinned ? &d().skinnedAttackClip : nullptr;
    const anim::Clip* reactClip = targetSkinned ? nullptr : &d().hitReactionClip;
    const anim::SkeletalClip* skelReact = targetSkinned ? &d().skinnedHitReaction : nullptr;

    math::Vec3 targetPos(0.0f);
    if (d().grid)
        targetPos = d().grid->cellToWorld(static_cast<int>(target->cell().x),
                                          static_cast<int>(target->cell().y));
    auto action = std::make_unique<action::AttackAction>(
        attacker, target, damage, atkClip, reactClip, skelAtkClip, skelReact);
    action->setHitCallback([targetPos]() {
        d().particleEmitter.burst(targetPos, 12, 2.5f,
                                  math::Vec4(1.0f, 0.6f, 0.1f, 1.0f),
                                  0.25f, 0.5f);
    });
    d().actionManager->enqueue(std::move(action));
}

namespace {

void collectRigDrawables(SceneNode* node, std::vector<picking::HitShape>& shapes) {
    if (!node) return;
    for (const auto& d : node->drawables()) {
        if (d.mesh) {
            shapes.push_back(picking::fromMesh(*d.mesh, node->world()));
        }
    }
    for (const auto& c : node->children()) {
        collectRigDrawables(c.get(), shapes);
    }
}

} // namespace

game::Unit* pickUnit(float x, float y, Renderer& r) {
    const picking::Ray ray = r.pickingRay(x, y);
    game::Unit* best = nullptr;
    float bestDist = std::numeric_limits<float>::infinity();
    for (size_t i = 0; i < d().registry->size(); ++i) {
        auto* c = d().registry->unitAt(i);
        if (!c || !c->pickable()) continue;
        std::vector<picking::HitShape> shapes;
        collectRigDrawables(c->node(), shapes);
        if (shapes.empty()) continue;
        const picking::Hit hit = picking::pickRay(ray, shapes);
        if (hit.valid && hit.distance < bestDist) { best = c; bestDist = hit.distance; }
    }
    return best;
}

math::Vec2 cellAtScreen(float x, float y, Renderer& r) {
    if (!d().grid) return math::Vec2(-1, -1);
    const picking::Ray ray = r.pickingRay(x, y);
    if (std::abs(ray.direction.y) < 1e-6f) return math::Vec2(-1, -1);
    const float t = -ray.origin.y / ray.direction.y;
    if (t < 0.0f) return math::Vec2(-1, -1);
    const math::Vec3 hitPos(ray.origin.x + ray.direction.x * t,
                            0.0f,
                            ray.origin.z + ray.direction.z * t);
    return d().grid->worldToCell(hitPos);
}

bool isPlayerTurn() {
    return d().turnManager && d().turnManager->isPlayerTurn();
}

std::vector<game::Unit*> charactersAll() {
    std::vector<game::Unit*> out;
    out.reserve(d().registry->size());
    for (size_t i = 0; i < d().registry->size(); ++i) {
        auto* c = d().registry->unitAt(i);
        if (c) out.push_back(c);
    }
    return out;
}

size_t charactersAliveCount() {
    size_t n = 0;
    for (size_t i = 0; i < d().registry->size(); ++i) {
        auto* c = d().registry->unitAt(i);
        if (c && !c->isDead()) ++n;
    }
    return n;
}

game::Unit* selectedUnit() {
    game::Unit* found = nullptr;
    for (size_t i = 0; i < d().registry->size(); ++i) {
        auto* c = d().registry->unitAt(i);
        if (c && c->isSelected() && !c->isDead()) { found = c; break; }
    }
    return found;
}

namespace {

anim::Clip* clipFor(game::Unit* u) {
    if (u == nullptr || u->node() == nullptr) return nullptr;
    return const_cast<anim::Clip*>(u->node()->clip());
}

} // namespace

void placeUnitAtCell(game::Unit* u) {
    if (u == nullptr || !d().grid || u->node() == nullptr) return;
    const math::Vec2 cell = u->cell();
    const math::Vec3 wp = d().grid->cellToWorld(static_cast<int>(cell.x),
                                                static_cast<int>(cell.y));
    const float unitY = wp.y;
    anim::Clip* clip = clipFor(u);
    if (clip != nullptr && !clip->keyframes.empty()) {
        const float baseY = clip->keyframes.front().translation.y;
        for (auto& kf : clip->keyframes) {
            const float dy = kf.translation.y - baseY;
            kf.translation.x = wp.x;
            kf.translation.y = unitY + dy;
            kf.translation.z = wp.z;
        }
        u->node()->setClip(clip);
    }
    u->node()->setLocal(math::Mat4::translation(math::Vec3(wp.x, unitY, wp.z))
                       * math::Mat4::scale(math::Vec3(u->baseScale())));
}

void saveGame() {
    if (d().savePath.empty() || d().registry->empty() || !d().turnManager) return;
    SaveData data = {};
    data.magic     = kSaveMagic;
    data.version   = kSaveVersion;
    data.turnIndex = d().turnManager->turnIndex();
    data.turnElapsed = d().turnManager->elapsed();
    int idx = 0;
    for (size_t i = 0; i < d().registry->size(); ++i) {
        if (idx >= kMaxSavedUnits) break;
        auto* c = d().registry->unitAt(i);
        if (!c) continue;
        SavedUnit& su = data.units[idx++];
        su.faction = static_cast<int32_t>(c->faction());
        su.hp = c->hp();
        su.cellX = c->cell().x;
        su.cellY = c->cell().y;
        const auto& sv = c->statuses();
        su.statusCount = static_cast<int32_t>(sv.size() < 4 ? sv.size() : 4);
        for (int j = 0; j < su.statusCount; ++j)
            su.statuses[j] = {static_cast<int32_t>(sv[j].type), sv[j].duration, sv[j].magnitude};
    }
    data.unitCount = static_cast<uint32_t>(idx);
    if (save::writeBytes(d().savePath.c_str(), &data, sizeof(data)))
        aout << "[save] saved " << data.unitCount << " units to " << d().savePath << std::endl;
    else
        aout << "[save] FAILED to write " << d().savePath << std::endl;
}

void loadGame() {
    if (d().savePath.empty() || d().registry->empty() || !d().turnManager || !d().grid) return;
    SaveData data = {};
    if (!save::readBytes(d().savePath.c_str(), &data, sizeof(data))) {
        aout << "[save] read failed: " << d().savePath << std::endl;
        return;
    }
    if (data.magic != kSaveMagic || data.version != kSaveVersion) {
        aout << "[save] bad magic/version, ignoring" << std::endl;
        return;
    }
    if (data.unitCount == 0 || data.unitCount > kMaxSavedUnits) {
        aout << "[save] bad unit count " << data.unitCount << std::endl;
        return;
    }
    auto applyChar = [](game::Unit* c, const SavedUnit& su) {
        if (c == nullptr) return;
        int diff = su.hp - c->hp();
        if (diff < 0) c->takeDamage(-diff);
        else if (diff > 0) c->heal(diff);
        c->setCell(math::Vec2(su.cellX, su.cellY));
        placeUnitAtCell(c);
        c->clearStatuses();
        for (int i = 0; i < su.statusCount && i < 4; ++i)
            c->applyStatus({static_cast<unit::StatusType>(su.statuses[i].type),
                            su.statuses[i].duration, su.statuses[i].magnitude});
    };
    int n = static_cast<int>(d().registry->size());
    if (static_cast<uint32_t>(n) > data.unitCount) n = static_cast<int>(data.unitCount);
    for (int i = 0; i < n; ++i)
        applyChar(d().registry->unitAt(i), data.units[i]);
    d().turnManager->restore(data.turnIndex, data.turnElapsed);
    d().lastTurnUnit = d().turnManager->currentUnit();
    setPathOverlay({});
    aout << "[save] loaded " << data.unitCount << " units" << std::endl;
}

void tickTurnStart() {
    game::Unit* cur = d().turnManager ? d().turnManager->currentUnit() : nullptr;
    if (cur == d().lastTurnUnit) return;
    if (cur) {
        int oldHp = cur->hp();
        cur->tickStatuses();
        if (cur->hp() < oldHp && d().grid) {
            math::Vec3 pos = d().grid->cellToWorld(
                static_cast<int>(cur->cell().x),
                static_cast<int>(cur->cell().y));
            d().particleEmitter.burst(pos, 6, 1.5f,
                                      math::Vec4(0.2f, 0.9f, 0.2f, 0.8f),
                                      0.2f, 0.8f);
        }
    }
    d().lastTurnUnit = cur;
}

} // namespace battle
