::redo::
write("Input number (1-6): ")
c = inkey()
write("\n")
if c == 27 or c == 3 then exit() end
n = c - 48
gcls()
width, height = screensize()
for i = 1, 100 do
  w = math.random() * width * 0.5
  h = math.random() * height * 0.5
  x = math.random() * (width - w)
  y = math.random() * (height - h)
  c = {math.random(), math.random(), math.random(), math.random() * 0.5 + 0.5}
  c[math.random(3)] = 1.0
  gcolor(rgb(c[1], c[2], c[3], c[4]))
  c = {math.random(), math.random(), math.random(), math.random() * 0.5 + 0.5}
  c[math.random(3)] = 1.0
  fillcolor(rgb(c[1], c[2], c[3], c[4]))
  if n == 1 then
    line(x, y, x + w, y + h)
  elseif n == 2 then
    box(x, y, w, h)
  elseif n == 3 then
    rbox(x, y, w, h, math.random() * 10 + 5, math.random() * 10 + 5)
  elseif n == 4 then
    circle(x + w / 2, y + h / 2, w / 2, h / 2)
  elseif n == 5 then
    arc(x + w / 2, y + h / 2, math.random() * 360, math.random() * 360, w / 2, h / 2)
  elseif n == 6 then
	fan(x + w / 2, y + h / 2, math.random() * 360, math.random() * 360, w / 2, h / 2)
  end
end
goto redo
