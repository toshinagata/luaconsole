a = rgb(0.4, 0.5, 1.0)
b = rgb(1.0, 1.0, 0.6)
c = rgb(0.6, 0.3, 0.5)
gcls()
gmode(1)
width, height = screensize()
patdef(1, 18, 18, {
0, 0, 0, 0, 0, 0, a, a, a, a, a, a, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, a, a, a, a, a, a, a, a, a, a, 0, 0, 0, 0,
0, 0, 0, a, a, a, a, a, a, a, a, a, a, a, a, 0, 0, 0,
0, 0, a, a, a, a, a, a, a, a, a, a, a, a, a, a, 0, 0,
0, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, 0,
0, a, a, a, a, b, b, a, a, a, a, b, b, a, a, a, a, 0,
a, a, a, a, a, b, c, a, a, a, a, c, b, a, a, a, a, a,
a, a, a, a, a, b, c, a, a, a, a, c, b, a, a, a, a, a,
a, a, a, a, a, b, b, a, a, a, a, b, b, a, a, a, a, a,
a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a,
a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a,
a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a,
0, a, a, a, b, b, a, a, a, a, a, a, b, b, a, a, a, 0,
0, a, a, a, a, b, b, b, a, a, b, b, b, a, a, a, a, 0,
0, 0, a, a, a, a, a, a, b, b, a, a, a, a, a, a, 0, 0,
0, 0, 0, a, a, a, a, a, a, a, a, a, a, a, a, 0, 0, 0,
0, 0, 0, 0, a, a, a, a, a, a, a, a, a, a, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, a, a, a, a, a, a, 0, 0, 0, 0, 0, 0
})
c1 = { x = 0, y = 0, dx = 4, dy = 4 }
c2 = { x = 260, y = 0, dx = -3, dy = 3 }
c3 = { x = 0, y = 202, dx = 5, dy = -5 }

function proceed(c)
  c.x = c.x + c.dx
  c.y = c.y + c.dy
  if c.x < 0 then
    c.x = -c.x
    c.dx = -c.dx
  end
  if c.y < 0 then
    c.y = -c.y
    c.dy = -c.dy
  end
  if c.x > width - 18 then
    c.dx = -c.dx
    c.x = c.x + c.dx * 2
  end
  if c.y > height - 18 then
    c.dy = -c.dy
    c.y = c.y + c.dy * 2
  end
end


patdraw(1, c1.x, c1.y)
patdraw(1, c2.x, c2.y)
patdraw(1, c3.x, c3.y)
while true do
  wait(20)
  paterase(1, c1.x, c1.y)
  paterase(1, c2.x, c2.y)
  paterase(1, c3.x, c3.y)
  proceed(c1)
  proceed(c2)
  proceed(c3)
  patdraw(1, c1.x, c1.y)
  patdraw(1, c2.x, c2.y)
  patdraw(1, c3.x, c3.y)
  if inkey(1) ~= 0 then break end
end
