local _ENV={envvar=5456}
local value = 1
local function testfn()
local local_value1 = 1
local local_value2 = "abc"
local local_value3 = 4234.33
local local_value4 ={envvar}
end

testfn()
