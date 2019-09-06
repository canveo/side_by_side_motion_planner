set term X11
set size square
set size ratio -1

plot 'map_obs.txt' u 6:8 w d ,\
     'map.txt' u 6:8 w d
pause -1

set term png
set output "map.png"
replot
     
pause -1

