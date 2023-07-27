package.cpath = "../lib/?.so;./?.so"
local libcompute = require("libcompute")

local result = libcompute.compute(5, 4)
print("result: " .. result)
