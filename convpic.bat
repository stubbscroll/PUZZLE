move aka*.bmp pic
move has*.bmp pic
move hey*.bmp pic
move hit*.bmp pic
move kur*.bmp pic
move mas*.bmp pic
move min*.bmp pic
move nur*.bmp pic
move pic*.bmp pic
move sli*.bmp pic
move yaj*.bmp pic
cd pic
for %%s in (*.bmp) do convert %%s %%~ns.png
del *.bmp
cd ..
