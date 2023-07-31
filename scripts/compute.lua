package.cpath = "/home/llz/CPP/acid-compute/lib/?.so;/home/llz/CPP/acid-compute/modules/?.so"
local libcompute = require("libcompute")

local methods = {}

methods["compute"] = libcompute

function Main(name)
    return methods[name].compute(a, b)
end
