--  SameGame for LuaCon
--  2016.7.16. Toshi Nagata
--  UTF-8 encoding
s = con -- 省略形を定義する
width, height = s.screensize()
xx = width // 32       --  盤面の幅
yy = height // 32 - 1  --  盤面の高さ
n = 5     --  駒の種類
num = 0   --  残り駒の数
point = 0 --  ポイント
cx = 0    --  現在のカーソル位置 (x)
cy = 0    --  現在のカーソル位置 (y)

board = {} -- 盤面: (x,y) の値 = board[x * xx + y + 1]

function disppiece(x, y)
  local c, col, ss, bcol
  c = board[x * xx + y + 1]
  if c == 1 then
    col = 1; ss = "♦♦"
  elseif c == 2 then
    col = 2; ss = "♥♥"
  elseif c == 3 then
    col = 3; ss = "♣♣"
  elseif c == 4 then
    col = 4; ss = "♠♠"
  elseif c == 5 then
    col = 5; ss = "＊＊"
  else
    col = 7; ss = "　　"
  end
  if x == cx and y == cy then
    bcol = 8
  else
    bcol = 0
  end
  s.color(col, bcol)
  s.locate(x * 4, y * 2 + 1)
  s.puts(ss)
  s.locate(x * 4, y * 2 + 2)
  s.puts(ss)
end

function dispinfo()
  s.color(7, 0)
  s.locate(0, 0); s.puts("【さめがめ for るあこん】 ")
  s.puts("残 " .. num .. " ")
  s.puts("点 " .. point)
  s.clearline()
end

function display()
  local x, y
  for x = 0, xx - 1 do
    for y = 0, yy - 1 do
      disppiece(x, y)
    end
  end
  dispinfo()
end

function mark(x, y)
  local c, n
  c = board[x * xx + y + 1]
  board[x * xx + y + 1] = c + 100
  n = 1
  if x > 0 and board[(x - 1) * xx + y + 1] == c then n = n + mark(x - 1, y) end
  if x < xx - 1 and board[(x + 1) * xx + y + 1] == c then n = n + mark(x + 1, y) end
  if y > 0 and board[x * xx + y] == c then n = n + mark(x, y - 1) end
  if y < yy - 1 and board[x * xx + y + 2] == c then n = n + mark(x, y + 1) end
  return n
end

for x = 0, xx - 1 do
  for y = 0, yy - 1 do
    board[x * xx + y + 1] = math.random(n)
  end
end
num = xx * yy
s.cls()
display()
while true do
::redo::
  ch = s.inkey()
  if ch == 27 or ch == 113 or ch == 81 then break end
  wx = cx
  wy = cy
  if ch == 28 then
    cx = (cx + 1) % xx
  elseif ch == 29 then
    cx = (cx - 1) % xx
  elseif ch == 30 then
    cy = (cy - 1) % yy
  elseif ch == 31 then
    cy = (cy + 1) % yy
  elseif ch == 13 then
    n = mark(cx, cy)
    if n == 1 then
      board[cx * xx + cy + 1] = board[cx * xx + cy + 1] - 100
    else
      for x = 0, xx - 1 do
        for y = 0, yy - 1 do
          if board[x * xx + y + 1] > 100 then disppiece(x, y) end
        end
      end
      for x = 0, xx - 1 do
        y1 = yy - 1
        for y = yy - 1, 0, -1 do
          if board[x * xx + y + 1] < 100 then
            board[x * xx + y1 + 1] = board[x * xx + y + 1]
            y1 = y1 - 1
          end
        end
        while y1 >= 0 do
          board[x * xx + y1 + 1] = 0
          y1 = y1 - 1
        end
      end
      x1 = 0
      for x = 0, xx - 1 do
        if board[x * xx + yy] == 0 then goto next end
        if x ~= x1 then
          for y = 0, yy - 1 do
            board[x1 * xx + y + 1] = board[x * xx + y + 1]
          end
        end
        x1 = x1 + 1
        ::next::
      end
      for x1 = x1, xx - 1 do
        for y = 0, yy - 1 do
          board[x1 * xx + y + 1] = 0
        end
      end
      point = point + (n - 2) * (n - 2)
      num = num - n
      display()
    end
    goto redo
  end
  if wx ~= cx or wy ~= cy then
    disppiece(wx, wy)
    disppiece(cx, cy)
  end
end
s.locate(0, yy * 2 + 1)
s.color(7, 0)

