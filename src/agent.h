#ifndef SPM_AGENT_H
#define SPM_AGENT_H

#include <stdbool.h>

typedef struct {
    const char *name;        // Short name: "claude"
    const char *display;     // Display name: "Claude Code"
    const char *config_dir;  // Config directory: ".claude"
    const char *skills_dir;  // Skills subdirectory: "skills"
    const char *binary;      // Binary name for which check (optional)
} AgentDef;

typedef struct {
    const AgentDef *def;
    char *install_path;      // Full path: /home/user/.claude/skills
    bool detected;
} Agent;

typedef struct {
    Agent *agents;
    int count;
    int capacity;
} AgentList;

// Get the built-in agent definitions
const AgentDef *get_agent_definitions(void);
int get_agent_count(void);

// Find agent definition by name
const AgentDef *find_agent_def(const char *name);

// Detect installed agents (global = install to home dir, local = install to cwd)
AgentList *detect_agents(bool global);

// Create agent list for specific agent names
AgentList *agents_from_names(const char **names, int count, bool global);

// Get install path for an agent (global or local)
char *get_agent_install_path(const AgentDef *def, bool global);

// Free agent list
void agent_list_free(AgentList *list);

#endif // SPM_AGENT_H
