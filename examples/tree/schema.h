#ifndef TREE_SCHEMA_H
#define TREE_SCHEMA_H

#include "../../cli.h"

// Describe the root schema.
#define APP_ARGUMENTS \
    CLI_ARG_FLAG(verbose, .short_name = 'v', .long_name = "verbose", .description = "Print extra diagnostics")

// The root schema has commands. The commands are defined in a child schema named Remote.
#define APP_COMMANDS \
    CLI_COMMAND_REF(remote, "remote", "Manage remotes", Remote)


// Describe the child schema and its leaf commands. Potentially, you could nest the command levels even more.
#define REMOTE_ARGUMENTS \
    CLI_ARG_OPTION_TEXT(config, .long_name = "config", .label = "FILE", .default_value = "default", \
        .description = "Configuration to use")

#define REMOTE_COMMANDS \
    CLI_COMMAND(add, "add", "Display a new remote", ADD_ARGUMENTS) \
    CLI_COMMAND(status, "status", "Display the selected configuration", CLI_NO_ARGUMENTS)

#define ADD_ARGUMENTS \
    CLI_ARG_POSITIONAL_TEXT(name, .required = true, .label = "NAME", .description = "Remote name") \
    CLI_ARG_POSITIONAL_TEXT(url, .required = true, .label = "URL", .description = "Remote address")


// Generate the child first. C needs its complete type so the parent can embed it in its command union.
// This extra generation step is needed for each defined child schema.
#define CLI_COMMAND_REF_NAME Remote
#define CLI_ARGUMENTS REMOTE_ARGUMENTS
#define CLI_COMMANDS REMOTE_COMMANDS
// Make commands required at this level (enabled by default, so it may be omitted).
#define CLI_COMMANDS_REQUIRED true
#include "../../cli.h"


// Generate the root schema.
#define CLI_ARGUMENTS APP_ARGUMENTS
#define CLI_COMMANDS APP_COMMANDS
// Make commands optional at this level.
#define CLI_COMMANDS_REQUIRED false
#include "../../cli.h"

#endif //TREE_SCHEMA_H
