#include "metadata_command.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "CLI11.hpp"
#include "json.hpp"
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include "cereal_optional.hpp"
#include "doctest.hpp"
#include "test_helpers.h"

CLI::App *MetadataCommand::CreateCommandCLI(CLI::App &app) {
    auto metadata = app.add_subcommand(
        "metadata",
        "Export or import the cross-format metadata of the audio file(s) as JSON. Use this to copy "
        "metadata between files, or to edit metadata with external tools like jq.");
    metadata->require_subcommand();

    auto exporter = metadata->add_subcommand(
        "export",
        "Write the metadata of each input file to stdout as newline-delimited JSON (NDJSON). Each line is "
        "an object of the form {\"path\":\"...\",\"metadata\":{...}}. The 'metadata' field is null if the "
        "file has no metadata that Signet understands. The output is suitable for piping into jq.");
    exporter->final_callback([this]() { m_mode = Mode::Export; });

    auto importer = metadata->add_subcommand(
        "import",
        "Read NDJSON from stdin (or a file) and replace the metadata of each input file. The JSON must be "
        "in the same shape as 'metadata export' produces: one object per line with 'path' and 'metadata' "
        "fields. The metadata of a file is fully replaced - fields absent from the JSON are cleared. If "
        "the JSON contains a single entry, it is applied to every input file (so copying metadata from "
        "one file to another just works). If it contains multiple entries, input files are matched to "
        "entries by the 'path' field. Note: format-specific metadata not represented in the cross-format "
        "model (e.g. WAV INFO chunks) is left untouched.");
    importer
        ->add_option(
            "json-source", m_import_source,
            "The JSON file to read from. Use '-' or omit to read from stdin.")
        ->default_val("-");
    importer->final_callback([this]() { m_mode = Mode::Import; });

    return metadata;
}

static nlohmann::json MetadataToJson(const Metadata &metadata) {
    std::stringstream ss;
    {
        cereal::JSONOutputArchive archive(ss);
        archive(cereal::make_nvp("Metadata", metadata));
    }
    auto parsed = nlohmann::json::parse(ss.str());
    if (parsed.contains("Metadata")) return parsed["Metadata"];
    return nullptr;
}

static bool JsonToMetadata(const nlohmann::json &j, Metadata &out, std::string &error) {
    out = Metadata {};
    if (j.is_null()) return true;
    if (!j.is_object()) {
        error = "metadata must be a JSON object or null";
        return false;
    }
    nlohmann::json wrapped;
    wrapped["Metadata"] = j;
    std::stringstream ss(wrapped.dump());
    try {
        cereal::JSONInputArchive archive(ss);
        archive(cereal::make_nvp("Metadata", out));
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
    return true;
}

static void ExportMetadata(AudioFiles &files, const std::string &command_name) {
    for (auto &f : files) {
        nlohmann::json line;
        line["path"] = f.OriginalPath().u8string();
        if (f.GetAudio().metadata.IsEmpty()) {
            line["metadata"] = nullptr;
        } else {
            try {
                line["metadata"] = MetadataToJson(f.GetAudio().metadata);
            } catch (const std::exception &e) {
                ErrorWithNewLine(command_name, f, "Failed to serialise metadata: {}", e.what());
                continue;
            }
        }
        fmt::print(stdout, "{}\n", line.dump());
    }
}

static std::string ReadAllFromStream(std::istream &in) {
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static void ImportMetadata(AudioFiles &files,
                           const std::string &source,
                           const std::string &command_name) {
    std::string content;
    if (source.empty() || source == "-") {
        content = ReadAllFromStream(std::cin);
    } else {
        std::ifstream stream(source);
        if (!stream) {
            ErrorWithNewLine(command_name, {}, "Could not open JSON source: {}", source);
            return;
        }
        content = ReadAllFromStream(stream);
    }

    // Parse NDJSON, but also accept a single JSON array or single object as a convenience for hand-written
    // input.
    std::vector<nlohmann::json> entries;
    {
        std::string trimmed = content;
        while (!trimmed.empty() && std::isspace((unsigned char)trimmed.front()))
            trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && std::isspace((unsigned char)trimmed.back()))
            trimmed.pop_back();

        if (!trimmed.empty() && trimmed.front() == '[') {
            try {
                auto arr = nlohmann::json::parse(trimmed);
                for (auto &e : arr) entries.push_back(std::move(e));
            } catch (const std::exception &e) {
                ErrorWithNewLine(command_name, {}, "Failed to parse JSON array: {}", e.what());
                return;
            }
        } else {
            std::istringstream stream(content);
            std::string line;
            size_t line_num = 0;
            while (std::getline(stream, line)) {
                ++line_num;
                std::string l = line;
                while (!l.empty() && std::isspace((unsigned char)l.front())) l.erase(l.begin());
                while (!l.empty() && std::isspace((unsigned char)l.back())) l.pop_back();
                if (l.empty()) continue;
                try {
                    entries.push_back(nlohmann::json::parse(l));
                } catch (const std::exception &e) {
                    ErrorWithNewLine(command_name, {}, "Failed to parse JSON on line {}: {}", line_num,
                                     e.what());
                    return;
                }
            }
        }
    }

    if (entries.empty()) {
        ErrorWithNewLine(command_name, {}, "No JSON entries to import");
        return;
    }

    auto apply_entry = [&](EditTrackedAudioFile &f, const nlohmann::json &entry) {
        if (!entry.is_object() || !entry.contains("metadata")) {
            ErrorWithNewLine(command_name, f, "JSON entry is missing the 'metadata' field");
            return;
        }
        Metadata new_meta;
        std::string err;
        if (!JsonToMetadata(entry["metadata"], new_meta, err)) {
            ErrorWithNewLine(command_name, f, "Failed to parse metadata: {}", err);
            return;
        }
        f.GetWritableAudio().metadata = std::move(new_meta);
        MessageWithNewLine(command_name, f, "Metadata replaced");
    };

    // If there is exactly one entry, broadcast it to every input file regardless of the 'path' field. This
    // makes the simple "copy metadata from one file to another" case work without needing to rewrite the
    // path with jq.
    if (entries.size() == 1) {
        for (auto &f : files) {
            apply_entry(f, entries.front());
        }
        return;
    }

    std::unordered_map<std::string, const nlohmann::json *> by_path;
    for (auto const &e : entries) {
        if (!e.is_object() || !e.contains("path") || !e["path"].is_string()) {
            ErrorWithNewLine(command_name, {},
                             "Each JSON entry must be an object with a string 'path' field");
            return;
        }
        by_path[e["path"].get<std::string>()] = &e;
    }

    for (auto &f : files) {
        auto const path_key = f.OriginalPath().u8string();
        auto it = by_path.find(path_key);
        if (it == by_path.end()) {
            MessageWithNewLine(command_name, f, "No matching entry in JSON; leaving metadata unchanged");
            continue;
        }
        apply_entry(f, *it->second);
    }
}

void MetadataCommand::ProcessFiles(AudioFiles &files) {
    switch (m_mode) {
        case Mode::Export: ExportMetadata(files, GetName()); break;
        case Mode::Import: ImportMetadata(files, m_import_source, GetName()); break;
        case Mode::None: break;
    }
}

TEST_CASE("MetadataCommand") {
    auto test_audio = TestHelpers::CreateSingleOscillationSineWave(2, 44100, 44100);

    SUBCASE("export with empty metadata runs") {
        TestHelpers::ProcessBufferWithCommand<MetadataCommand>("metadata export", test_audio);
    }

    SUBCASE("round-trip empty metadata via JSON") {
        MetadataItems::Loop loop {};
        loop.name = "loop1";
        loop.type = MetadataItems::LoopType::Forward;
        loop.start_frame = 100;
        loop.num_frames = 500;
        test_audio.metadata.loops.push_back(loop);

        auto exported_json = MetadataToJson(test_audio.metadata);
        REQUIRE(!exported_json.is_null());

        Metadata restored;
        std::string err;
        REQUIRE(JsonToMetadata(exported_json, restored, err));
        REQUIRE(err.empty());
        REQUIRE(restored.loops.size() == 1);
        REQUIRE(restored.loops[0].name == "loop1");
        REQUIRE(restored.loops[0].start_frame == 100);
        REQUIRE(restored.loops[0].num_frames == 500);
    }

    SUBCASE("JsonToMetadata accepts null") {
        Metadata m;
        std::string err;
        REQUIRE(JsonToMetadata(nlohmann::json(nullptr), m, err));
        REQUIRE(m.IsEmpty());
    }
}
