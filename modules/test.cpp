#include <cstddef>
#include <iostream>

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

// 返回result的个数
static int func_compute(lua_State* L) {
    int a = lua_tonumber(L, 1);
    int b = lua_tonumber(L, 2);
    lua_pop(L, 2);
    lua_pushnumber(L, a + b);
    return 1;
}

static const luaL_Reg my_funcs[] = {{"compute", func_compute}, {NULL, NULL}};

extern "C" {
int luaopen_libcompute(lua_State* L) {
    luaL_checkversion(L);
    luaL_newlib(L, my_funcs);
    return 1;
}
}


// int main() {
//     lua_State* Lua = luaL_newstate();
//     luaL_openlibs(Lua);

//     lua_pushcfunction(Lua, func_compute);
//     lua_setglobal(Lua, "compute");
//     // lua_pushnumber(Lua, 1);
//     // lua_pushnumber(Lua, 2);

//     // 执行lua脚本
//     if (luaL_dofile(Lua, "../scripts/compute.lua") != 0) {
//         // 执行失败
//         std::string error_message = lua_tostring(Lua, -1);
//         std::cout << "Lua error: " << error_message << std::endl;
//         lua_pop(Lua, -1);
//     }

//     // 关闭lua解释器
//     lua_close(Lua);

//     return 0;
// }