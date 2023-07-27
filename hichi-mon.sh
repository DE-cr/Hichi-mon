#!/usr/bin/sh

# Hichi power monitor (https://github.com/DE-cr/Hichi-mon)
# using esp32 with (optional) ssd1306 display
# (file hichi-mon.sh)

day="${1:-$(date +%F)}"
geometry=$(xrandr | grep '*') && geometry=${geometry%  *}

cat ~/Dropbox/Hichi-mon/log/${day}*.csv |
perl -lne '
  next unless /^(\d\d):(\d\d):(\d\d) (-?\d+)$/;
  ($s, $w) = (($1*60+$2)*60+$3, $4);
  $wr = $w>0 ? $w : 0;
  if (defined $ps) {
    $kwh  += ($w +$pw )/2/1000 * ($s-$ps)/60/60;
    $kwhr += ($wr+$pwr)/2/1000 * ($s-$ps)/60/60;
  } else { $kwh = $kwhr = 0; }
  print "$_ $kwh $kwhr ", $kwhr-$kwh;
  ($ps, $pw, $pwr) = ($s, $w, $wr);
  ' |
feedgnuplot \
  --lines \
  --domain \
  --timefmt %H:%M:%S \
  --set 'format x "%H:%M"' \
  --title "Date: $day" \
  --xlabel 'Time [hour]' \
  --ylabel 'Power [W]' \
  --y2label 'Energy [kWh]' \
  --y2 1,2,3 \
  --legend 0 'Power (net demand)' \
  --legend 1 'Energy (net demand)' \
  --legend 2 'Energy (paid for)' \
  --legend 3 'Energy (PV excess)' \
  --set 'key top left' \
  --terminal x11 \
  --geometry $geometry
# --terminal 'png size 1920,1080' > "$day.png"
