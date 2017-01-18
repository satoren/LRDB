local value = 1
function testfn(arg)
local local_value=value*2
local local_value2=3
end
local value3 = 1
function testfn2(arg)
local local_value3=4
testfn(4)
end
testfn2(2)
