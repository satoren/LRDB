local value = 1
local function testfn()
local local_value1 = 1
local local_value2 = "abc"
local local_value3 = value
local local_value4 ={4234.3}
return local_value1,local_value2,local_value3,local_value4
end

testfn()
