# 定义编译器和编译选项
CXX := g++-11
CXXFLAGS := -std=c++20 -Wall -DLUA_USE_LINUX=1

TARGET := rpc_server
ROOT := /home/llz/CPP/acid-compute

# 定义目标文件夹和输出文件夹
OBJ_DIR := $(ROOT)/obj
BIN_DIR := $(ROOT)/bin
# 列出所有的CPP文件
# CPP_FILES := ${wildcard *.cpp}  # 在此处添加所有的CPP文件
CPP_FILES := rpc_server.cpp
INCLUDE := $(ROOT)/third_party/lua/include
LIBDIR := $(ROOT)/lib

# 生成目标文件名列表，例如file1.cpp -> file1.o
OBJ_FILES := $(addprefix $(OBJ_DIR)/,$(notdir $(CPP_FILES:.cpp=.o)))

# 静态库名
LIB_NAME := lua

# 目标：all (默认目标，直接执行make时会被调用)
all: $(BIN_DIR) $(OBJ_DIR) exe clean

exe: ${OBJ_FILES}
	$(CXX) $(CXXFLAGS) -o ${TARGET} $^ -L${LIBDIR} -ldl -l${LIB_NAME} -Wl,-rpath=${LIBDIR}

# 生成目标文件
$(OBJ_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -I${INCLUDE} -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

# 清理生成的文件
clean:
	rm -rf $(OBJ_DIR)

# 加上-DLUA_USE_LINUX=1之后重新编译lua编译器
