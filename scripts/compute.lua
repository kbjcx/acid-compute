package.cpath = "/home/llz/CPP/acid-compute/lib/?.so;/home/llz/CPP/acid-compute/modules/?.so"
local libcompute = require("libcompute")

local result = libcompute.compute(5, 4)
print("result: " .. result)
