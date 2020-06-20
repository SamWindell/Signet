#pragma once
#include "CLI11.hpp"
#include "rang.hpp"

static constexpr auto fmt_divider = '\033';

static constexpr char fmt_bold[] = {fmt_divider, '\001', '\0'};
static constexpr char fmt_yellow[] = {fmt_divider, '\002', '\0'};
static constexpr char fmt_cyan[] = {fmt_divider, '\003', '\0'};
static constexpr char fmt_green[] = {fmt_divider, '\004', '\0'};

static void PrintSignetHeading() {
    std::cout << rang::style::bold << "Signet\n" << rang::style::reset;
    std::cout << rang::fg::gray
              << "============================================================================\n\n"
              << rang::fg::reset;
}

static void PrintSignetCLI(const std::string &str) {
    auto sections = Split(str, "\033");
    for (auto s : sections) {
        char c = s[0];
        bool keep_char = false;
        switch (c) {
            case fmt_bold[1]: std::cout << rang::style::bold; break;
            case fmt_yellow[1]: std::cout << rang::style::bold << rang::fg::yellow; break;
            case fmt_cyan[1]: std::cout << rang::style::bold << rang::fg::cyan; break;
            case fmt_green[1]: std::cout << rang::style::bold << rang::fg::green; break;
            default: keep_char = true; break;
        }
        if (!keep_char) s.remove_prefix(1);

        std::cout << s;

        switch (c) {
            case fmt_bold[1]: std::cout << rang::style::reset; break;
            case fmt_yellow[1]: std::cout << rang::style::reset << rang::fg::reset; break;
            case fmt_cyan[1]: std::cout << rang::style::reset << rang::fg::reset; break;
            case fmt_green[1]: std::cout << rang::style::reset << rang::fg::reset; break;
            default: break;
        }
    }
}

class WrappedFormatter : public CLI::Formatter {
  public:
    static constexpr int wrap_size = 75;

    inline std::string make_usage(const CLI::App *app, std::string name) const override {
        std::stringstream out;

        out << fmt_bold << get_label("Usage") << ":\n" << fmt_divider << (name.empty() ? "" : "  ") << name;

        std::vector<std::string> groups = app->get_groups();

        // Print an Options badge if any options exist
        std::vector<const CLI::Option *> non_pos_options =
            app->get_options([](const CLI::Option *opt) { return opt->nonpositional(); });
        if (!non_pos_options.empty())
            out << " " << fmt_green << "[" << get_label("OPTIONS") << "]" << fmt_divider;

        // Positionals need to be listed here
        std::vector<const CLI::Option *> positionals =
            app->get_options([](const CLI::Option *opt) { return opt->get_positional(); });

        // Print out positionals if any are left
        if (!positionals.empty()) {
            // Convert to help names
            std::vector<std::string> positional_names(positionals.size());
            std::transform(positionals.begin(), positionals.end(), positional_names.begin(),
                           [this](const CLI::Option *opt) { return make_option_usage(opt); });

            out << fmt_cyan << " " << CLI::detail::join(positional_names, " ") << fmt_divider;
        }

        // Add a marker if subcommands are expected or optional
        if (!app->get_subcommands([](const CLI::App *subc) {
                    return ((!subc->get_disabled()) && (!subc->get_name().empty()));
                })
                 .empty()) {
            out << fmt_yellow;
            out << " " << (app->get_require_subcommand_min() == 0 ? "[" : "")
                << get_label(app->get_require_subcommand_max() < 2 || app->get_require_subcommand_min() > 1
                                 ? "SUBCOMMAND"
                                 : "SUBCOMMANDS")
                << (app->get_require_subcommand_min() == 0 ? "]" : "");
            out << fmt_divider;
        }

        out << std::endl;

        return out.str();
    }

    inline std::string
    make_group(std::string group, bool is_positional, std::vector<const CLI::Option *> opts) const override {
        std::stringstream out;

        out << fmt_bold << "\n" << group << ":\n" << fmt_divider;
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
            out << fmt_bold << "\n" << group << ":\n" << fmt_divider;
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
        out << std::setw(static_cast<int>(2)) << "";
        out << fmt_yellow + sub->get_name() + "\n" + fmt_divider;

        const auto desc = WrapText(sub->get_description(), wrap_size - 4);
        out << std::setw(static_cast<int>(4)) << "";
        for (const char c : desc) {
            out.put(c);
            if (c == '\n') {
                out << std::setw(static_cast<int>(4)) << "";
            }
        }
        out << "\n\n";

        return out.str();
    }

    std::string make_option(const CLI::Option *opt, bool is_positional) const override {
        std::stringstream out;
        out << std::setw(static_cast<int>(2)) << "";
        out << (is_positional ? fmt_cyan : fmt_green);
        out << make_option_name(opt, is_positional) + make_option_opts(opt);
        out << "\033\n";

        const auto desc = WrapText(make_option_desc(opt), wrap_size - 4);
        out << std::setw(static_cast<int>(4)) << "";
        for (const char c : desc) {
            out.put(c);
            if (c == '\n') {
                out << std::setw(static_cast<int>(4)) << "";
            }
        }
        out << "\n\n";
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
        std::string result = fmt_bold + std::string("Description:\n  ") + fmt_divider;
        auto body = (!desc.empty()) ? WrapText(desc, wrap_size, 2) + "\n\n" : std::string {};
        return result + body;
    }

    inline std::string make_expanded(const CLI::App *sub) const override {
        std::stringstream out;
        // out << sub->get_display_name() << "\n";

        out << fmt_yellow;
        out << sub->get_name();
        out << fmt_divider;
        out << "\n";

        out << make_description(sub);
        out << make_positionals(sub);
        out << make_groups(sub, CLI::AppFormatMode::Sub);
        out << make_subcommands(sub, CLI::AppFormatMode::Sub);

        // Drop blank spaces
        std::string tmp = CLI::detail::find_and_replace(out.str(), "\n\n", "\n");
        tmp = tmp.substr(0, tmp.size() - 1); // Remove the final '\n'

        // Indent all but the first line (the name)
        return CLI::detail::find_and_replace(tmp, "\n", "\n  ") + "\n";
    }
};
