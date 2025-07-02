--creates eventdump.txt in the main game directory
local f = io.open("eventdump.txt", "w")
local i=0
local function writeParam(name, data, nullable)
	f:write(data.__type or data.__ctype)
	if data.__metadata.Tool_ClumpType ~= nil then f:write(("<%s>"):format(data.__metadata.Tool_ClumpType)) end
	f:write(" " .. name)
	local default = data.__metadata.Tool_DefaultValue
	if default=="null" or default=="NULL" then default=false end
	if nullable and not default then f:write(" (optional)") end
	if default and not nullable then f:write((" (%s)"):format(default)) end
	if default and nullable then f:write((" (%s, optional)"):format(default)) end
	if not default and not nullable then f:write(" (no default)") end
	if data.__metadata.Tool_Description ~= nil then f:write(" //" .. data.__metadata.Tool_Description:sub(2, -2)) end
	f:write("\n")
end
for k, v in pairs(_ENV) do
	if type(v) == "table" and v.Start ~= nil and v. SetEnabled ~= nil then
		i=i+1
		f:write("+++ " .. k .. " +++\n")
		local required, nullable = {}, {}
		for kk, vv in pairs(v.__vars) do
			if vv.__metadata.Tool_Nullable and tostring(vv.__metadata.Tool_Nullable):lower() ~= "false" then
				nullable[kk] = vv
			else
				required[kk] = vv
			end
		end
		for kk, vv in pairs(required) do
			writeParam(kk, vv, false)
		end
		for kk, vv in pairs(nullable) do
			writeParam(kk, vv, true)
		end
		f:write("\n")
		f:close()
		f = io.open("eventdump.txt", "a") --if I don't do this it won't write every line to the file for some reason, yes I tried calling :flush()
	end
end
f:write("Total: " .. tostring(i) .. " events\n")
f:flush()
f:close()
