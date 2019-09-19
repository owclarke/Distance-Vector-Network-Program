rm "routing-outputA.txt"
rm "routing-outputB.txt"
rm "routing-outputC.txt"
rm "routing-outputE.txt"
rm "routing-outputF.txt"

make
xterm -title "Router A 10000" -hold -e "./my-router A" &
xterm -title "Router B 10001" -hold -e "./my-router B" &
xterm -title "Router C 10002" -hold -e "./my-router C" &
xterm -title "Router D 10003" -hold -e "./my-router D" &
xterm -title "Router E 10004" -hold -e "./my-router E" &
xterm -title "Router F 10005" -hold -e "./my-router F" &

