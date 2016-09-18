--  Load platform-dependent startup files

function file_exists(name)
  local f = io.open(name,"r")
  if f ~= nil then io.close(f) return true else return false end
end

fname = "../startup_" .. sys.platform .. ".lua"
if file_exists(fname) then
  dofile(fname)
end

--  Config files

sys.config_name = "../luacon_config"

function sys.load_config()
  local fname
  fname = (sys.basedir or ".") .. "/" .. sys.config_name
  if file_exists(fname) then
    dofile(fname)
  else
    sys.config = {}
  end
end

function dump_table(t, indent)
  local str = "{"
  local k, v, n, m
  if indent == nil then
    indent = ""
  end
  local function dump_value(v, indent)
    if type(v) == "table" then return(dump_table(v, indent .. "  ")) end
	if type(v) == "string" then return(string.format("%q", v)) end
	return tostring(v)
  end
  local len = #t
  if len > 0 then
    for k = 1, len do
      str = str .. dump_value(t[k], indent)
	  if k < len then
	    if k % 4 == 0 then
		  str = str .. ",\n" .. indent
		else
		  str = str .. ", "
		end
	  end
	end
  end
  n = 0
  for k, v in pairs(t) do
    if type(k) == "number" then
	  if math.tointeger(k) == k and k >= 1 and k <= len then
	    goto continue
	  end
	  k = "[" .. tostring(k) .. "]"
	elseif type(k) == "string" then
	  if load("local " .. k) == nil then
	    k = string.format("[%q]", k)  -- quoted string with "[]"
	  end
	else
	  error("dump_table: only numbers and strings are allowed as table keys")
	end
	if n + len > 0 then
	  str = str .. ","
	end
	str = str .. string.format("\n%s  %s = %s", indent, k, dump_value(v, indent))
	n = n + 1
    ::continue::
  end
  if n > 0 then
    str = str .. "\n" .. indent .. "}"
  else
    str = str .. "}"
  end
  return str
end

function sys.save_config()
  local fname, fh, msg
  fname = (sys.basedir or ".") .. "/" .. sys.config_name
  fh, msg = io.open(fname, "w")
  if not fh then
    error(msg)
  end
  if not sys.config then
    sys.config = {}
  end
  fh:write("sys.config = " .. dump_table(sys.config, ""))
  fh:close()
end

function sys.set_config(key, value)
  sys.config[key] = value
end

function sys.get_config(key)
  return sys.config[key]
end

sys.load_config()  --  Load configure data

--  Define abbreviation (convenience for beginners)

locate = con.locate
puts = con.puts
cls = con.cls
clearline = con.clearline
color = con.color
inkey = con.inkey
gcolor = con.gcolor
fillcolor = con.fillcolor
pset = con.pset
line = con.line
circle = con.circle
arc = con.arc
fan = con.fan
poly = con.poly
box = con.box
rbox = con.rbox
gcls = con.gcls
gmode = con.gmode
patdef = con.patdef
patundef = con.patundef
patdraw = con.patdraw
paterase = con.paterase
rgb = con.rgb
screensize = con.screensize

wait = sys.wait
write = io.write
exit = os.exit

