#include "RenderGraph.h"

#include <utility>

void RenderGraph::clear() {
    passes.clear();
    executedPassNames.clear();
    stats = {};
}

void RenderGraph::addPass(std::string name, PassFunction function) {
    if (!function) {
        return;
    }

    passes.push_back({std::move(name), std::move(function)});
    stats.passCount = passes.size();
}

void RenderGraph::execute() {
    executedPassNames.clear();
    for (RenderPass& pass : passes) {
        pass.function();
        executedPassNames.push_back(pass.name);
    }

    stats.executedPasses = executedPassNames.size();
}

const RenderGraphStats& RenderGraph::getStats() const {
    return stats;
}

const std::vector<std::string>& RenderGraph::getExecutedPassNames() const {
    return executedPassNames;
}
