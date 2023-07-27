# 定义编译器和编译选项
CXX := g++-11
CXXFLAGS := -std=c++20 -Wall

TARGET := libcompute.so
# 定义目标文件夹和输出文件夹
OBJ_DIR := obj
BIN_DIR := .
# 列出所有的CPP文件
CPP_FILES := ${wildcard *.cpp}  # 在此处添加所有的CPP文件
INCLUDE := ../third_party/lua/include
LIBDIR := ../lib

# 生成目标文件名列表，例如file1.cpp -> file1.o
OBJ_FILES := $(addprefix $(OBJ_DIR)/,$(notdir $(CPP_FILES:.cpp=.o)))

# 静态库名
LIB_NAME := lua

# 目标：all (默认目标，直接执行make时会被调用)
all: $(OBJ_DIR) dynamic

dynamic: ${OBJ_FILES}
	$(CXX) $(CXXFLAGS) -shared -o ${TARGET} $^ -L${LIBDIR} -l${LIB_NAME} -Wl,rpath="$&ORIGIN/../lib"

# 生成目标文件
$(OBJ_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -fPIC -I${INCLUDE} -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

# 清理生成的文件
clean:
	rm -rf $(OBJ_DIR)
