#include "Scene.h"
#include "SceneEditSession.h"
#include "SceneSerializer.h"
#include "ResourceManager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool value, const char* reason) {
    if (!value) throw std::runtime_error(reason);
}
}

int main() {
    try {
        Scene scene;
        const Entity a = scene.createEntity("Player", {1.0f, 2.0f});
        const Entity b = scene.createEntity("Enemy", {3.0f, 4.0f});
        const std::string guidA = scene.guid(a);
        const std::string guidB = scene.guid(b);
        check(guidA.size() == 36 && guidB.size() == 36 && guidA != guidB,
              "Every entity must have a unique 36-character GUID");
        check(scene.findEntityByGuid(guidA) == a, "GUID lookup failed");
        check(!scene.setEntityGuid(b, guidA), "Duplicate GUID accepted");
        check(!scene.setEntityGuid(b, "not-a-uuid"), "Malformed GUID accepted");
        check(scene.guid(a) == guidA && scene.guid(b) == guidB,
              "Rejected GUID assignment modified entities");

        Scene copied = scene;
        check(copied.findEntityByGuid(guidA) == a, "Copy lost GUID mapping");
        SceneEditSession session;
        check(session.begin(scene), "Play session did not start");
        check(!session.begin(scene), "Nested Play Mode accepted");
        scene.getTransform(a)->x = 90.0f;
        scene.destroyEntity(b);
        const Entity spawned = scene.createEntity("RuntimeOnly");
        check(scene.isValidEntity(spawned), "Runtime spawn failed");
        check(scene.guid(a) == guidA, "Runtime update replaced identity");
        check(session.stop(scene), "Stop did not restore authored scene");
        check(scene.getTransform(a)->x == 1.0f &&
              scene.isValidEntity(b) && !scene.isValidEntity(spawned),
              "Stop leaked runtime transforms, deletion or spawned entity");
        check(scene.guid(a) == guidA && scene.guid(b) == guidB,
              "Stop did not restore authored GUIDs");
        check(!session.stop(scene), "Stop succeeded without Play Mode");

        SceneHistory history(2);
        history.record(scene);
        scene.setTransform(a, {7.0f, 8.0f});
        history.record(scene);
        scene.setTransform(a, {9.0f, 10.0f});
        check(history.undo(scene) && scene.getTransform(a)->x == 7.0f,
              "First undo failed");
        check(history.undo(scene) && scene.getTransform(a)->x == 1.0f,
              "Second undo failed");
        check(!history.undo(scene), "Undo history exceeded bounded capacity");
        check(history.redo(scene) && scene.getTransform(a)->x == 7.0f,
              "Redo failed");
        history.record(scene);
        check(!history.canRedo(), "New edit did not invalidate redo history");
        history.clear();
        check(!history.canUndo(), "Clear did not empty history");

        const auto folder = std::filesystem::temp_directory_path() /
            ("simpleengine_guid_test_" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(folder);
        struct Cleanup {
            std::filesystem::path path;
            ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); }
        } cleanup{folder};

        ResourceManager resources;
        std::string error;
        const auto file = (folder / "scene.json").string();
        check(saveSceneToJson(scene, resources, file, error), "Scene GUID save failed");
        Scene loaded;
        check(loadSceneFromJson(loaded, resources, file, error), "Scene GUID reload failed");
        check(loaded.findEntityByGuid(guidA) != InvalidEntity &&
              loaded.findEntityByGuid(guidB) != InvalidEntity,
              "Serialized GUIDs did not survive reload");
        const std::string serialized = [&] {
            std::ifstream f(file);
            return std::string(std::istreambuf_iterator<char>(f), {});
        }();
        check(serialized.find("\"guid\"") != std::string::npos,
              "Scene file is missing GUID fields");

        // Legacy scenes without GUIDs must remain readable and get generated IDs.
        const auto legacy = (folder / "legacy.json").string();
        {
            std::ofstream out(legacy);
            out << R"({"entities":[{"id":5,"name":{"name":"Legacy"}}]})";
        }
        Scene older;
        check(loadSceneFromJson(older, resources, legacy, error), "Legacy scene cannot load");
        const Entity old = older.findEntityByName("Legacy");
        check(old != InvalidEntity && older.guid(old).size() == 36,
              "Legacy scene did not generate an identity");

        // Duplicate GUIDs must fail without destroying the current open scene.
        const auto malformed = (folder / "duplicate.json").string();
        {
            std::ofstream out(malformed);
            out << "{\"entities\":[{\"id\":0,\"guid\":\"" << guidA
                << "\"},{\"id\":1,\"guid\":\"" << guidA << "\"}]}";
        }
        const std::string preservedId = older.guid(old);
        check(!loadSceneFromJson(older, resources, malformed, error),
              "Duplicate GUID scene was loaded");
        check(older.findEntityByGuid(preservedId) == old,
              "Failed import destroyed the open scene");

        scene.destroyEntity(a);
        check(scene.findEntityByGuid(guidA) == InvalidEntity,
              "Deletion leaked stale GUID mapping");
        std::cout << "Scene GUID, Play snapshot, undo/redo and serializer tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Scene authoring test failed: " << e.what() << '\n';
        return 1;
    }
}
