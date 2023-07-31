# 定义编译器和编译选项
CXX := gcc
CXXFLAGS := -std=gnu99 -Wall

# 定义目标文件夹和输出文件夹
OBJ_DIR := .
BIN_DIR := .
# 列出所有的CPP文件
CPP_FILES := ${wildcard *.c}  # 在此处添加所有的CPP文件
CORE_O= lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o ltm.o lundump.o lvm.o lzio.o
LIB_O=  lauxlib.o lbaselib.o lcorolib.o ldblib.o liolib.o lmathlib.o loadlib.o loslib.o lstrlib.o ltablib.o lutf8lib.o linit.o
LUA_DIR := ../third_party/lua/src

# 生成目标文件名列表，例如file1.cpp -> file1.o
OBJ_FILES := $(addprefix $(OBJ_DIR)/,$(notdir $(CPP_FILES:.c=.o)))

# 静态库名
LIB_NAME := lua

# 目标：all (默认目标，直接执行make时会被调用)
all: $(OBJ_DIR) dynamic

dynamic: ${CORE_O} ${LIB_O}
	$(CXX) $(CXXFLAGS) -shared -o liblua.so $^

# 生成目标文件
$(OBJ_DIR)/%.o: %.c
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

# 创建输出文件夹
$(BIN_DIR):
	mkdir -p $@

$(OBJ_DIR):
	mkdir -p $@

# 清理生成的文件
clean:
	rm -rf $(BIN_DIR)/executable $(OBJ_DIR)
