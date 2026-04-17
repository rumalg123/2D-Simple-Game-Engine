#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

struct RenderGraphStats {
    std::size_t passCount = 0;
    std::size_t executedPasses = 0;
};

class RenderGraph {
public:
    using PassFunction = std::function<void()>;

    void clear();
    void addPass(std::string name, PassFunction function);
    void execute();

    const RenderGraphStats& getStats() const;
    const std::vector<std::string>& getExecutedPassNames() const;

private:
    struct RenderPass {
        std::string name;
        PassFunction function;
    };

    std::vector<RenderPass> passes;
    std::vector<std::string> executedPassNames;
    RenderGraphStats stats;
};
