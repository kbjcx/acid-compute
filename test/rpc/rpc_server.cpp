
#include "lua.hpp"

#include <iostream>
#include <string>


int main() {
    // 初始化lua解释器
    lua_State* Lua = luaL_newstate();
    luaL_openlibs(Lua);

    lua_pushstring(Lua, "hello world");
    lua_setglobal(Lua, "str");

    lua_pushnumber(Lua, 12);
    lua_setglobal(Lua, "a");
    lua_pushnumber(Lua, 13);
    lua_setglobal(Lua, "b");


    // 执行lua脚本
    if (luaL_dofile(Lua, "/root/acid-compute/scripts/compute.lua") != 0) {
        // 脚本执行失败
        std::string error_message = lua_tostring(Lua, -1);
        std::cout << "Lua error: " << error_message << std::endl;
        lua_pop(Lua, -1);
    }

    // 调用lua函数
    lua_getglobal(Lua, "Main");
    lua_pushstring(Lua, "compute");
    // 无参函数, 返回值为可调用函数列表
    if (lua_pcall(Lua, 1, 1, 0) != 0) {
        // 调用失败
        std::string error_message = lua_tostring(Lua, -1);
        std::cout << "Lua error: " << error_message << std::endl;
        lua_pop(Lua, -1);
    }
    else {
        // 获取返回值
        auto i = lua_gettop(Lua);
        auto count = lua_tonumber(Lua, i);
        std::cout << "Lua result count: " << count << " stack: " << i << std::endl;
        // auto result = lua_tonumber(Lua, -2);
        // std::cout << "Lua result: " << result << std::endl;
    }

    // 关闭lua解释器
    lua_close(Lua);

    return 0;
}