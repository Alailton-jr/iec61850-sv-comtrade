sudo tcpdump -i eth0 -l | \
while read -r line; do
    ((count++))
done &

while true; do
    echo "SV packets: $count"
    sleep 1
done
