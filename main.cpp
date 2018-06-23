#include <lua.hpp>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <iostream>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM == 501
#define LUA_OK 0
#endif

void printStackContents(lua_State *state)
{
    const int stack_top = -1 - lua_gettop(state);
    printf("stack contents:\n");
    if(stack_top >= 0) {
        printf("    ERROR: stack is invalid, gettop returns %d", lua_gettop(state));
    }
    if(stack_top == -1) {
        printf("    (stack is empty)\n");
        return;
    }
    for(int i = -1; i > stack_top; --i) {
        const int item_type = lua_type(state, i);
        printf("(%d) | ", i);
        switch(item_type) {
            case LUA_TNIL:
                printf("nil (nil)");
                break;
            case LUA_TNUMBER:
                printf("%g (number)", lua_tonumber(state, i));
                break;
            case LUA_TBOOLEAN:
                printf("%d (bool)", lua_toboolean(state, i));
                break;
            case LUA_TSTRING:
                printf("%s (string)", lua_tostring(state, i));
                break;
            case LUA_TTABLE:
                printf("%p (table)", lua_topointer(state, i));
                break;
            case LUA_TFUNCTION:
                printf("%p (function)", (void*)lua_tocfunction(state, i));
                break;
            case LUA_TTHREAD:
                printf("%p (thread)", (void*)lua_tothread(state, i));
                break;
            case LUA_TUSERDATA:
                printf("%p (userdata)", lua_touserdata(state, i));
                break;
            case LUA_TLIGHTUSERDATA:
                printf("%p (light userdata)", lua_touserdata(state, i));
                break;
            default:
                printf("%s (unknown type)", lua_typename(state, item_type));
                break;
        }
        printf("\n");
    }
}

bool luaHandleError(lua_State *state, int error_code)
{
    switch(error_code)
    {
        case LUA_ERRFILE:
        case LUA_ERRSYNTAX:
        case LUA_ERRMEM:
            std::cout << lua_tostring(state, -1) << std::endl;
            return false;
            break;
        case LUA_OK:
            return true;
            break;
    }
    return true;
}

bool loadLua(lua_State *state, const char *filename)
{
    std::cout << "Loading file: " << filename << std::endl;
    auto fail = luaL_loadfile(state, filename);
    if(!luaHandleError(state, fail))
    {
        return false;
    }
    fail = lua_pcall(state, 0, 0, 0);
    if(!luaHandleError(state, fail))
    {
        return false;
    }
    return true;
}


using dep_map_t = std::unordered_map<std::string, std::unordered_set<std::string>>;

//! Parse dependencies from a global table.
dep_map_t parseDependencies(lua_State *state, const char *table_name)
{
    dep_map_t dep_map;
    lua_getglobal(state, table_name);
    lua_pushnil(state);
    while(lua_next(state, -2) != 0)
    {
        const std::string &key = lua_tostring(state, -2);
        const auto iter = dep_map.emplace(key, std::unordered_set<std::string>{}).first;

        lua_pushstring(state, key.c_str());
        lua_gettable(state, -2);
        while(lua_next(state, -2) != 0)
        {
            const std::string subkey = lua_tostring(state, -2);
            if(subkey == "inherits")
            {
                lua_pushstring(state, key.c_str());
                lua_gettable(state, -2);
                while(lua_next(state, -2) != 0)
                {
                    iter->second.emplace(lua_tostring(state, -1));
                    lua_pop(state, 1);
                }
            }
            lua_pop(state, 1);
        }
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    return dep_map;
}

//! Describe a dependency error by examining the remaining inhabitants of a
//! dep-map.
void describeDepenedencyErrors(const dep_map_t &dep_map)
{
    std::vector<std::string> unresolved, circular;

    for(const auto &kvp : dep_map)
    {
        std::cout << "Dependency error in " << kvp.first << '\n';
        unresolved.clear();
        circular.clear();
        for(const auto &dep : kvp.second)
        {
            if(dep_map.find(dep) == dep_map.end())
            {
                unresolved.emplace_back(dep);
            }
            else
            {
                circular.emplace_back(dep);
            }
        }
        if(!circular.empty())
        {
            std::cout << "  Found upstream dependency error:\n";
            for(const auto &circ : circular)
            {
                std::cout << "   - " << circ << '\n';
            }
        }
        if(!unresolved.empty())
        {
            std::cout <<   "  Found unresolved dependencies:\n";
            for(const auto &unre : unresolved)
            {
                std::cout << "   - " << unre << '\n';
            }
        }

    }
}

//! Destructively reduce the depmap to a series of mutually-exclusive batches.
//! This is guaranteed not to fail even if the dependency graph is completely
//! invalid.
auto buildDepBatches(dep_map_t &dep_map)
{
    std::vector<std::vector<std::string>> batches;
    while(!dep_map.empty())
    {
        std::vector<std::string> ready;
        for(const auto &kvp : dep_map)
        {
            if(kvp.second.empty())
            {
                ready.emplace_back(kvp.first);
            }
        }
        if(ready.empty())
        {
            
            describeDepenedencyErrors(dep_map);
            break;
        }
        // remove ready deps from the dep graph
        for(const auto &dep : ready)
        {
            dep_map.erase(dep);
        }
        // remove ready deps from the dep map deps
        for(auto &kvp : dep_map)
        {
            for(const auto &dep : ready)
            {
                kvp.second.erase(dep);
            }
        }
        batches.emplace_back(std::move(ready));
    }
    return batches;
}


/*
*
for n1 in g.nodes_iter():
    if g.has_edge(n1, n1):
        g.remove_edge(n1, n1)
    for n2 in g.successors(n1):
        for n3 in g.successors(n2):
            for n4 in nx.dfs_preorder_nodes(g, n3):
                if g.has_edge(n1, n4):
                    g.remove_edge(n1, n4)    
*/

std::vector<std::string> dfs(std::unordered_map<std::string, std::unordered_set<std::string>> &graph, const std::string &node)
{
    std::vector<std::string> ret;
    for(const auto &child : graph[node])
    {
        ret.emplace_back(child);
        for(const auto &desc : dfs(graph, child))
        {
            ret.emplace_back(desc);
        }
    }
    return ret;
}

void buildReverseMap(const dep_map_t &dep_map)
{
    std::unordered_set<std::string> _nodes;
    std::unordered_map<std::string, std::unordered_set<std::string>> graph;
    for(const auto &kvp : dep_map)
    {
        _nodes.emplace(kvp.first);
        graph[kvp.first];
        for(const auto &parent : kvp.second)
        {
            _nodes.emplace(parent);
            graph[parent].emplace(kvp.first);
        }
    }
    std::cout << "---------- 0 ----------" <<  std::endl;
    for(const auto &kvp : graph)
    {
        if(kvp.second.size() == 0)
            continue;
        std::cout << kvp.first << std::endl;
        for(const auto &child : kvp.second)
            std::cout << "    " << child << std::endl;
    }
    std::cout << "=--\n";
    std::vector<std::string> nodes(_nodes.begin(), _nodes.end());
    for(const auto &n0 : nodes)
    {
        // remove self-link?
        for(const auto &n1 : graph[n0])
        {
            for(const auto &n3 : dfs(graph, n1))
            {
                if(graph[n0].find(n3) != graph[n0].end())
                {
                    graph[n0].erase(n3);
                }
            }
        }
    }
    std::cout << "---------- 1 ----------" <<  std::endl;


    for(const auto &kvp : graph)
    {
        if(kvp.second.size() == 0)
            continue;
        std::cout << kvp.first << std::endl;
        for(const auto &child : kvp.second)
            std::cout << "    " << child << std::endl;
    }
    std::cout << "=--\n";
}

void traDuce(const std::vector<std::string> &nodes, const dep_map_t &dep_map)
{
    for(const auto &n0 : nodes)
    {
    }
}

/**
 * Aspects' default constructors provide sensible default values which can be
 * overwritten.
 */


struct Description {
    Description(std::string s, std::string l) :
        short_desc{s},
        long_desc{l}
    {

    }
    std::string short_desc;
    std::string long_desc;
};

std::unordered_map<std::string, Description> descmap;

// std::unordered_map<std::string, std::function<void(lua_State *state)>> funcmap;

void loadObject(lua_State *state, const std::string &table)
{
    lua_getglobal(state, "objects");
    lua_pushstring(state, table.c_str());
    lua_gettable(state, -2);
    lua_pushnil(state);
    while(lua_next(state, -2) != 0)
    {
        const std::string key = lua_tostring(state, -2);
        if(key == "inherits")
        {
            lua_pop(state, 1);
            continue;
        }
        else if(key == "description")
        {
            lua_pushstring(state, key.c_str());
            lua_gettable(state, -2);
            lua_pop(state, 1);

            lua_getfield(state, -1, "short");
            const std::string srt = lua_tostring(state, -1);
            lua_pop(state, 1);

            lua_getfield(state, -1, "long");
            const std::string lng = lua_tostring(state, -1);
            lua_pop(state, 1);

            descmap.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(srt, lng));
        }
        lua_pop(state, 1);
    }
    lua_pop(state, 2);
}

void loadObjects(lua_State *state, std::vector<std::vector<std::string>> &batches)
{
    // this should be parameterized lookup
    for(const auto &batch : batches)
    {
        for(const auto &table : batch)
        {
            loadObject(state, table);
        }
    }
}

void resolveDeps(lua_State *state, const char *global_table)
{
    auto dep_map = parseDependencies(state, global_table);
    dep_map_t depmap_copy = dep_map;
    buildReverseMap(depmap_copy);
    std::vector<std::string> nodes;
    for(const auto &kvp : dep_map)
    {
        nodes.emplace_back(kvp.first);
    }
    traDuce(nodes, depmap_copy);
    auto batches = buildDepBatches(dep_map);
    return;
    int i = 0;
    for(const auto &r : batches)
    {
        std::cout << "Batch " << i++ << std::endl;
        for(const auto &node : r)
        {
            std::cout << "    " << node << std::endl;
        }
    }
    loadObjects(state, batches);
}

int main()
{
    lua_State *state = luaL_newstate();
    loadLua(state, "items.lua");
    resolveDeps(state, "objects");

    lua_close(state);
    return 0;
}
