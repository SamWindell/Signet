#pragma once
#include "CLI11.hpp"
#include "fmt/color.h"

// A hack to be able to easily override the const methods of CLI::Formatter
int global_formatter_indent {};

class SignetCLIHelpFormatter : public CLI::Formatter {
  public:
    enum class OutputMode { CommandLine, Markdown };
    static constexpr int wrap_size = 75;

    SignetCLIHelpFormatter(OutputMode mode) : m_mode(mode) {
        labels_["SUBCOMMANDS"] = "COMMANDS";
        labels_["SUBCOMMAND"] = "COMMAND";
        labels_["Subcommand"] = "Command";
        labels_["subcommand"] = "command";
        labels_["Subcommands"] = "Commands";
        labels_["subcommands"] = "commands";
    }

    inline std::string make_usage(const CLI::App *app, std::string name) const override {
        std::stringstream out;

        out << fmt::format("{}:\n", FormatHeading(get_label("Usage"))) << (name.empty() ? "" : "  ")
            << FormatCLIExample(name);

        std::vector<std::string> groups = app->get_groups();

        // Print an Options badge if any options exist
        std::vector<const CLI::Option *> non_pos_options =
            app->get_options([](const CLI::Option *opt) { return opt->nonpositional(); });
        if (!non_pos_options.empty()) out << " " << FormatOption(fmt::format("[{}]", get_label("OPTIONS")));

        // Positionals need to be listed here
        std::vector<const CLI::Option *> positionals =
            app->get_options([](const CLI::Option *opt) { return opt->get_positional(); });

        // Print out positionals if any are left
        if (!positionals.empty()) {
            // Convert to help names
            std::vector<std::string> positional_names(positionals.size());
            std::transform(positionals.begin(), positionals.end(), positional_names.begin(),
                           [this](const CLI::Option *opt) { return make_option_usage(opt); });

            out << " " << FormatPositional(CLI::detail::join(positional_names, " "));
        }

        // Add a marker if subcommands are expected or optional
        if (!app->get_subcommands([](const CLI::App *subc) {
                    return ((!subc->get_disabled()) && (!subc->get_name().empty()));
                })
                 .empty()) {
            out << " "
                << FormatSubcommandCLIExample((app->get_require_subcommand_min() == 0 ? "[" : "") +
                                              get_label(app->get_require_subcommand_max() < 2 ||
                                                                app->get_require_subcommand_min() > 1
                                                            ? "COMMAND"
                                                            : "COMMANDS") +
                                              (app->get_require_subcommand_min() == 0 ? "]" : ""));
        }

        out << std::endl;

        return out.str();
    }

    inline std::string
    make_group(std::string group, bool is_positional, std::vector<const CLI::Option *> opts) const override {
        std::stringstream out;

        out << fmt::format("\n{}:\n", FormatHeading(group));
        for (const auto opt : opts) {
            out << make_option(opt, is_positional);
        }

        return out.str();
    }

    inline std::string make_subcommands(const CLI::App *app, CLI::AppFormatMode mode) const override {
        std::stringstream out;

        auto subcommands = app->get_subcommands({});

        // Make a list in definition order of the groups seen
        std::vector<std::string> subcmd_groups_seen;
        for (const auto com : subcommands) {
            if (com->get_name().empty()) {
                if (!com->get_group().empty()) {
                    out << make_expanded(com);
                }
                continue;
            }
            std::string group_key = com->get_group();
            if (!group_key.empty() && std::find_if(subcmd_groups_seen.begin(), subcmd_groups_seen.end(),
                                                   [&group_key](std::string a) {
                                                       return CLI::detail::to_lower(a) ==
                                                              CLI::detail::to_lower(group_key);
                                                   }) == subcmd_groups_seen.end())
                subcmd_groups_seen.push_back(group_key);
        }

        // For each group, filter out and print subcommands
        for (const std::string &group : subcmd_groups_seen) {
            out << fmt::format("\n{}:\n", FormatHeading(group));
            auto subcommands_group = app->get_subcommands([&group](const CLI::App *sub_app) {
                return CLI::detail::to_lower(sub_app->get_group()) == CLI::detail::to_lower(group);
            });
            for (const auto new_com : subcommands_group) {
                if (new_com->get_name().empty()) continue;
                if (mode != CLI::AppFormatMode::All) {
                    out << make_subcommand(new_com);
                } else {
                    out << new_com->help(new_com->get_name(), CLI::AppFormatMode::Sub);
                    out << "\n";
                }
            }
        }

        return out.str();
    }

    std::string make_subcommand(const CLI::App *sub) const override {
        std::stringstream out;

        global_formatter_indent += 2;
        out << IndentTextIfNeeded(FormatSubcommand(sub->get_name()), global_formatter_indent) << "\n";

        global_formatter_indent += 2;
        auto desc = WrapTextIfNeeded(sub->get_description(), wrap_size - global_formatter_indent);
        desc = IndentTextIfNeeded(desc, global_formatter_indent);
        global_formatter_indent -= 2;

        global_formatter_indent -= 2;
        out << desc << "\n\n";

        return out.str();
    }

    std::string make_option(const CLI::Option *opt, bool is_positional) const override {
        std::stringstream out;

        global_formatter_indent += 2;
        const auto content = make_option_name(opt, is_positional) + make_option_opts(opt);
        out << IndentTextIfNeeded(is_positional ? FormatPositional(content) : FormatOption(content),
                                  global_formatter_indent);
        out << "\n";

        global_formatter_indent += 2;
        auto desc = WrapTextIfNeeded(make_option_desc(opt), wrap_size - global_formatter_indent);
        desc = IndentTextIfNeeded(desc, global_formatter_indent);
        global_formatter_indent -= 2;

        global_formatter_indent -= 2;
        out << desc << "\n\n";
        return out.str();
    }

    std::string make_description(const CLI::App *app) const override {
        std::string desc = app->get_description();
        auto min_options = app->get_require_option_min();
        auto max_options = app->get_require_option_max();
        if (app->get_required()) {
            desc += " REQUIRED ";
        }
        if ((max_options == min_options) && (min_options > 0)) {
            if (min_options == 1) {
                desc += " \n[Exactly 1 of the following options is required]";
            } else {
                desc += " \n[Exactly " + std::to_string(min_options) +
                        "options from the following list are required]";
            }
        } else if (max_options > 0) {
            if (min_options > 0) {
                desc += " \n[Between " + std::to_string(min_options) + " and " + std::to_string(max_options) +
                        " of the follow options are required]";
            } else {
                desc +=
                    " \n[At most " + std::to_string(max_options) + " of the following options are allowed]";
            }
        } else if (min_options > 0) {
            desc += " \n[At least " + std::to_string(min_options) + " of the following options are required]";
        }
        std::string result = FormatHeading("Description:") + std::string("\n");
        global_formatter_indent += 2;
        std::string content = WrapTextIfNeeded(desc, wrap_size - global_formatter_indent);
        content = IndentTextIfNeeded(content, global_formatter_indent);
        global_formatter_indent -= 2;
        auto body = (!desc.empty()) ? content + "\n\n" : std::string {};
        return result + body;
    }

    inline std::string make_expanded(const CLI::App *sub) const override {
        std::stringstream out;

        global_formatter_indent += 2;

        out << IndentTextIfNeeded(FormatSubcommand(sub->get_name()), global_formatter_indent);
        out << "\n";

        global_formatter_indent += 2;

        out << make_description(sub);
        out << make_positionals(sub);
        out << make_groups(sub, CLI::AppFormatMode::Sub);
        out << make_subcommands(sub, CLI::AppFormatMode::Sub);

        global_formatter_indent -= 2;

        // Drop blank spaces
        std::string tmp = CLI::detail::find_and_replace(out.str(), "\n\n", "\n");
        tmp = tmp.substr(0, tmp.size() - 1); // Remove the final '\n'

        global_formatter_indent -= 2;

        // Indent all but the first line (the name)
        return tmp + "\n";
        // return CLI::detail::find_and_replace(tmp, "\n", "\n  ") + "\n";
    }

    inline std::string
    make_help(const CLI::App *app, std::string name, CLI::AppFormatMode mode) const override {

        // This immediately forwards to the make_expanded method. This is done this way so that subcommands
        // can have overridden formatters
        if (mode == CLI::AppFormatMode::Sub) return make_expanded(app);

        std::stringstream out;
        if ((app->get_name().empty()) && (app->get_parent() != nullptr)) {
            if (app->get_group() != "Commands") {
                out << app->get_group() << ':';
            }
        }

        out << make_description(app);
        out << make_usage(app, name);
        out << make_positionals(app);
        out << make_groups(app, mode);
        out << make_subcommands(app, mode);
        out << '\n' << make_footer(app);

        return out.str();
    }

  private:
    std::string IndentTextIfNeeded(const std::string &str, usize indent) const {
        if (m_mode == OutputMode::Markdown) return str;
        return IndentText(str, indent);
    }

    std::string WrapTextIfNeeded(const std::string &str, unsigned size) const {
        if (m_mode == OutputMode::Markdown) return str;
        return WrapText(str, size);
    }

    std::string FormatCLIExample(const std::string &str) const {
        if (m_mode == OutputMode::CommandLine) {
            return str;
        }
        return fmt::format("`{}`", str);
    }

    std::string FormatHeading(const std::string &heading) const {
        switch (m_mode) {
            case OutputMode::CommandLine: {
                return IndentText(fmt::format(fmt::emphasis::bold, "{}", heading), global_formatter_indent);
            }
            case OutputMode::Markdown: {
                if (global_formatter_indent) {
                    // Limit the heading to H6 - this seems to be the max for markdown.
                    return "##" + std::string(std::min(global_formatter_indent, 4), '#') + " " + heading;
                } else {
                    return "## " + heading;
                }
            }
        }
        return "";
    }

    std::string FormatOption(const std::string &option) const {
        switch (m_mode) {
            case OutputMode::CommandLine: {
                return fmt::format(fg(fmt::color::green) | fmt::emphasis::bold, "{}", option);
            }
            case OutputMode::Markdown: {
                return fmt::format("`{}`", option);
            }
        }
        return "";
    }

    std::string FormatPositional(const std::string &pos) const {
        switch (m_mode) {
            case OutputMode::CommandLine: {
                return fmt::format(fg(fmt::color::cyan) | fmt::emphasis::bold, "{}", pos);
            }
            case OutputMode::Markdown: {
                return fmt::format("`{}`", pos);
            }
        }
        return "";
    }

    std::string FormatSubcommandCLIExample(const std::string &sub) const {
        if (m_mode == OutputMode::CommandLine) {
            return FormatSubcommand(sub);
        }
        return FormatCLIExample(sub);
    }

    std::string FormatSubcommand(const std::string &sub) const {
        switch (m_mode) {
            case OutputMode::CommandLine: {
                return fmt::format(fg(fmt::color::yellow) | fmt::emphasis::bold, "{}", sub);
            }
            case OutputMode::Markdown: {
                return FormatHeading(sub);
            }
        }
        return "";
    }

    OutputMode m_mode;
};

static void PrintSignetHeading() {
    fmt::print(fmt::emphasis::bold, "Signet\n");

    std::array<char, SignetCLIHelpFormatter::wrap_size> divider {};
    std::memset(divider.data(), '=', divider.size() - 1);
    fmt::print(fg(fmt::color::gray), "{}\n\n", divider.data());
}

// Subcommands can have different formatters to their parents, to set all of them, we have to go down
// recursively setting the new formatter.
static void SetFormatterRecursively(CLI::App *app, std::shared_ptr<SignetCLIHelpFormatter> formatter) {
    app->formatter(formatter);
    for (auto s : app->get_subcommands({})) {
        SetFormatterRecursively(s, formatter);
    }
}
