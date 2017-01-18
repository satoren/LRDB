local value = 1
function testfn(arg)
local local_value1 = 1
local local_value2 = "abc"
local local_value3 = 4234.33
local local_value4 ={4234.3}
end

local co = coroutine.create(testfn())
local stat, ret = coroutine.resume( co, 5 )