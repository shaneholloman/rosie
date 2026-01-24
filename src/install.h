#ifndef SPM_INSTALL_H
#define SPM_INSTALL_H

#include <stdbool.h>
#include "agent.h"
#include "skill.h"

typedef struct {
    const char *spec;           // "owner/repo" or URL
    const char *skill_name;     // Specific skill to install, or NULL for all
    const char **agent_names;   // Specific agents or NULL for auto-detect
    int agent_count;
    bool global;                // Install globally (default) or locally
    bool yes;                   // Skip confirmation prompts
    bool list_only;             // Just list skills, don't install
} InstallOptions;

typedef struct {
    const char *skill_name;     // Name of skill to remove
    const char **agent_names;   // Specific agents or NULL for auto-detect
    int agent_count;
    bool global;                // Remove from global (default) or local
    bool yes;                   // Skip confirmation prompts
} RemoveOptions;

// Main install function
int install_package(const InstallOptions *opts);

// Install a single skill to a single agent
int install_skill_to_agent(const Skill *skill, const Agent *agent);

// Remove a skill
int remove_skill(const RemoveOptions *opts);

#endif // SPM_INSTALL_H
