#include "agent.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Built-in agent definitions
static const AgentDef AGENT_DEFS[] = {
    {"claude",    "Claude Code",  ".claude",    "skills", "claude"},
    {"cursor",    "Cursor",       ".cursor",    "skills", "cursor"},
    {"opencode",  "OpenCode",     ".opencode",  "skills", "opencode"},
    {"cline",     "Cline",        ".cline",     "skills", NULL},
    {"codex",     "Codex",        ".codex",     "skills", "codex"},
    {"windsurf",  "Windsurf",     ".windsurf",  "skills", NULL},
    {"continue",  "Continue",     ".continue",  "skills", NULL},
    {"copilot",   "GitHub Copilot", ".github",  "skills", NULL},
    {"aider",     "Aider",        ".aider",     "skills", "aider"},
    {"roo",       "Roo",          ".roo",       "skills", NULL},
    {"amplify",   "Amplify",      ".amplify",   "skills", NULL},
    {"zed",       "Zed",          ".zed",       "skills", "zed"},
    {NULL, NULL, NULL, NULL, NULL}  // Sentinel
};

const AgentDef *get_agent_definitions(void) {
    return AGENT_DEFS;
}

int get_agent_count(void) {
    int count = 0;
    while (AGENT_DEFS[count].name != NULL) {
        count++;
    }
    return count;
}

const AgentDef *find_agent_def(const char *name) {
    if (!name) return NULL;

    for (int i = 0; AGENT_DEFS[i].name != NULL; i++) {
        if (strcmp(AGENT_DEFS[i].name, name) == 0) {
            return &AGENT_DEFS[i];
        }
    }
    return NULL;
}

char *get_agent_install_path(const AgentDef *def, bool global) {
    if (!def) return NULL;

    char *base;
    if (global) {
        base = get_home_dir();
        if (!base) return NULL;
    } else {
        base = str_dup(".");
    }

    char *config_path = path_join(base, def->config_dir);
    spm_free(base);

    char *skills_path = path_join(config_path, def->skills_dir);
    spm_free(config_path);

    return skills_path;
}

static Agent *agent_create(const AgentDef *def, bool global) {
    Agent *agent = spm_malloc(sizeof(Agent));
    agent->def = def;
    agent->install_path = get_agent_install_path(def, global);
    agent->detected = false;
    return agent;
}

static void agent_list_add(AgentList *list, Agent *agent) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->agents = spm_realloc(list->agents, list->capacity * sizeof(Agent));
    }
    list->agents[list->count++] = *agent;
    spm_free(agent);
}

AgentList *detect_agents(bool global) {
    AgentList *list = spm_malloc(sizeof(AgentList));
    list->agents = NULL;
    list->count = 0;
    list->capacity = 0;

    char *home = get_home_dir();
    if (!home) {
        log_error("Cannot determine home directory");
        return list;
    }

    // Always detect based on global config dirs (that's where agents live)
    // But set install path based on global flag
    for (int i = 0; AGENT_DEFS[i].name != NULL; i++) {
        char *config_path = path_join(home, AGENT_DEFS[i].config_dir);

        if (dir_exists(config_path)) {
            Agent *agent = agent_create(&AGENT_DEFS[i], global);
            agent->detected = true;
            agent_list_add(list, agent);
            log_debug("Detected agent: %s (%s)", AGENT_DEFS[i].display, config_path);
        }

        spm_free(config_path);
    }

    spm_free(home);
    return list;
}

AgentList *agents_from_names(const char **names, int count, bool global) {
    AgentList *list = spm_malloc(sizeof(AgentList));
    list->agents = NULL;
    list->count = 0;
    list->capacity = 0;

    for (int i = 0; i < count; i++) {
        const AgentDef *def = find_agent_def(names[i]);
        if (!def) {
            log_error("Unknown agent: %s", names[i]);
            continue;
        }

        Agent *agent = agent_create(def, global);
        agent->detected = true;  // User explicitly specified it
        agent_list_add(list, agent);
    }

    return list;
}

void agent_list_free(AgentList *list) {
    if (!list) return;

    for (int i = 0; i < list->count; i++) {
        spm_free(list->agents[i].install_path);
    }
    spm_free(list->agents);
    spm_free(list);
}
