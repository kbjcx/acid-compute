#include "acid/acid.h"
#include "lua.h"
#include "lua.hpp"

#include <ostream>
#include <string>


int main() {
    // 初始化lua解释器
    lua_State* Lua = luaL_newstate();
    luaL_openlibs(Lua);

    // 执行lua脚本
    if (luaL_dofile(Lua, "./scripts/register_methods.lua") != 0) {
        // 脚本执行失败
        std::string error_message = lua_tostring(Lua, -1);
        std::cout << "Lua error: " << error_message << std::endl;
        lua_pop(Lua, -1);
    }

    // 调用lua函数
    lua_getglobal(Lua, "RegisterMethods");
    // 无参函数, 返回值为可调用函数列表
    if (lua_pcall(Lua, 0, 1, 0) != 0) {
        // 调用失败
        std::string error_message = lua_tostring(Lua, -1);
        std::cout << "Lua error: " << error_message << std::endl;
        lua_pop(Lua, -1);
    }
    else {
        // 获取返回值
        auto result = lua_touserdata(Lua, -1);
        std::cout << "Lua result: " << result << std::endl;
    }

    // 关闭lua解释器
    lua_close(Lua);

    return 0;
}